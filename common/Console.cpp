// Console.cpp - Visual debug console shared between PPC and IOS
//   Written by Palapeli
//   Written by stebler
//
// Based on MKW-SP:
// https://github.com/stblr/mkw-sp/blob/main/common/Console.cc
// https://github.com/stblr/mkw-sp/blob/main/common/VI.cc
//
// SPDX-License-Identifier: MIT

#include <AddressMap.h>
#include <CPUCache.hpp>
#include <Console.hpp>
#include <Util.h>

extern const u8 ConsoleFont[128][16];

constexpr u8 BG_INTENSITY = 16;
constexpr u8 FG_INTENSITY = 235;

constexpr u8 GLYPH_WIDTH = 8;
constexpr u8 GLYPH_HEIGHT = 16;

constexpr bool SIDEWAYS_CONSOLE = true;

static u16 s_xfbWidth;
static u16 s_xfbHeight;
u32* const s_xfb = reinterpret_cast<u32*>(CONSOLE_XFB_ADDRESS);

static u8 s_cols;
static u8 s_rows;
static u8 s_col;
static bool s_newline = false;

#ifndef TARGET_IOS

extern "C" volatile u16 vtr;
extern "C" volatile u16 dcr;
extern "C" volatile u32 vto;
extern "C" volatile u32 vte;
extern "C" volatile u32 tfbl;
extern "C" volatile u32 bfbl;
extern "C" volatile u16 hsw;
extern "C" volatile u16 hsr;
extern "C" volatile u16 visel;

/**
 * Initalize and display the debug console.
 */
void Console::Init()
{
    ConfigureVideo(true);

    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    CPUCache::DCInvalidate(data, sizeof(Boot_ConsoleData));

    *data = {
        .xfbWidth = s_xfbWidth,
        .xfbHeight = s_xfbHeight,
        .lock = 0,
        .ppcRow = -1,
        .iosRow = -1,

        .xfbInit = true,
    };

    CPUCache::DCFlush(data, sizeof(Boot_ConsoleData));
}

/**
 * Configure VI for the debug console.
 * @param clear Clear the XFB.
 */
void Console::ConfigureVideo(bool clear)
{
    bool isProgressive = visel & 1 || dcr & 4;
    bool isNtsc = (dcr >> 8 & 3) == 0;
    s_xfbWidth = 640;
    s_xfbHeight = isProgressive || isNtsc ? 480 : 574;

    if (clear) {
        for (u16 y = 0; y < s_xfbHeight; y++) {
            for (u16 x = 0; x < s_xfbWidth; x++) {
                WriteGrayscaleToXFB(x, y, 16);
            }
        }
        FlushXFB();
    }

    vtr = s_xfbHeight << (3 + isProgressive) | (vtr & 0xf);
    if (isProgressive) {
        vto = 0x6 << 16 | 0x30;
        vte = 0x6 << 16 | 0x30;
    } else if (isNtsc) {
        vto = 0x3 << 16 | 0x18;
        vte = 0x2 << 16 | 0x19;
    } else {
        vto = 0x1 << 16 | 0x23;
        vte = 0x0 << 16 | 0x24;
    }
    hsw = 0x2828;
    hsr = 0x10F5;
    tfbl = 1 << 28 | reinterpret_cast<u32>(s_xfb) >> 5;
    bfbl = 1 << 28 | reinterpret_cast<u32>(s_xfb) >> 5;

    if (!SIDEWAYS_CONSOLE) {
        s_cols = GetXFBWidth() / GLYPH_WIDTH - 1;
        s_rows = GetXFBHeight() / GLYPH_HEIGHT - 2;
    } else {
        s_cols = GetXFBHeight() / GLYPH_WIDTH - 1;
        s_rows = GetXFBWidth() / GLYPH_HEIGHT - 2;
    }
    s_col = 0;
}

#endif

/**
 * Reinitialize the console after reloading to a new instance.
 */
void Console::Reinit()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    CPUCache::DCInvalidate(data, sizeof(Boot_ConsoleData));

    s_xfbWidth = data->xfbWidth;
    s_xfbHeight = data->xfbHeight;

    if (!SIDEWAYS_CONSOLE) {
        s_cols = GetXFBWidth() / GLYPH_WIDTH - 1;
        s_rows = GetXFBHeight() / GLYPH_HEIGHT - 2;
    } else {
        s_cols = GetXFBHeight() / GLYPH_WIDTH - 1;
        s_rows = GetXFBWidth() / GLYPH_HEIGHT - 2;
    }
    s_col = 0;

    Print("\n");
}

