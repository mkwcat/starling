// IOS.hpp - Interface for IOS commands
//   Written by mkwcat
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <ISFSTypes.hpp>
#include <Types.h>
#include <Util.h>
#include <cassert>
#include <cstring>

#ifdef TARGET_IOS
#  include <OS.hpp>
#  include <Syscalls.h>
#else
#  include <IPC.h>
#endif

namespace IOS
{

/**
 * IOS errors converted to C++ namespace form.
 * Original values found in Types.h
 */
namespace IOSError
{
enum E {
    OK = IOS_ERROR_OK,
    NO_ACCESS = IOS_ERROR_NO_ACCESS,
    EXISTS = IOS_ERROR_EXISTS,
    INVALID = IOS_ERROR_INVALID,
    MAX_OPEN = IOS_ERROR_MAX_OPEN,
    NOT_FOUND = IOS_ERROR_NOT_FOUND,
    QUEUE_FULL = IOS_ERROR_QUEUE_FULL,
    IO = IOS_ERROR_IO,
    NO_MEMORY = IOS_ERROR_NO_MEMORY,
};
} // namespace IOSError

/**
 * IPC command types converted to C++ namespace form.
 * Original values found in Types.h
 */
enum class Cmd : s32 {
    OPEN = IOS_CMD_OPEN,
    CLOSE = IOS_CMD_CLOSE,
    READ = IOS_CMD_READ,
    WRITE = IOS_CMD_WRITE,
    SEEK = IOS_CMD_SEEK,
    IOCTL = IOS_CMD_IOCTL,
    IOCTLV = IOS_CMD_IOCTLV,
    REPLY = IOS_CMD_REPLY,
};

/**
 * IPC open modes converted to C++ namespace form.
 * Original values found in Types.h
 */
namespace Mode
{
enum E {
    NONE = IOS_MODE_NONE,
    READ = IOS_MODE_READ,
    WRITE = IOS_MODE_WRITE,
    READ_WRITE = IOS_MODE_READ_WRITE,
};
} // namespace Mode

typedef s32 (*IPCCallback)(s32 result, void* userdata);

template <u32 TInCount, u32 TOutCount>
struct IOVector {
    ::IOVector* GetData()
    {
        return reinterpret_cast<::IOVector*>(this);
    }

    struct {
        const void* data;
        u32 len;
    } in[TInCount];

    struct {
        void* data;
        u32 len;
    } out[TOutCount];
};

template <u32 TInCount>
struct IVector {
    ::IOVector* GetData()
    {
        return reinterpret_cast<::IOVector*>(this);
    }

    struct {
        const void* data;
        u32 len;
    } in[TInCount];
};

template <u32 TOutCount>
struct OVector {
    ::IOVector* GetData()
    {
        return reinterpret_cast<::IOVector*>(this);
    }

    struct {
        void* data;
        u32 len;
    } out[TOutCount];
};

typedef ::IOVector TVector;

/**
 * Allocate memory for IPC. Always 32-bit aligned.
 */
static inline void* Alloc(u32 size);

/**
 * Free memory allocated using IOS::Alloc.
 */
static inline void Free(void* ptr);

#ifdef TARGET_IOS

constexpr s32 ipcHeap = 0;

static inline void* Alloc(u32 size)
{
    void* ptr = IOS_AllocAligned(ipcHeap, AlignUp(size, 32), 32);
    assert(ptr);
    return ptr;
}

static inline void Free(void* ptr)
{
    s32 ret = IOS_Free(ipcHeap, ptr);
    assert(ret == IOS::IOSError::OK);
}

#endif

struct Request {
    Cmd cmd;
    s32 result;

    union {
        s32 fd;
        s32 handle;
    };

    union {
        struct {
            char* path;
            u32 mode;
            u32 uid;
            u16 gid;
        } open;

        struct {
            u8* data;
            u32 len;
        } read, write;

        struct {
            s32 where;
            s32 whence;
        } seek;

        struct {
            u32 cmd;
            u8* in;
            u32 in_len;
            u8* out;
            u32 out_len;
        } ioctl;

        struct {
            u32 cmd;
            u32 in_count;
            u32 out_count;
            ::IOVector* vec;
        } ioctlv;

        u32 args[5];
    };

#ifdef TARGET_IOS
    s32 Reply(s32 ret)
    {
        return IOS_ResourceReply(reinterpret_cast<IOSRequest*>(this), ret);
    }
#endif
};

class Resource
{
public:
    /**
     * Default constructor (fd initializes to -1).
     */
    Resource() = default;

    /**
     * Construct a Resource by providing an open file descriptor.
     */
    Resource(s32 fd)
    {
        this->m_fd = fd;
    }

    /**
     * Construct a Resource by providing a path to open.
     */
    explicit Resource(const char* path, u32 mode = 0)
    {
        this->m_fd = IOS_Open(path, mode);
    }

    /**
     * Copy constructor (deleted).
     */
    Resource(const Resource& from) = delete;

    /**
     * Move constructor.
     */
    Resource(Resource&& from)
    {
        this->m_fd = from.m_fd;
        from.m_fd = -1;
    }

    /**
     * Resource destructor. Automatically closes any open resource.
     */
    ~Resource()
    {
        if (this->m_fd >= 0) {
            Close();
        }
    }

    /**
     * Manually close the resource.
     */
    s32 Close()
    {
        const s32 ret = IOS_Close(this->m_fd);
        if (ret >= 0) {
            this->m_fd = -1;
        }
        return ret;
    }

