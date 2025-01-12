// System.cpp - Starling IOS system
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "System.hpp"
#include <AES.hpp>
#include <Config.hpp>
#include <Console.hpp>
#include <DI.hpp>
#include <DeviceEmuDI.hpp>
#include <DeviceEmuES.hpp>
#include <DeviceEmuFS.hpp>
#include <DeviceStarling.hpp>
#include <DiskManager.hpp>
#include <ES.hpp>
#include <FAT.h>
#include <HWReg/ACR.hpp>
#include <Kernel.hpp>
#include <Log.hpp>
#include <OS.hpp>
#include <SDCard.hpp>
#include <SHA.hpp>
#include <Syscalls.h>
#include <Types.h>
#include <Util.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

DeviceStarling* System::s_deviceStarling;

/**
 * Abort the IOS module.
 */
void System::Abort()
{
    char report[256];
    snprintf(report, sizeof(report), "Thread: %d\n", IOS_GetThreadId());

    Console::Print("E[IOS Abort] Abort was called!\n");
    Console::Print(report);
    // TODO: Application exit
    IOS_CancelThread(0, 0);
    while (true) {
    }
}

void abort()
{
    System::Abort();
    __builtin_unreachable();
}

void __assert_func(
    const char* file, int line, const char* func, const char* expr
)
{
    char report[256];
    snprintf(
        report, sizeof(report),
        "Expression: %s\n"
        "File: %s\n"
        "Line: %d\n"
        "Function: %s\n"
        "LR: %08X\n",
        expr, file, line, func,
        reinterpret_cast<u32>(__builtin_return_address(0))
    );

    Console::Print("E[IOS AssertFail] Assertion failed:\n");
    Console::Print(report);
    // TODO: Application exit
    IOS_CancelThread(0, 0);
    while (true) {
    }
}

bool s_timerStarted = false;
u8 s_timerIndex = 0;
u64 s_baseEpoch = 0;

struct {
    u32 m_timer = 0;
    u64 m_tick = 0;
} s_timerCtx[2];

static u64 DiffTicks(u64 tick0, u64 tick1)
{
    return tick1 < tick0 ? u64(-1) - tick0 + tick1 : tick1 - tick0;
}

static s32 TimerThreadEntry([[maybe_unused]] void* arg)
{
    // 32 minute interval
    static constexpr u32 TimerInterval = 1000 * (60 * 32);

    while (true) {
        System::SleepUsec(TimerInterval);

        u8 prev = s_timerIndex;
        u8 next = prev ^ 1;

        u32 prevTimer = s_timerCtx[prev].m_timer;
        u32 nextTimer = HWRegRead<ACR::TIMER>();

        s_timerCtx[next].m_tick =
            s_timerCtx[prev].m_tick + DiffTicks(prevTimer, nextTimer);
        s_timerCtx[next].m_timer = nextTimer;
        s_timerIndex = next;
    }
}

/**
 * Initialize the internal clock used for file times.
 */
void System::SetTime(u32 hwTimerVal, u64 epoch)
{
    s_timerCtx[s_timerIndex].m_timer = hwTimerVal;
    s_timerCtx[s_timerIndex].m_tick = 0;
    s_baseEpoch = epoch;

    if (!s_timerStarted) {
        s_timerStarted = true;
        new Thread(TimerThreadEntry, nullptr, nullptr, 0x400, 1);
    }
}

/**
 * Get the internal clock value.
 */
u64 System::GetTime()
{
    u8 i = s_timerIndex;
    u64 timeNow = s_timerCtx[i].m_tick +
                  DiffTicks(s_timerCtx[i].m_timer, HWRegRead<ACR::TIMER>());

    return s_baseEpoch + (timeNow / 1898614);
}

/**
 * Sleep for the specified amount of microseconds.
 */
