// Types.h - Common Starling types
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

#ifdef TARGET_PPC

typedef float f32;
typedef double f64;

#endif

enum {
    IOS_ERROR_OK = 0,
    IOS_ERROR_NO_ACCESS = -1,
    IOS_ERROR_EXISTS = -2,
    IOS_ERROR_INVALID = -4,
    IOS_ERROR_MAX_OPEN = -5,
    IOS_ERROR_NOT_FOUND = -6,
    IOS_ERROR_QUEUE_FULL = -8,
    IOS_ERROR_IO = -12,
    IOS_ERROR_NO_MEMORY = -22,
};

enum {
    IOS_CMD_OPEN = 1,
    IOS_CMD_CLOSE = 2,
    IOS_CMD_READ = 3,
    IOS_CMD_WRITE = 4,
    IOS_CMD_SEEK = 5,
    IOS_CMD_IOCTL = 6,
    IOS_CMD_IOCTLV = 7,
    IOS_CMD_REPLY = 8,
};

enum {
    IOS_MODE_NONE = 0,
    IOS_MODE_READ = 1,
    IOS_MODE_WRITE = 2,
    IOS_MODE_READ_WRITE = (IOS_MODE_READ | IOS_MODE_WRITE),
};

enum {
    IOS_SEEK_SET = 0,
    IOS_SEEK_CUR = 1,
    IOS_SEEK_END = 2,
};

#ifdef __cplusplus
}
#endif
