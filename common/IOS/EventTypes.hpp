// EventTypes.hpp - IOS event types
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <System/Types.h>

constexpr const char EVENT_DEVICE_NAME[] = "/dev/starling";

enum class EventRMIoctl {
    RegisterEventHook,
    StartGameEvent,
    SetTime,
};

enum class EventRMReply {
    Close,
    DeviceUpdate,
};

#pragma pack(push, 4)

struct EventRMData {
    static constexpr u32 DEVICE_COUNT = 9;

    struct DeviceUpdate {
        u8 enabled[DEVICE_COUNT];
        u8 mounted[DEVICE_COUNT];
        u8 error[DEVICE_COUNT];
    };

    union {
        DeviceUpdate deviceUpdate;
    };
};

struct EventRMTime {
    u32 hwTimer;
    u64 epoch;
};

#pragma pack(pop)
