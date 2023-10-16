#include "IPC.h"
#include <CPUCache.hpp>
#include <Console.hpp>
#include <HWReg/ACR.hpp>
#include <cstring>

bool g_useRvlIpc = false;

template <typename T>
uintptr_t VirtualToPhysical(T* ptr)
{
    return reinterpret_cast<uintptr_t>(ptr) & 0x7fffffff;
}

template <typename T>
T* PhysicalToVirtual(uintptr_t addr)
{
    return reinterpret_cast<T*>(addr | 0x80000000);
}

alignas(0x20) static IOSRequest s_request;

extern "C" {
// These are defined in channel/Import_RVL_OS.h
s32 RVL_IOS_Open(const char* path, u32 mode);
s32 RVL_IOS_Close(s32 fd);
s32 RVL_IOS_Read(s32 fd, void* data, u32 len);
s32 RVL_IOS_Write(s32 fd, const void* data, u32 len);
s32 RVL_IOS_Seek(s32 fd, u32 where, u32 whence);
s32 RVL_IOS_Ioctl(
    s32 fd, u32 ioctl, const void* in, u32 inLen, void* out, u32 outLen
);
s32 RVL_IOS_Ioctlv(s32 fd, u32 ioctl, u32 inCount, u32 outCount, IOVector* vec);
}

static void Sync()
{
    CPUCache::DCFlush(s_request);

    HWRegWrite<ACR::IPC_PPCMSG>(VirtualToPhysical(&s_request));
    HWRegSetFlag(ACR::IPC_PPCCTRL::X1);

    while (!HWRegReadFlag(ACR::IPC_PPCCTRL::Y2)) {
        if (HWRegReadFlag(ACR::IPC_PPCCTRL::Y1)) {
            // Expected an ack but got a reply!
            HWRegSetFlag(ACR::IPC_PPCCTRL::Y1);
            HWRegSetFlag(ACR::IPC_PPCCTRL::X2);
        }
    }

    HWRegSetFlag(ACR::IPC_PPCCTRL::Y2);

    u32 reply;
    do {
        while (!HWRegReadFlag(ACR::IPC_PPCCTRL::Y1)) {
            if (HWRegReadFlag(ACR::IPC_PPCCTRL::Y2)) {
                // Expected a reply but got an ack!
                HWRegSetFlag(ACR::IPC_PPCCTRL::Y2);
            }
        }
        reply = HWRegRead<ACR::IPC_ARMMSG>();
        HWRegSetFlag(ACR::IPC_PPCCTRL::Y1);
        HWRegSetFlag(ACR::IPC_PPCCTRL::X2);
    } while (reply != VirtualToPhysical(&s_request));

    CPUCache::DCInvalidate(s_request);
}

s32 IOS_Open(const char* path, u32 mode)
{
    if (g_useRvlIpc) {
        return RVL_IOS_Open(path, mode);
    }

    char pathFixed[64] alignas(32) = {};
    strncpy(pathFixed, path, 63);

    CPUCache::DCFlush(pathFixed, sizeof(pathFixed));

    s_request = {};
    s_request.cmd = IOS_CMD_OPEN;
    s_request.open.path = VirtualToPhysical(pathFixed);
    s_request.open.mode = mode;

    Sync();

    return s_request.result;
}

s32 IOS_Close(s32 fd)
{
    if (g_useRvlIpc) {
        return RVL_IOS_Close(fd);
    }

    s_request = {};
    s_request.cmd = IOS_CMD_CLOSE;
    s_request.fd = fd;

    Sync();

    return s_request.result;
}

s32 IOS_Seek(s32 fd, s32 offset, s32 origin)
{
    if (g_useRvlIpc) {
        return RVL_IOS_Seek(fd, offset, origin);
    }

    s_request = {};
    s_request.cmd = IOS_CMD_SEEK;
    s_request.fd = fd;
    s_request.seek.offset = offset;
    s_request.seek.origin = origin;

    Sync();

    return s_request.result;
}

s32 IOS_Read(s32 fd, void* data, s32 size)
{
    if (g_useRvlIpc) {
        return RVL_IOS_Read(fd, data, size);
    }

    s_request = {};
    s_request.cmd = IOS_CMD_READ;
    s_request.fd = fd;
    s_request.read.data = VirtualToPhysical(data);
    s_request.read.size = size;

    Sync();

    CPUCache::DCInvalidate(data, size);
    return s_request.result;
}

s32 IOS_Write(s32 fd, const void* data, s32 size)
{
    if (g_useRvlIpc) {
        return RVL_IOS_Write(fd, data, size);
    }

    s_request = {};
    s_request.cmd = IOS_CMD_WRITE;
    s_request.fd = fd;
    s_request.write.data = VirtualToPhysical(data);
    s_request.write.size = size;

    CPUCache::DCFlush(data, size);

    Sync();

    return s_request.result;
}

s32 IOS_Ioctl(
    s32 fd, u32 command, const void* in, u32 in_size, void* out, u32 out_size
)
{
    if (g_useRvlIpc) {
        return RVL_IOS_Ioctl(fd, command, in, in_size, out, out_size);
    }

    CPUCache::DCFlush(in, in_size);
    CPUCache::DCFlush(out, out_size);

    s_request = {};
    s_request.cmd = IOS_CMD_IOCTL;
    s_request.fd = fd;
    s_request.ioctl.cmd = command;
    s_request.ioctl.in = VirtualToPhysical(in);
    s_request.ioctl.in_size = in_size;
    s_request.ioctl.out = VirtualToPhysical(out);
    s_request.ioctl.out_size = out_size;

    Sync();

    CPUCache::DCInvalidate(out, out_size);

    return s_request.result;
}

s32 IOS_Ioctlv(s32 fd, u32 command, u32 in_count, u32 out_count, IOVector* vec)
{
    if (g_useRvlIpc) {
        return RVL_IOS_Ioctlv(fd, command, in_count, out_count, vec);
    }

    for (u32 i = 0; i < in_count + out_count; i++) {
        if (vec[i].data && vec[i].size != 0) {
            CPUCache::DCFlush(vec[i].data, vec[i].size);
            vec[i].data =
                reinterpret_cast<void*>(VirtualToPhysical(vec[i].data));
        }
    }
    CPUCache::DCFlush(vec, (in_count + out_count) * sizeof(IOVector));

    s_request = {};
    s_request.cmd = IOS_CMD_IOCTLV;
    s_request.fd = fd;
    s_request.ioctlv.cmd = command;
    s_request.ioctlv.in_count = in_count;
    s_request.ioctlv.out_count = out_count;
    s_request.ioctlv.vec = VirtualToPhysical(vec);

    Sync();

    for (u32 i = in_count; i < in_count + out_count; i++) {
        if (vec[i].data && vec[i].size != 0) {
            vec[i].data =
                PhysicalToVirtual<void*>(reinterpret_cast<u32>(vec[i].data));
            CPUCache::DCInvalidate(vec[i].data, vec[i].size);
        }
    }

    return s_request.result;
}
