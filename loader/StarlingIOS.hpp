#pragma once

#include <Types.h>
#include <cstring>

class StarlingIOS
{
public:
    /**
     * Check if running on Dolphin Emulator. This can be useful for debugging or
     * features that don't require Starling IOS.
     */
    static bool IsDolphin();

    /**
     * Flush the data cache range on PPC and invalidate the data cache range on
     * ARM.
     */
    static void SafeFlush(const void* start, u32 size);

    /**
     * Launch Starling IOS entry point on ARM. Returns success immediately if
     * running on Dolphin.
     */
    static bool BootstrapEntry();

    /**
     * Open the Starling IOS manager device (DeviceStarling). The result and
     * handle is managed by this interface.
     */
    static void RMOpen();

    /**
     * Handle commands from Starling IOS. This function blocks until a request
     * is received. Returns once Starling IOS is exhausted of commands.
     */
    static void RMHandleCommands();

    /**
     * Close the Starling IOS manager device.
     */
    static void RMClose();
};