#ifdef TARGET_IOS

#  include <Syscalls.h>

/**
 * IOS: Lock resource from PPC.
 */
static void Lock()
{
    auto data =
        reinterpret_cast<volatile Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);

    for (u32 i = 0; i < 8;) {
        IOS_InvalidateDCache(
            const_cast<Boot_ConsoleData*>(data), sizeof(Boot_ConsoleData)
        );

        u32 lock = data->lock;

        // Check if PPC has locked it
        if (lock & Boot_ConsoleData::PPC_LOCK) {
            i = 0;
            continue;
        }

        data->lock = lock | Boot_ConsoleData::IOS_LOCK;
        CPUCache::DCFlush(
            const_cast<Boot_ConsoleData*>(data), sizeof(Boot_ConsoleData)
        );
        i++;
    }
}

/**
 * IOS: Unlock resource to PPC.
 */
static void Unlock()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    IOS_InvalidateDCache(data, sizeof(Boot_ConsoleData));

    data->lock = 0;

    CPUCache::DCFlush(data, sizeof(Boot_ConsoleData));
}

/**
 * IOS: Get current row.
 */
static u8 GetRow()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    CPUCache::DCInvalidate(data, sizeof(Boot_ConsoleData));

    return data->iosRow;
}

/**
 * IOS: Increment row.
 */
static s32 IncrementRow()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    CPUCache::DCInvalidate(data, sizeof(Boot_ConsoleData));

    data->iosRow++;
    if (data->iosRow <= data->ppcRow) {
        data->iosRow = data->ppcRow + 1;
    }

    CPUCache::DCFlush(data, sizeof(Boot_ConsoleData));

    return data->iosRow;
}

/**
 * IOS: Decrement row.
 */
static s32 DecrementRow()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    CPUCache::DCInvalidate(data, sizeof(Boot_ConsoleData));

    data->iosRow--;
    data->ppcRow--;

    CPUCache::DCFlush(data, sizeof(Boot_ConsoleData));

    return data->iosRow;
}

#else

/**
 * PPC: Lock resource from IOS.
 */
static void Lock()
{
    auto data = reinterpret_cast<volatile Boot_ConsoleData*>(
        CONSOLE_DATA_ADDRESS | 0xC0000000
    );

    for (u32 i = 0; i < 16;) {
        u32 lock = data->lock;

        // Check if IOS has locked it
        if (lock & Boot_ConsoleData::IOS_LOCK) {
            i = 0;
            continue;
        }

        data->lock = lock | Boot_ConsoleData::PPC_LOCK;
        i++;
    }
}

/**
 * PPC: Unlock resource to IOS.
 */
static void Unlock()
{
    auto data =
        reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    data->lock = 0;
}

/**
 * PPC: Get current row.
 */
static s32 GetRow()
{
    auto data =
        reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    return data->ppcRow;
}

/**
 * PPC: Increment row.
 */
static s32 IncrementRow()
{
    auto data =
        reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    data->ppcRow++;
    if (data->ppcRow <= data->iosRow) {
        data->ppcRow = data->iosRow + 1;
    }

    return data->ppcRow;
}

/**
 * PPC: Decrement row.
 */
static s32 DecrementRow()
{
    auto data =
        reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    data->iosRow--;
    data->ppcRow--;

    return data->ppcRow;
}

#endif

/**
 * Get the width of the console framebuffer.
 */
u16 Console::GetXFBWidth()
{
    return s_xfbWidth;
}

/**
 * Get the height of the console framebuffer.
 */
u16 Console::GetXFBHeight()
{
    return s_xfbHeight;
}

/**
 * Read from the specified pixel on the framebuffer.
 */
u8 Console::ReadGrayscaleFromXFB(u16 x, u16 y)
{
    if (x > s_xfbWidth || y > s_xfbHeight) {
        return 16;
    }

    u32 val = s_xfb[y * (s_xfbWidth / 2) + x / 2];
    if (x & 1) {
        return val >> 8;
    } else {
        return val >> 24;
    }
}

