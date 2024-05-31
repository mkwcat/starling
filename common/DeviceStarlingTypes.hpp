// DeviceStarlingTypes.hpp - IOS event types
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <AddressMap.h>
#include <Types.h>

typedef u8 DiskID;

namespace DeviceStarlingTypes
{

constexpr const char RM_PATH[] = "/dev/starling";

enum class Command {
    // Sent from the loader to IOS
    RECEIVE_COMMAND,
    START_GAME,

    // Sent from IOS to the loader
    CLOSE_REPLY,
    SELECT_DISK,
    INSERT_RIIVOLUTION_XML,
    SET_TITLE_LIST,
    REMOVE_DISK,
    DONE,
};

using Ioctl = Command;

constexpr u32 MAX_DISK_COUNT = 9;

struct CommandData {
    static CommandData FromDiskID(DiskID diskId)
    {
        CommandData data;
        data.disk.diskId = diskId;
        return data;
    }

    union {
        struct {
            u32 diskId;
        } disk;
    };
};

} // namespace DeviceStarlingTypes
