#pragma once

#include <ES.hpp>
#include <ImportTypes.h>
#include <Types.h>

class ImportInfo
{
public:
    static bool IsKorea()
    {
        return s_importDolId == 1 || s_importDolId == 2;
    }

    static u8 s_importDolId;
    static ES::TMDFixed<512> s_tmd;
};
