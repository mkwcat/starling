// System.hpp - Saoirse IOS system
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0

#pragma once

#include <Types.h>
#include <Util.h>
#include <cstring>

class DeviceStarling;

class System
{
public:
    /**
     * Get the system heap.
     */
    static s32 GetHeap()
    {
        return s_heapId;
    }

    /**
     * Abort the IOS module.
     */
    static void Abort();

    /**
     * Initialize the internal clock used for file times.
     */
    static void SetTime(u32 hwTimerVal, u64 epoch);

    /**
     * Get the internal clock value.
     */
    static u64 GetTime();

    /**
     * Sleep for the specified amount of microseconds.
     */
    static void SleepUsec(u32 usec);

    /**
     * Write a 32-bit value to kernel-only memory.
     */
    static void PrivilegedWrite(u32 address, u32 value);

    /**
     * Memcpy with only word writes to work around a Wii hardware bug. This
     * function was ported over to the regular memcpy and is now redundant.
     */
    static void* UnalignedMemcpy(void* dest, const void* src, size_t len)
    {
        return std::memcpy(dest, src, len);
    }

    /**
     * Get the DeviceStarling instance.
     */
    static DeviceStarling* GetDeviceStarling()
    {
        return s_deviceStarling;
    }

private:
    /**
     * System thread entry point. This thread runs under system mode.
     */
    static s32 ThreadEntry(void* arg);

public:
    /**
     * IOS module entry point.
     */
    static void Entry();

private:
    static s32 s_heapId;

    static DeviceStarling* s_deviceStarling;
};
