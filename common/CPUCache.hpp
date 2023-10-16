#pragma once

#include <Types.h>

namespace CPUCache
{

void DCStore(const void* start, u32 size);

template <typename T>
void DCStore(const T& val)
{
    DCStore(&val, sizeof(T));
}

void DCFlush(const void* start, u32 size);

template <typename T>
void DCFlush(const T& val)
{
    DCFlush(&val, sizeof(T));
}

void DCInvalidate(void* start, u32 size);

template <typename T>
void DCInvalidate(T& val)
{
    DCInvalidate(&val, sizeof(T));
}

#ifdef TARGET_PPC

void ICInvalidate(void* start, u32 size);

#endif

} // namespace CPUCache
