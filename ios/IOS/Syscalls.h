// Syscalls.h - IOS system call definitions
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <System/Types.h>
#include <System/Util.h>

EXTERN_C_START

/*
 * IOS Thread
 */
typedef s32 (*IOSThreadProc)(void* arg);

s32 IOS_CreateThread(
    IOSThreadProc proc, void* arg, u32* stack_top, u32 stacksize, s32 priority,
    bool detached
);
s32 IOS_JoinThread(s32 threadid, void** value);
s32 IOS_CancelThread(s32 threadid, void* value);
s32 IOS_GetThreadId();
s32 IOS_GetProcessId();
s32 IOS_StartThread(s32 threadid);
s32 IOS_SuspendThread(s32 threadid);
void IOS_YieldThread();
u32 IOS_GetThreadPriority(s32 threadid);
s32 IOS_SetThreadPriority(s32 threadid, u32 priority);

/*
 * IOS Message
 */
s32 IOS_CreateMessageQueue(u32* buf, u32 msg_count);
s32 IOS_DestroyMessageQueue(s32 queue_id);
s32 IOS_SendMessage(s32 queue_id, u32 message, u32 flags);
s32 IOS_JamMessage(s32 queue_id, u32 message, u32 flags);
s32 IOS_ReceiveMessage(s32 queue_id, u32* message, u32 flags);

/*
 * IOS Timer
 */
s32 IOS_CreateTimer(s32 usec, s32 repeat_usec, s32 queue, u32 msg);
s32 IOS_RestartTimer(s32 timer, s32 usec, s32 repeat_usec);
s32 IOS_StopTimer(s32 timer);
s32 IOS_DestroyTimer(s32 timer);
u32 IOS_GetTime();

/*
 * IOS Memory
 */
s32 IOS_CreateHeap(void* ptr, s32 length);
s32 IOS_DestroyHeap(s32 heap);
void* IOS_Alloc(s32 heap, u32 length);
void* IOS_AllocAligned(s32 heap, u32 length, u32 align);
s32 IOS_Free(s32 heap, void* ptr);

/*
 * IPC (Inter-process communication)
 */

typedef struct {
    void* data;
    u32 len;
} IOVector;

typedef struct {
    u32 cmd;
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
            void* data;
            u32 len;
        } read, write;

        struct {
            s32 where;
            s32 whence;
        } seek;

        struct {
            u32 cmd;
            void* in;
            u32 in_len;
            void* out;
            u32 out_len;
        } ioctl;

        struct {
            u32 cmd;
            u32 in_count;
            u32 out_count;
            IOVector* vec;
        } ioctlv;

        u32 args[5];
    };
} IOSRequest;

s32 IOS_Open(const char* path, u32 mode);
s32 IOS_OpenAsync(const char* path, u32 mode, s32 queue_id, IOSRequest* msg);
s32 IOS_Close(s32 fd);
s32 IOS_CloseAsync(s32 fd, s32 queue_id, IOSRequest* msg);

s32 IOS_Seek(s32 fd, s32 where, s32 whence);
s32 IOS_SeekAsync(s32 fd, s32 where, s32 whence, s32 queue_id, IOSRequest* msg);
s32 IOS_Read(s32 fd, void* buf, s32 len);
s32 IOS_ReadAsync(s32 fd, void* buf, s32 len, s32 queue_id, IOSRequest* msg);
s32 IOS_Write(s32 fd, const void* buf, s32 len);
s32 IOS_WriteAsync(
    s32 fd, const void* buf, s32 len, s32 queue_id, IOSRequest* msg
);

s32 IOS_Ioctl(
    s32 fd, u32 command, const void* in, u32 in_len, void* out, u32 out_len
);
s32 IOS_IoctlAsync(
    s32 fd, u32 command, const void* in, u32 in_len, void* out, u32 io_len,
    s32 queue_id, IOSRequest* msg
);

s32 IOS_Ioctlv(s32 fd, u32 command, u32 in_count, u32 out_count, IOVector* vec);
s32 IOS_IoctlvAsync(
    s32 fd, u32 command, u32 in_count, u32 out_count, IOVector* vec,
    s32 queue_id, IOSRequest* msg
);

s32 IOS_RegisterResourceManager(const char* device, s32 queue_id);
s32 IOS_ResourceReply(const IOSRequest* request, s32 reply);

/*
 * ARM Memory and Cache
 */
void IOS_InvalidateDCache(void* address, u32 size);
void IOS_FlushDCache(const void* address, u32 size);
void* IOS_VirtualToPhysical(void* virt);

/*
 * Misc IOS
 */
s32 IOS_SetPPCACRPerms(u8 enable);
s32 IOS_SetIpcAccessRights(u8* rights);
s32 IOS_SetUid(u32 pid, u32 uid);
u32 IOS_GetUid();
s32 IOS_SetGid(u32 pid, u16 gid);
u16 IOS_GetGid();
s32 IOS_LaunchElf(const char* path);
s32 IOS_LaunchRM(const char* path);

EXTERN_C_END
