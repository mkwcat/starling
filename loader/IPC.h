#pragma once

#include <Types.h>
#include <Util.h>

EXTERN_C_START

extern bool g_useRvlIpc;

typedef struct {
    void* data;
    u32 size;
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
            uintptr_t path;
            u32 mode;
            u32 uid;
            u16 gid;
        } open;

        struct {
            uintptr_t data;
            u32 size;
        } read, write;

        struct {
            s32 offset;
            s32 origin;
        } seek;

        struct {
            u32 cmd;
            uintptr_t in;
            u32 in_size;
            uintptr_t out;
            u32 out_size;
        } ioctl;

        struct {
            u32 cmd;
            u32 in_count;
            u32 out_count;
            uintptr_t vec;
        } ioctlv;

        u32 args[5];
    };
} IOSRequest;

s32 IOS_Open(const char* path, u32 mode);

s32 IOS_Close(s32 fd);

s32 IOS_Seek(s32 fd, s32 offset, s32 origin);

s32 IOS_Read(s32 fd, void* data, s32 size);

s32 IOS_Write(s32 fd, const void* data, s32 size);

s32 IOS_Ioctl(
    s32 fd, u32 command, const void* in, u32 in_size, void* out, u32 out_size
);

s32 IOS_Ioctlv(s32 fd, u32 command, u32 in_count, u32 out_count, IOVector* vec);

EXTERN_C_END
