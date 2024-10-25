// ISFSTypes.hpp - ISFS types
//   Written by Star
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <Types.h>

namespace ISFS
{

namespace ISFSError
{
enum {
    OK = 0,
    INVALID = -101,
    ACCESS_DENIED = -102,
    CORRUPT = -103,
    NOT_READY = -104,
    ALREADY_EXISTS = -105,
    NOT_FOUND = -106,
    MAX_HANDLES_OPEN = -109,
    MAX_PATH_DEPTH = -110,
    LOCKED = -111,
    UNKNOWN = -117,
};
} // namespace ISFSError

constexpr char SEPARATOR_CHAR = '/';

constexpr u32 MAX_PATH_DEPTH = 8;
constexpr u32 MAX_PATH_LENGTH = 64; // Including the NULL terminator
constexpr u32 MAX_NAME_LENGTH = 12;
constexpr u32 EMUFS_MAX_PATH_LENGTH = 2048;

constexpr s32 MAX_OPEN_COUNT = 15;

enum class ISFSIoctl {
    FORMAT = 0x1,
    GET_STATS = 0x2,
    CREATE_DIR = 0x3,
    READ_DIR = 0x4,
    SET_ATTR = 0x5,
    GET_ATTR = 0x6,
    DELETE = 0x7,
    RENAME = 0x8,
    CREATE_FILE = 0x9,
    GET_FILE_STATS = 0xB,
    GET_USAGE = 0xC,
    SHUTDOWN = 0xD,

    EX_OPEN = 0x1000,
    EX_DIR_OPEN = 0x1001,
    EX_DIR_NEXT = 0x1002,
};

struct RenameBlock {
    char pathOld[MAX_PATH_LENGTH];
    char pathNew[MAX_PATH_LENGTH];
};

struct AttrBlock {
    // UID, title specific
    u32 ownerId;
    // GID, the "maker", for example 01 (0x3031) in RMCE01.
    u16 groupId;
    char path[MAX_PATH_LENGTH];
    // Access flags (like IOS::Mode). If the caller's identifiers match UID or
    // GID, use those permissions. Otherwise use otherPerm.
    // Permissions for UID
    u8 ownerPerm;
    // Permissions for GID
    u8 groupPerm;
    // Permissions for any other process
    u8 otherPerm;
    u8 attributes;
    u8 pad[2];
};

struct Direct_Stats {
    enum {
        // Read only
        RDO = 0x01,
        // Hidden
        HID = 0x02,
        // System
        SYS = 0x04,
        // Directory
        DIR = 0x10,
        // Archive
        ARC = 0x20,
    };

    u64 dirOffset;
    u64 size;
    u8 attribute;
    char name[EMUFS_MAX_PATH_LENGTH];
};

} // namespace ISFS
