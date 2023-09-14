#include <System/Util.h>
#include <algorithm>
#include <cassert>
#include <string.h>

constexpr u32 ReadShorts(u16*& src)
{
    u32 value = (src[0] << 16) | src[1];
    src += 2;
    return value;
}

constexpr u32 ReadBytes(u8*& src)
{
    u32 value = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
    src += 4;
    return value;
}

void* memcpy(void* __restrict dst, const void* __restrict src, size_t len)
{
    if (len == 0) {
        return dst;
    }

    const u32 dstAddr = u32(dst);
    const u32 dstRounded = AlignDown(dstAddr, 4);
    const u32 dstEndAddr = dstAddr + len;
    const u32 dstEndRounded = AlignDown(dstEndAddr, 4);

    // Do main rounded copy in words

    if (len > 3) {
        u32* dstRoundUp = (u32*) AlignUp(dst, 4);
        u32 dstRoundUpLen = dstEndRounded - AlignUp(dstAddr, 4);
        u32 srcAlignAddr = u32(src) + AlignUp(dstAddr, 4) - dstAddr;

        if (IsAligned(srcAlignAddr, 4)) {
            // Copy reading words
            u32* srcFixed = (u32*) srcAlignAddr;

            // Copy 4 words at a time
            while (dstRoundUpLen >= 16) {
                *dstRoundUp++ = *srcFixed++;
                *dstRoundUp++ = *srcFixed++;
                *dstRoundUp++ = *srcFixed++;
                *dstRoundUp++ = *srcFixed++;
                dstRoundUpLen -= 16;
            }

            // Copy one word at a time
            while (dstRoundUpLen >= 4) {
                *dstRoundUp++ = *srcFixed++;
                dstRoundUpLen -= 4;
            }
        } else if (IsAligned(srcAlignAddr, 2)) {
            // Copy reading shorts
            u16* srcFixed = (u16*) srcAlignAddr;

            // Copy 4 words at a time
            while (dstRoundUpLen >= 16) {
                *dstRoundUp++ = ReadShorts(srcFixed);
                *dstRoundUp++ = ReadShorts(srcFixed);
                *dstRoundUp++ = ReadShorts(srcFixed);
                *dstRoundUp++ = ReadShorts(srcFixed);
                dstRoundUpLen -= 16;
            }

            // Copy one word at a time
            while (dstRoundUpLen >= 4) {
                *dstRoundUp++ = ReadShorts(srcFixed);
                dstRoundUpLen -= 4;
            }
        } else {
            // Copy reading bytes
            u8* srcFixed = (u8*) srcAlignAddr;

            // Copy 4 words at a time
            while (dstRoundUpLen >= 16) {
                *dstRoundUp++ = ReadBytes(srcFixed);
                *dstRoundUp++ = ReadBytes(srcFixed);
                *dstRoundUp++ = ReadBytes(srcFixed);
                *dstRoundUp++ = ReadBytes(srcFixed);
                dstRoundUpLen -= 16;
            }

            // Copy one word at a time
            while (dstRoundUpLen >= 4) {
                *dstRoundUp++ = ReadBytes(srcFixed);
                dstRoundUpLen -= 4;
            }
        }
    }

    // Write the leading bytes
    if (dstRounded != dstAddr) {
        u8* srcU8 = (u8*) src;
        u32 srcData = ReadBytes(srcU8);

        srcData >>= ((dstAddr % 4) * 8);
        u32 mask = 0xFFFFFFFF >> ((dstAddr % 4) * 8);
        if (dstEndAddr - dstRounded < 4)
            mask &= ~(0xFFFFFFFF >> ((dstEndAddr - dstRounded) * 8));
        MaskU32(dstRounded, mask, srcData & mask);
    }

    // Write the trailing bytes
    if (dstEndAddr != dstEndRounded &&
        // Check if this was covered by the leading bytes copy
        (dstEndRounded != dstRounded || dstRounded == dstAddr)) {
        u8* srcU8 = ((u8*) src) + dstEndRounded - dstAddr;
        u32 srcData = ReadBytes(srcU8);

        u32 mask = ~(0xFFFFFFFF >> ((dstEndAddr - dstEndRounded) * 8));
        MaskU32(dstEndRounded, mask, srcData & mask);
    }

    return dst;
}