/**
 * Write to the specified pixel on the framebuffer.
 */
void Console::WriteGrayscaleToXFB(u16 x, u16 y, u8 intensity)
{
    if (x > s_xfbWidth || y > s_xfbHeight) {
        return;
    }

    u32* val = &s_xfb[y * (s_xfbWidth / 2) + x / 2];
    u8 y0 = *val >> 24;
    u8 y1 = *val >> 8;
    if (x & 1) {
        y1 = intensity;
    } else {
        y0 = intensity;
    }
    *val = y0 << 24 | 127 << 16 | y1 << 8 | 127;
}

/**
 * Move the framebuffer up by the specified height.
 */
void Console::MoveUp(u16 height)
{
    if (!SIDEWAYS_CONSOLE) {
        u32 offset = height * (s_xfbWidth / 2);

        u32 src = AlignDown(offset, 32);
        u32 dest = 0;
        u32 totalSize = AlignDown(s_xfbHeight * (s_xfbWidth / 2), 32);

        // Copy 8 words at a time
        while (src < totalSize) {
            s_xfb[dest++] = s_xfb[src++];
            s_xfb[dest++] = s_xfb[src++];
            s_xfb[dest++] = s_xfb[src++];
            s_xfb[dest++] = s_xfb[src++];
            s_xfb[dest++] = s_xfb[src++];
            s_xfb[dest++] = s_xfb[src++];
            s_xfb[dest++] = s_xfb[src++];
            s_xfb[dest++] = s_xfb[src++];
        }
    } else {
        // Move left instead of up
        u32 offset = height / 2;
        u32 lineCount = s_xfbWidth / 2 - offset;

        for (u32 y = 0; y < s_xfbHeight; y++) {
            u32 src = y * (s_xfbWidth / 2) + offset;
            u32 dest = y * (s_xfbWidth / 2);

            for (u32 i = 0; i < lineCount; i++) {
                s_xfb[dest++] = s_xfb[src++];
            }

            for (u32 i = 0; i < offset; i++) {
                WriteGrayscaleToXFB(s_xfbWidth - offset + i, y, 16);
            }
        }
    }
}

/**
 * Flush the XFB to main memory after writing to it.
 */
void Console::FlushXFB()
{
    CPUCache::DCFlush(s_xfb, 320 * 574 * sizeof(u32));
}

/**
 * Print a character. Assumes the resource is already locked.
 */
static void PrintChar(char c)
{
    if (c == '\n') {
        if (s_newline) {
            IncrementRow();
        }

        s_col = 0;
        s_newline = true;
        return;
    }

    if (c == '\r') {
        s_col = 0;
        return;
    }

    if (s_newline) {
        IncrementRow();
        s_newline = false;
    }

    if (s_col >= s_cols) {
        IncrementRow();
        s_col = 0;
    }

    s32 row = GetRow();

    if (row < 0) {
        return;
    }

    while (row >= s_rows) {
        Console::MoveUp(GLYPH_HEIGHT);
        row = DecrementRow();
    }

    const u8* glyph = ConsoleFont[' '];
    if (u32(c) < 128) {
        glyph = ConsoleFont[u32(c)];
    }

    u16 y0 = row * GLYPH_HEIGHT + GLYPH_HEIGHT / 2;
    for (u16 y = 0; y < GLYPH_HEIGHT; y++) {
        u16 x0 = s_col * GLYPH_WIDTH + GLYPH_WIDTH / 2;
        for (u16 x = 0; x < GLYPH_WIDTH; x++) {
            u8 intensity =
                glyph[(y * GLYPH_WIDTH + x) / 8] & (1 << (8 - (x % 8)))
                    ? FG_INTENSITY
                    : BG_INTENSITY;
            if (SIDEWAYS_CONSOLE) {
                Console::WriteGrayscaleToXFB(
                    y0 + y, s_xfbHeight - (x0 + x), intensity
                );
            } else {
                Console::WriteGrayscaleToXFB(x0 + x, y0 + y, intensity);
            }
        }
    }

    s_col++;
}

/**
 * Print a string to the visual console.
 */
void Console::Print(const char* s)
{
    Lock();
    for (; *s; s++) {
        PrintChar(*s);
    }
    FlushXFB();
    Unlock();
}
