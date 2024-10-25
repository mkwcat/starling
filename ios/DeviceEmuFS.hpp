// DeviceEmuFS.hpp - Emulated IOS filesystem RM
//   Written by Star
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <Types.h>

namespace DeviceEmuFS
{

void Init();

/**
 * Checks if a path is redirected somewhere else by the frontend.
 */
bool IsPathReplaced(const char* isfsPath);

} // namespace DeviceEmuFS
