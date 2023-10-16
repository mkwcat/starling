#pragma once

#include "HWOps.hpp"
#include <Types.h>

/**
 * Memory Controller Registers.
 */
namespace MEMCRReg
{

// DDR protection enable/disable
// Note: DDR protection affects DI as well.
enum class MEM_PROT_DDR : u16 {
    _ADDRESS = 0xB420A,
    _ACCEPT_VALUE,
};

// DDR protection base address
enum class MEM_PROT_DDR_BASE : u16 {
    _ADDRESS = 0xB420C,
    _ACCEPT_VALUE,
};

// DDR protection end address
enum class MEM_PROT_DDR_END : u16 {
    _ADDRESS = 0xB420E,
    _ACCEPT_VALUE,
};

}; // namespace MEMCRReg
