#pragma once

#include <Import/Twm.h>
#include <System/ES.hpp>
#include <System/Types.h>

struct ChannelInitInfo {
    TwmImportEntry* twmTable;
    TwmImportEntry* twmTableEnd;

    // Provided by loader
    ES::TMDFixed<32> tmd;
    s32 rgnselVer;
};

extern ChannelInitInfo s_initInfo;

#define VER_KOREA (s_initInfo.rgnselVer == 1 || s_initInfo.rgnselVer == 2)
