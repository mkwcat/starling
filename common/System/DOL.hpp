#pragma once

#include <System/Types.h>

struct DOL {
    static constexpr u32 SECTION_COUNT = 7 + 11;

    u32 section[SECTION_COUNT];
    u32 sectionAddr[SECTION_COUNT];
    u32 sectionSize[SECTION_COUNT];

    u32 bssAddr;
    u32 bssSize;
    u32 entryPoint;
    u32 pad[0x1C / 4];
};
