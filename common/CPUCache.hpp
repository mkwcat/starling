#pragma once

#include <Types.h>

#ifdef TARGET_PPC

#  if __has_builtin(__builtin_dcbst)
#    define __dcbst(_ADDR) __builtin_dcbst(_ADDR)
#  else
#    define __dcbst(_ADDR) asm("dcbst 0, %0" ::"r"(_ADDR))
#  endif

#  if __has_builtin(__builtin_dcbf)
#    define __dcbf(_ADDR) __builtin_dcbf(_ADDR)
#  else
#    define __dcbf(_ADDR) asm("dcbf 0, %0" ::"r"(_ADDR))
#  endif

#  if __has_builtin(__builtin_dcbi)
#    define __dcbi(_ADDR) __builtin_dcbi(_ADDR)
#  else
#    define __dcbi(_ADDR) asm("dcbi 0, %0" ::"r"(_ADDR))
#  endif

#  if __has_builtin(__builtin_dcbz)
#    define __dcbz(_ADDR) __builtin_dcbz(_ADDR)
#  else
#    define __dcbz(_ADDR) asm("dcbz 0, %0" ::"r"(_ADDR))
#  endif

#  if __has_builtin(__builtin_icbi)
#    define __icbi(_ADDR) __builtin_icbi(_ADDR)
#  else
#    define __icbi(_ADDR) asm("icbi 0, %0" ::"r"(_ADDR))
#  endif

#endif

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
