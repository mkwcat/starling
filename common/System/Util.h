// Util.h - Common Starling utilities
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "Types.h"
#include <assert.h>

#ifndef ATTRIBUTE_ALIGN
#  define ATTRIBUTE_ALIGN(v) __attribute__((aligned(v)))
#endif

#ifndef ATTRIBUTE_PACKED
#  define ATTRIBUTE_PACKED __attribute__((packed))
#endif

#ifndef ATTRIBUTE_TARGET
#  define ATTRIBUTE_TARGET(t) __attribute__((target(#t)))
#endif

#ifndef ATTRIBUTE_SECTION
#  define ATTRIBUTE_SECTION(t) __attribute__((section(#t)))
#endif

#ifndef ATTRIBUTE_NOINLINE
#  define ATTRIBUTE_NOINLINE __attribute__((noinline))
#endif

#ifdef __cplusplus
#  define EXTERN_C_START extern "C" {
#  define EXTERN_C_END }
#else
#  define EXTERN_C_START
#  define EXTERN_C_END
#endif

#define ASSERT assert

#define FILL(_START, _END) u8 _##_START[_END - _START]

#define ASM(...) asm volatile(#__VA_ARGS__)

#define ASM_THUMB_FUNCTION(_PROTOTYPE, ...)                                    \
    _Pragma("GCC diagnostic push");                                            \
    _Pragma("GCC diagnostic ignored \"-Wreturn-type\"");                       \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"");                  \
    __attribute__((naked)) ATTRIBUTE_TARGET(thumb)                             \
        ATTRIBUTE_NOINLINE _PROTOTYPE                                          \
    {                                                                          \
        ASM(__VA_ARGS__);                                                      \
    }                                                                          \
    _Pragma("GCC diagnostic pop");

#define ASM_ARM_FUNCTION(_PROTOTYPE, ...)                                      \
    _Pragma("GCC diagnostic push");                                            \
    _Pragma("GCC diagnostic ignored \"-Wreturn-type\"");                       \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"");                  \
    __attribute__((naked)) ATTRIBUTE_TARGET(arm) ATTRIBUTE_NOINLINE _PROTOTYPE \
    {                                                                          \
        ASM(__VA_ARGS__);                                                      \
    }                                                                          \
    _Pragma("GCC diagnostic pop");

#ifdef __cplusplus

template <typename T>
constexpr T AlignUp(T num, unsigned int align)
{
    u32 raw = (u32) num;
    return (T) ((raw + align - 1) & -align);
}

template <typename T>
constexpr T AlignDown(T num, unsigned int align)
{
    u32 raw = (u32) num;
    return (T) (raw & -align);
}

template <class T>
constexpr bool IsAligned(T addr, unsigned int align)
{
    return !((u32) addr & (align - 1));
}

typedef unsigned int size_t;

template <class T1, class T2>
constexpr bool CheckBounds(T1 bounds, size_t boundLen, T2 buffer, size_t len)
{
    size_t low = (size_t) bounds;
    size_t high = low + boundLen;
    size_t inside = (size_t) buffer;
    size_t insidehi = inside + len;

    return (high >= low) && (insidehi >= inside) && (inside >= low) &&
           (insidehi <= high);
}

template <class T>
constexpr bool InMEM1(T addr)
{
    const u32 value = (u32) addr;
    return value < 0x01800000;
}

template <class T>
constexpr bool InMEM2(T addr)
{
    const u32 value = (u32) addr;
    return (value >= 0x10000000) && (value < 0x14000000);
}

template <class T>
constexpr bool InMEM1Effective(T addr)
{
    const u32 value = (u32) addr;
    return (value >= 0x80000000) && (value < 0x81800000);
}

template <class T>
constexpr bool InMEM2Effective(T addr)
{
    const u32 value = (u32) addr;
    return (value >= 0x90000000) && (value < 0x94000000);
}

#endif

static inline u32 U64Hi(u64 value)
{
    return value >> 32;
}

static inline u32 U64Lo(u64 value)
{
    return value & 0xFFFFFFFF;
}

static inline u32 ByteSwapU32(u32 val)
{
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
           ((val & 0xFF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

static inline u16 ByteSwapU16(u16 val)
{
    return ((val & 0xFF) << 8) | ((val & 0xFF00) >> 8);
}

static inline u32 _ReadU8(u32 address)
{
    return *(volatile u8*) address;
}

static inline u32 _ReadU16(u32 address)
{
    return *(volatile u16*) address;
}

static inline u32 _ReadU32(u32 address)
{
    return *(volatile u32*) address;
}

static inline void _WriteU8(u32 address, u8 value)
{
    *(volatile u8*) address = value;
}

static inline void _WriteU16(u32 address, u16 value)
{
    *(volatile u16*) address = value;
}

static inline void _WriteU32(u32 address, u32 value)
{
    *(volatile u32*) address = value;
}

static inline void _MaskU16(u32 address, u16 clear, u16 set)
{
    *(volatile u16*) address = ((*(volatile u16*) address) & ~clear) | set;
}

static inline void _MaskU32(u32 address, u32 clear, u32 set)
{
    *(volatile u32*) address = ((*(volatile u32*) address) & ~clear) | set;
}

#define WriteU8(_ADDRESS, _VALUE) /*                                        */ \
    _WriteU8((u32) (_ADDRESS), (u8) (_VALUE))

#define WriteU16(_ADDRESS, _VALUE) /*                                       */ \
    _WriteU16((u32) (_ADDRESS), (u16) (_VALUE))

#define WriteU32(_ADDRESS, _VALUE) /*                                       */ \
    _WriteU32((u32) (_ADDRESS), (u32) (_VALUE))

#define ReadU8(_ADDRESS) /*                                                 */ \
    _ReadU8((u32) (_ADDRESS))

#define ReadU16(_ADDRESS) /*                                                */ \
    _ReadU16((u32) (_ADDRESS))

#define ReadU32(_ADDRESS) /*                                                */ \
    _ReadU32((u32) (_ADDRESS))

#define MaskU16(_ADDRESS, _CLEAR, _SET) /*                                  */ \
    _MaskU16((u32) (_ADDRESS), (u16) (_CLEAR), (u16) (_SET))

#define MaskU32(_ADDRESS, _CLEAR, _SET) /*                                  */ \
    _MaskU32((u32) (_ADDRESS), (u32) (_CLEAR), (u32) (_SET))

#define ReadU16LE(_ADDRESS) /*                                              */ \
    ByteSwapU16(_ReadU16((u32) (_ADDRESS)))

#define ReadU32LE(_ADDRESS) /*                                              */ \
    ByteSwapU32(_ReadU32((u32) (_ADDRESS)))

#define WriteU16LE(_ADDRESS, _VALUE) /*                                     */ \
    _WriteU16((u32) (_ADDRESS), ByteSwapU16((u16) (_VALUE)))

#define WriteU32LE(_ADDRESS, _VALUE) /*                                     */ \
    _WriteU32((u32) (_ADDRESS), ByteSwapU32((u32) (_VALUE)))

// libogc doesn't have this for some reason?