    /**
     * Read data from the resource.
     */
    s32 Read(void* data, u32 length)
    {
        return IOS_Read(this->m_fd, data, length);
    }

    /**
     * Write data to the resource.
     */
    s32 Write(const void* data, u32 length)
    {
        return IOS_Write(this->m_fd, data, length);
    }

    /**
     * Seek the resource.
     */
    s32 Seek(s32 offset, s32 origin)
    {
        return IOS_Seek(this->m_fd, offset, origin);
    }

    s32 GetFd() const
    {
        return this->m_fd;
    }

    bool IsValid() const
    {
        return m_fd >= 0;
    }

protected:
    s32 m_fd = -1;
};

template <typename TIoctlEnum>
class ResourceCtrl : public Resource
{
public:
    using Resource::Resource;

    s32 Ioctl(
        TIoctlEnum cmd, const void* input, u32 inputLength, void* output,
        u32 outputLength
    )
    {
        return IOS_Ioctl(
            this->m_fd, static_cast<u32>(cmd), input, inputLength, output,
            outputLength
        );
    }

    s32 Ioctlv(TIoctlEnum cmd, u32 inCount, u32 outCount, ::IOVector* vec)
    {
        return IOS_Ioctlv(
            this->m_fd, static_cast<u32>(cmd), inCount, outCount, vec
        );
    }

    template <u32 TInCount, u32 TOutCount>
    s32 Ioctlv(TIoctlEnum cmd, IOVector<TInCount, TOutCount>& vec)
    {
        return Ioctlv(cmd, TInCount, TOutCount, vec.GetData());
    }

    template <u32 TInCount>
    s32 Ioctlv(TIoctlEnum cmd, IVector<TInCount>& vec)
    {
        return Ioctlv(cmd, TInCount, 0, vec.GetData());
    }

    template <u32 TOutCount>
    s32 Ioctlv(TIoctlEnum cmd, OVector<TOutCount>& vec)
    {
        return Ioctlv(cmd, 0, TOutCount, vec.GetData());
    }

#ifdef TARGET_IOS
    /*
     * Queue-based asynchronous IOCTL calls.
     */

    s32 IoctlAsync(
        TIoctlEnum cmd, void* input, u32 inputLen, void* output, u32 outputLen,
        Queue<IOS::Request*>* queue, IOS::Request* req
    )
    {
        return IOS_IoctlAsync(
            this->m_fd, static_cast<u32>(cmd), input, inputLen, output,
            outputLen, queue->GetID(), reinterpret_cast<IOSRequest*>(req)
        );
    }

    s32 IoctlvAsync(
        TIoctlEnum cmd, u32 inputCnt, u32 outputCnt, ::IOVector* vec,
        Queue<IOS::Request*>* queue, IOS::Request* req
    )
    {
        return IOS_IoctlvAsync(
            this->m_fd, static_cast<u32>(cmd), inputCnt, outputCnt, vec,
            queue->GetID(), reinterpret_cast<IOSRequest*>(req)
        );
    }

    template <u32 TInCount, u32 TOutCount>
    s32 IoctlvAsync(
        TIoctlEnum cmd, IOVector<TInCount, TOutCount>& vec,
        Queue<IOS::Request*>* queue, IOS::Request* req
    )
    {
        return IoctlvAsync(cmd, TInCount, TOutCount, vec.GetData(), queue, req);
    }

    template <u32 TInCount>
    s32 IoctlvAsync(
        TIoctlEnum cmd, IVector<TInCount>& vec, Queue<IOS::Request*>* queue,
        IOS::Request* req
    )
    {
        return IoctlvAsync(cmd, TInCount, 0, vec.GetData(), queue, req);
    }

    template <u32 TOutCount>
    s32 IoctlvAsync(
        TIoctlEnum cmd, OVector<TOutCount>& vec, Queue<IOS::Request*>* queue,
        IOS::Request* req
    )
    {
        return IoctlvAsync(cmd, 0, TOutCount, vec.GetData(), queue, req);
    }
#endif
};

class File : public ResourceCtrl<ISFS::ISFSIoctl>
{
public:
    struct Stats {
        u32 size;
        u32 pos;
    };

    using ResourceCtrl::ResourceCtrl;

    File(const char* path, u32 mode = 0)
    {
        if (std::strlen(path) < 64) {
            this->m_fd = IOS_Open(path, mode);
            return;
        }

        // Long path
        this->m_fd = IOS_Open("/dev/fs", IOS::Mode::NONE);
        if (this->m_fd < 0) {
            return;
        }

        IOVector<2, 0> vec;
        vec.in[0].data = path;
        vec.in[0].len = std::strlen(path) + 1;
        vec.in[1].data = &mode;
        vec.in[1].len = sizeof(u32);

        s32 ret = this->Ioctlv(ISFS::ISFSIoctl::EX_OPEN, vec);

        if (ret < 0) {
            this->Close();
            this->m_fd = ret;
            return;
        }
    }

    u32 Tell()
    {
        Stats stats;
        const s32 ret = this->GetStats(&stats);
        assert(ret == IOS::IOSError::OK);
        return stats.pos;
    }

    u32 GetSize()
    {
        Stats stats;
        const s32 ret = this->GetStats(&stats);
        assert(ret == IOS::IOSError::OK);
        return stats.size;
    }

    s32 GetStats(Stats* stats)
    {
        return this->Ioctl(
            ISFS::ISFSIoctl::GET_FILE_STATS, nullptr, 0, stats, sizeof(Stats)
        );
    }
};

} // namespace IOS
