#pragma once

#include <System/Util.h>
#include <cstring>

namespace ICache
{

void Invalidate(void* start, size_t size);

} // namespace ICache

#define BITS(_VALUE, _OFFSET, _SIZE)                                           \
    (_VALUE) << 32 - (_OFFSET) - (_SIZE) & (1 << (_SIZE)) - 1

#define CONFIG_BAT(_DBAT, _IBAT, _VP, _VS, _BL, _BEPI, _PP, _WIMG, _BRPN)      \
    LOAD_VALUE(                                                                \
        r4, BITS(_VP, 0, 1) | BITS(_VS, 1, 1) | BITS(_BL, 2, 11) |             \
                BITS(_BEPI, 17, 15)                                            \
    )                                                                          \
    LOAD_VALUE(r3, BITS(_PP, 0, 2) | BITS(_WIMG, 3, 4) | BITS(_BRPN, 17, 15))