void System::SleepUsec(u32 usec)
{
    if (usec == 0)
        return;

    u32 queueData;
    const s32 queue = IOS_CreateMessageQueue(&queueData, 1);
    assert(queue >= 0);

    const s32 timer = IOS_CreateTimer(usec, 0, queue, 1);
    assert(timer >= 0);

    u32 msg;
    s32 ret = IOS_ReceiveMessage(queue, &msg, 0);
    assert(ret == IOS::IOSError::OK && msg == 1);

    ret = IOS_DestroyTimer(timer);
    assert(ret == IOS::IOSError::OK);

    ret = IOS_DestroyMessageQueue(queue);
    assert(ret == IOS::IOSError::OK);
}

void usleep(unsigned int usec)
{
    System::SleepUsec(usec);
}

/**
 * Write a 32-bit value to kernel-only memory.
 */
void System::PrivilegedWrite(u32 address, u32 value)
{
    const s32 queue =
        IOS_CreateMessageQueue(reinterpret_cast<u32*>(address), 0x40000000);
    assert(queue >= 0);

    s32 ret = IOS_SendMessage(queue, value, 0);
    assert(ret == IOS::IOSError::OK);

    ret = IOS_DestroyMessageQueue(queue);
    assert(ret == IOS::IOSError::OK);
}

/**
 * System thread entry point. This thread runs under system mode.
 */
s32 System::ThreadEntry([[maybe_unused]] void* arg)
{
    SHA::s_instance = new SHA();
    AES::s_instance = new AES();
    ES::s_instance = new ES();

    Kernel::ImportKoreanCommonKey();

    DiskManager::s_instance = new DiskManager();

    DeviceEmuDI::Init();
    DeviceEmuFS::Init();

    Kernel::PatchIOSOpen();

    new Thread(DeviceEmuES::ThreadEntry, nullptr, nullptr, 0x2000, 80);

    // Kernel::PatchIOSOpen();

    System::s_deviceStarling->Run();

    return IOS::IOSError::OK;
}

s32 System::s_heapId = -1;

// TODO: The size could be determined automatically
constexpr u32 SYSTEM_HEAP_SIZE = 0x40000; // 256 KB
static u8 s_systemHeapData[SYSTEM_HEAP_SIZE] alignas(32);

static u8 s_systemThreadStack[0x1000] alignas(32);

/**
 * IOS module entry point.
 */
void System::Entry()
{
    Console::Reinit();
    Console::Print("I[IOS_Loader] Launching IOS Module... OK\n");
    PRINT(IOS, INFO, "> Enter IOS Module");

    IOS_SetThreadPriority(0, 40);

    // Create system heap
    {
        s32 ret = IOS_CreateHeap(s_systemHeapData, sizeof(s_systemHeapData));
        assert(ret >= 0);
        System::s_heapId = ret;
    }

    // Enable log mutex as we can now allocate memory for it
    Log::g_useMutex = true;

    Config::s_instance = new Config();
    System::s_deviceStarling = new DeviceStarling();

    // Run C++ static constructors
    {
        typedef void (*CtorFunc)(void);
        extern CtorFunc _init_array_start[], _init_array_end[];

        for (CtorFunc* ctor = _init_array_start; ctor != _init_array_end;
             ctor++) {
            (*ctor)();
        }
    }

    // Create system mode thread
    {
        s32 ret = IOS_CreateThread(
            System::ThreadEntry, nullptr,
            reinterpret_cast<u32*>(
                s_systemThreadStack + sizeof(s_systemThreadStack)
            ),
            sizeof(s_systemThreadStack), 80, true
        );
        assert(ret >= 0);

        // Set new thread CPSR with system mode enabled
        u32 cpsr = 0x1F | ((u32) (&System::ThreadEntry) & 1 ? 0x20 : 0);
        System::PrivilegedWrite(0xFFFE0000 + ret * 0xB0, cpsr);

        ret = IOS_StartThread(ret);
        assert(ret >= 0);
    }
}

extern "C" s32 Entry([[maybe_unused]] void* arg)
{
    System::Entry();
    return IOS::IOSError::OK;
}
