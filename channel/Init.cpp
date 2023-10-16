#include "Apploader.hpp"
#include "Heap.hpp"
#include "System.hpp"
#include <CPUCache.hpp>
#include <Console.hpp>
#include <IPC.h>
#include <Import.h>
#include <ImportInfo.hpp>
#include <Import_RVL_OS.h>
#include <LoMem.hpp>
#include <Log.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>

REPLACE(0x80006D60) //

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    Heap::Init();

    // Enable log mutex as we can now allocate memory for it
    Log::g_useMutex = true;

    PRINT(System, INFO, "> Entering Channel");

    Channel::System::Init();
    Channel::System::Run();
    Channel::System::Shutdown();

    return 0;
}

extern "C" void __eabi()
{
}

REPLACE(0x800746F8) //

void exit()
{
    Log::g_useMutex = false;

    PRINT(System, INFO, "> Exiting");

#if 0
    if (ReadU32(0x80001804) == 0x53545542 /* STUB */ &&
        ReadU32(0x80001808) == 0x48415858 /* HAXX */) {
        OSDisableInterrupts();

        PRINT(System, INFO, "> meow");

        for (int i = 0; i < 32; i++) {
            IOS_Close(i);
        }

        PRINT(System, INFO, "> closed");

        // Correct IOS version for the stub
        os1->ios_number = 58;
        DCache::Store(&os1->ios_number, sizeof(os1->ios_number));

        ((void (*)(void)) 0x80001800)();
        while (true) {
        }
    }
#endif

    PRINT(System, INFO, "> nya");

    OSReturnToMenu();

    // Shouldn't return but spin infinitely if it somehow does
    while (true) {
    }
}

typedef void (*PFN_voidfunc)();
__attribute__((section(".ctors"))) extern PFN_voidfunc _ctors_start[];
__attribute__((section(".ctors"))) extern PFN_voidfunc _ctors_end[];

IMPORT(0x800746B0) //
void __init_cpp();

REPLACE(0x80074690) //

void __init_user()
{
    g_useRvlIpc = true;

    __init_cpp();

    for (PFN_voidfunc* ctor = _ctors_start; ctor != _ctors_end && *ctor;
         ++ctor) {
        (*ctor)();
    }
}

IMPORT(0x8006C30C) //
void OSPanic(const char* file, int line, const char* format, ...);

extern "C" void __assert_func(
    const char* file, int line, [[maybe_unused]] const char* func,
    const char* expr
)
{
    OSPanic(file, line, "%s", expr);
    while (true) {
    }
}

REPLACE(0x8006F834) //

void ConfigBATs()
{
}

void std::__throw_length_error(const char* str)
{
    OSPanic(__FILE__, __LINE__, "std::__throw_length_error: %s", str);
    __builtin_unreachable();
}

void std::__throw_bad_array_new_length()
{
    OSPanic(__FILE__, __LINE__, "std::__throw_bad_array_new_length");
    __builtin_unreachable();
}

void std::__throw_bad_alloc()
{
    OSPanic(__FILE__, __LINE__, "std::__throw_bad_alloc");
    __builtin_unreachable();
}

IMPORT(0x800E66F8) //
void someprintf(const char* str);

void VReport(const char* source, const char* format, va_list args)
{
    char str[80] = {};

    vsnprintf(str, sizeof(str), format, args);

    char* strPtr = str;
    while (*strPtr != '\0') {
        char* endPtr = std::strchr(strPtr, '\n');

        if (endPtr == nullptr) {
            endPtr = strPtr + std::strlen(strPtr);
        }

        u32 curLen = std::distance(strPtr, endPtr);

        char newStr[80] = {};
        strncpy(newStr, strPtr, curLen);

        Console::Print("I[");
        Console::Print(source);
        Console::Print("] ");
        Console::Print(newStr);
        Console::Print("\n");

        strPtr = endPtr;
        if (*strPtr == '\n') {
            strPtr++;
        }
    }
}

REPLACE(0x800E6884) //

void RVL_vprintf(const char* format, va_list args)
{
    VReport("OSReport", format, args);
}

REPLACE(0x800B2068) //

void WUD_DEBUGPrint(const char* format, ...)
{
    (void) format;

#if 0
    va_list args;
    va_start(args, format);
    VReport("WUD_DEBUGPrint", format, args);
    va_end(args);
#endif
}

REPLACE(0x80097F3C) //

s32 ESP_GetTitleId([[maybe_unused]] u64* titleID)
{
    // Should fail on its own, but just to enforce it
    return -1017;
}

// Greatly improves init performance by disabling NWC24 code
REPLACE(0x800739B0) //

void __OSInitNet()
{
}
