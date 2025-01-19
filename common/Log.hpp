// Log.hpp - Debug log
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <cstdarg>

namespace Log
{

enum class LogSource {
    System,
    LibCPP,
    DVD,
    BS2,
    Patcher,
    Riivo,
    IOS,
    IOS_Loader,
    IOS_DevMgr,
    IOS_SDCard,
    IOS_USB,
    IOS_EmuFS,
    IOS_EmuDI,
    IOS_EmuES,
};

enum class LogLevel {
    INFO = 'I',
    WARN = 'W',
    NOTICE = 'N',
    ERROR = 'E',
};

extern bool g_viLogEnabled;

extern bool g_useMutex;

bool IsEnabled();

void VPrint(
    LogSource src, const char* srcStr, const char* funcStr, LogLevel level,
    const char* format, va_list args
);
void Print(
    LogSource src, const char* srcStr, const char* funcStr, LogLevel level,
    const char* format, ...
);

#define STR(f) #f

#define PRINT(CHANNEL, LEVEL, ...)                                             \
    Log::Print(                                                                \
        Log::LogSource::CHANNEL, #CHANNEL, __FUNCTION__, Log::LogLevel::LEVEL, \
        __VA_ARGS__                                                            \
    )

} // namespace Log