void* memmove(void* dst, const void* src, size_t len)
{
    // memcpy copies forward
    if (u32(dst) < u32(src)) {
        return memcpy(dst, src, len);
    }

#ifdef TARGET_IOS
    // Not accomodated for here!
    assert(!InMEM1(dst));
#else
    assert(InMEM1Effective(dst) || InMEM2Effective(dst));
#endif

    // Copy backwards byte by byte
    const u8* srcU8 = ((const u8*) src);
    u8* dstU8 = ((u8*) dst);

    for (size_t i = len; i > 0; i--) {
        dstU8[i - 1] = srcU8[i - 1];
    }

    return dst;
}

void* memset(void* dst, int value0, size_t len)
{
    if (len == 0) {
        return dst;
    }

    u8 value = value0;

    const u32 dstAddr = u32(dst);
    const u32 dstRounded = AlignDown(dstAddr, 4);
    const u32 dstEndAddr = dstAddr + len;
    const u32 dstEndRounded = AlignDown(dstEndAddr, 4);

    // Do main rounded set in words
    u32 valueFixed = (value << 24) | (value << 16) | (value << 8) | value;

    if (len > 3) {
        u32* dstRoundUp = (u32*) AlignUp(dst, 4);
        u32 dstRoundUpLen = dstEndRounded - AlignUp(dstAddr, 4);

        // Write 4 words at a time
        while (dstRoundUpLen >= 16) {
            *dstRoundUp++ = valueFixed;
            *dstRoundUp++ = valueFixed;
            *dstRoundUp++ = valueFixed;
            *dstRoundUp++ = valueFixed;
            dstRoundUpLen -= 16;
        }

        // Write one word at a time
        while (dstRoundUpLen >= 4) {
            *dstRoundUp++ = valueFixed;
            dstRoundUpLen -= 4;
        }
    }

    // Write the leading bytes
    if (dstRounded != dstAddr) {
        u32 srcData = valueFixed;

        srcData >>= ((dstAddr % 4) * 8);
        u32 mask = 0xFFFFFFFF >> ((dstAddr % 4) * 8);
        if (dstEndAddr - dstRounded < 4)
            mask &= ~(0xFFFFFFFF >> ((dstEndAddr - dstRounded) * 8));
        MaskU32(dstRounded, mask, srcData & mask);
    }

    // Write the trailing bytes
    if (dstEndAddr != dstEndRounded &&
        // Check if this was covered by the leading bytes write
        (dstEndRounded != dstRounded || dstRounded == dstAddr)) {
        u32 srcData = valueFixed;

        u32 mask = ~(0xFFFFFFFF >> ((dstEndAddr - dstEndRounded) * 8));
        MaskU32(dstEndRounded, mask, srcData & mask);
    }

    return dst;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
    const u8* su1 = (const u8*) s1;
    const u8* su2 = (const u8*) s2;

    size_t i = 0;
    for (; i < n && su1[i] == su2[i]; i++) {
    }

    return i < n ? su1[i] - su2[i] : 0;
}

void* memchr(const void* s, int c, size_t n)
{
    const u8* su = (const u8*) s;

    for (size_t i = 0; i < n; i++) {
        if (su[i] == c) {
            // This function is also a const-cast
            return (void*) &su[i];
        }
    }

    return NULL;
}

size_t strlen(const char* s)
{
    const char* f = s;
    while (*s != '\0') {
        s++;
    }
    return s - f;
}

int strcmp(const char* s1, const char* s2)
{
    size_t i = 0;
    for (; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') {
            break;
        }
    }

    return s1[i] - s2[i];
}

int strncmp(const char* s1, const char* s2, size_t n)
{
    size_t i = 0;
    for (; i < n && s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') {
            break;
        }
    }

    return i < n ? s1[i] - s2[i] : 0;
}

char* strchr(const char* s, int c)
{
    while (*s != '\0') {
        if (*s == c) {
            // This function is also a const-cast
            return (char*) s;
        }

        s++;
    }

    return NULL;
}

char* strcpy(char* __restrict dst, const char* __restrict src)
{
    return (char*) memcpy(dst, src, strlen(src) + 1);
}

char* strncpy(char* __restrict dst, const char* __restrict src, size_t n)
{
    if (n == 0) {
        return dst;
    }

    return (char*) memcpy(dst, src, std::min<size_t>(strlen(src) + 1, n));
}
