// Log.cpp - Debug log
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "Log.hpp"
#include <Debug/Console.hpp>
#include <System/OS.hpp>
#include <System/Types.h>
#ifdef TARGET_IOS
#  include <Disk/DiskManager.hpp>
#else
#  include <Import/RVL_OS.h>
#endif
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>

bool Log::g_viLogEnabled = true;

bool Log::g_useMutex = false;

static Mutex* s_logMutex;

static constexpr std::array<char, 4> s_logChars = {
    'I',
    'W',
    'N',
    'E',
};

bool Log::IsEnabled()
{
    if (g_viLogEnabled) {
        return true;
    }

#ifdef TARGET_IOS
    if (DiskManager::s_instance != nullptr &&
        DiskManager::s_instance->IsLogEnabled()) {
        return true;
    }
#endif

    return false;
}

void Log::VPrint(
    [[maybe_unused]] LogSource src, [[maybe_unused]] const char* srcStr,
    [[maybe_unused]] const char* funcStr, [[maybe_unused]] LogLevel level,
    const char* format, va_list args
)
{
    if (!IsEnabled()) {
        return;
    }

    u32 slvl = static_cast<u32>(level);
    assert(slvl < s_logChars.size());

    if (g_useMutex) {
        if (s_logMutex == nullptr) {
            s_logMutex = new Mutex;
        }
        s_logMutex->Lock();
    }

    static std::array<char, 256> s_logBuffer;
    u32 len = vsnprintf(&s_logBuffer[0], s_logBuffer.size(), format, args);
    if (len >= s_logBuffer.size()) {
        len = s_logBuffer.size() - 1;
        s_logBuffer[len] = 0;
    }

    // Remove newline at the end of log
    if (s_logBuffer[len - 1] == '\n')
        s_logBuffer[len - 1] = 0;

    static std::array<char, 256> s_printBuffer;

    len = snprintf(
        &s_printBuffer[0], s_printBuffer.size(), "%c[%s %s] %s\n",
        s_logChars[slvl], srcStr, funcStr, s_logBuffer.data()
    );

#ifdef TARGET_IOS
    if (DiskManager::s_instance != nullptr &&
        DiskManager::s_instance->IsLogEnabled() &&
        src != LogSource::IOS_SDCard) {
        DiskManager::s_instance->WriteToLog(&s_printBuffer[0], len);
    }
#endif

    if (g_viLogEnabled) {
        Console::Print(&s_printBuffer[0]);
    }

    if (g_useMutex) {
        s_logMutex->Unlock();
    }
}

void Log::Print(
    LogSource src, const char* srcStr, const char* funcStr, LogLevel level,
    const char* format, ...
)
{
    va_list args;
    va_start(args, format);
    VPrint(src, srcStr, funcStr, level, format, args);
    va_end(args);
}
