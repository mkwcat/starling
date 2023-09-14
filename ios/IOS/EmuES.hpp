// EmuFS.hpp - Proxy ES RM
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <System/ES.hpp>
#include <System/Types.h>

namespace EmuES
{

ES::ESError DIVerify(u64 titleID, const ES::Ticket* ticket);
s32 ThreadEntry(void* arg);

} // namespace EmuES
