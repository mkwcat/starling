#include "CPUCache.hpp"

#include <Util.h>

namespace CPUCache
{

#ifdef TARGET_PPC

// PPC cache functions

void DCStore(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t address = reinterpret_cast<size_t>(start);
    size = AlignUp(size, 0x20);
    do {
        asm("dcbst %y0" : : "Z"(*reinterpret_cast<u8*>(address)));
        address += 0x20;
        size -= 0x20;
    } while (size > 0);
    asm("sync");
}

void DCFlush(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t address = reinterpret_cast<size_t>(start);
    size = AlignUp(size, 0x20);
    do {
        asm("dcbf %y0" : : "Z"(*reinterpret_cast<u8*>(address)));
        address += 0x20;
        size -= 0x20;
    } while (size > 0);
    asm("sync");
}

void DCInvalidate(void* start, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t address = reinterpret_cast<size_t>(start);
    size = AlignUp(size, 0x20);
    do {
        asm("dcbi %y0" : : "Z"(*reinterpret_cast<u8*>(address)));
        address += 0x20;
        size -= 0x20;
    } while (size > 0);
}

void ICInvalidate(void* start, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t address = reinterpret_cast<size_t>(start);
    size = AlignUp(size, 0x20);
    do {
        asm("icbi %y0" : : "Z"(*reinterpret_cast<u8*>(address)));
        address += 0x20;
        size -= 0x20;
    } while (size > 0);
    asm("sync; isync");
}

#else

// IOS cache functions

#  include <Syscalls.h>

void DCStore(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }

    // No store on IOS
    IOS_FlushDCache(start, size);
}

void DCFlush(const void* start, size_t size)
{
    if (size == 0) {
        return;
    }

    IOS_FlushDCache(start, size);
}

void DCInvalidate(void* start, size_t size)
{
    if (size == 0) {
        return;
    }

    IOS_InvalidateDCache(start, size);
}

#endif

} // namespace CPUCache
