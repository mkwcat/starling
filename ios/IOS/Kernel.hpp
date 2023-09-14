// Kernel.hpp - IOS kernel patching
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <System/Types.h>

class Kernel
{
public:
    /**
     * Patch IOS_Open for device redirection.
     * This can only be called in system mode.
     */
    static void PatchIOSOpen();

    /**
     * Import the korean common key. TODO: This will not be needed if DI is ever
     * fully emulated.
     * This can only be called in system mode.
     */
    static void ImportKoreanCommonKey();

    /**
     * Check if the current device is Wii U hardware.
     */
    static bool IsWiiU();

    /**
     * Reboot the Espresso (Wii U PPC) into Wii U mode. This will allow enabling
     * the extra PPC cores.
     */
    static bool ResetEspresso(u32 entry);
};
