// ACR.hpp - Wii Hardware I/O
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "HWOps.hpp"
#include <Types.h>

/**
 * ACR (Hollywood Registers).
 */
namespace ACR
{

enum class IPC_PPCMSG : u32 {
    _ADDRESS = 0x000,
    _ACCEPT_VALUE,
    _ACCEPT_POINTER,
};

enum class IPC_PPCCTRL : u32 {
    _ADDRESS = 0x004,

    X1 = 1 << 0,
    Y2 = 1 << 1,
    Y1 = 1 << 2,
    X2 = 1 << 3,
    IY1 = 1 << 4,
    IY2 = 1 << 5,
};

enum class IPC_ARMMSG : u32 {
    _ADDRESS = 0x008,
    _ACCEPT_VALUE,
    _ACCEPT_POINTER,
};

enum class IPC_ARMCTRL : u32 {
    _ADDRESS = 0x00C,

    X1 = 1 << 0,
    Y2 = 1 << 1,
    Y1 = 1 << 2,
    X2 = 1 << 3,
    IY1 = 1 << 4,
    IY2 = 1 << 5,
};

enum class TIMER : u32 {
    _ADDRESS = 0x010,
    _ACCEPT_VALUE,
};

enum class ALARM : u32 {
    _ADDRESS = 0x014,
    _ACCEPT_VALUE,
};

enum class VISOLID : u32 {
    _ADDRESS = 0x024,
    _ACCEPT_VALUE,
};

enum class PPC_IRQFLAG : u32 {
    _ADDRESS = 0x030,
    _ACCEPT_VALUE,
};

enum class PPC_IRQMASK : u32 {
    _ADDRESS = 0x034,
    _ACCEPT_VALUE,
};

enum class ARM_IRQFLAG : u32 {
    _ADDRESS = 0x038,
    _ACCEPT_VALUE,
};

enum class ARM_IRQMASK : u32 {
    _ADDRESS = 0x03C,
    _ACCEPT_VALUE,
};

// Controls access to the IOP SRAM
enum class SRNPROT : u32 {
    _ADDRESS = 0x060,

    // Enables the AES engine access to SRAM
    AESEN = 0x01,
    // Enables the SHA-1 engine access to SRAM
    SHAEN = 0x02,
    // Enables the Flash/NAND engine access to SRAM
    FLAEN = 0x04,
    // Enables PPC access to SRAM
    AHPEN = 0x08,
    // Enables OH1 access to SRAM
    OH1EN = 0x10,
    // Enables the SRAM mirror at 0xFFFE0000
    IOUEN = 0x20,

    IOPDBGEN = 0x40,
};

enum class BUSPROT : u32 {
    _ADDRESS = 0x064,

    // Flash/NAND Engine PPC; Set/cleared by syscall_54
    PPCFLAEN = 0x00000002,
    // AES Engine PPC; Set/cleared by syscall_54
    PPCAESEN = 0x00000004,
    // SHA-1 Engine PPC; Set/cleared by syscall_54
    PPCSHAEN = 0x00000008,
    // Enhanced Host Interface PPC; Set/cleared by syscall_54
    PPCEHCEN = 0x00000010,
    // Open Host Interface #0 PPC; Set/cleared by syscall_54
    PPC0H0EN = 0x00000020,
    // Open Host Interface #1 PPC; Set/cleared by syscall_54
    PPC0H1EN = 0x00000040,
    // SD Interface #0 PPC; Set/cleared by syscall_54
    PPCSD0EN = 0x00000080,
    // SD Interface #1 PPC; Set/cleared by syscall_54
    PPCSD1EN = 0x00000100,
    // ?? Set/cleared by syscall_54
    PPCSREN = 0x00000400,
    // ?? Set/cleared by syscall_54
    PPCAHMEN = 0x00000800,

    // Flash/NAND Engine IOP
    IOPFLAEN = 0x00020000,
    // AES Engine IOP
    IOPAESEN = 0x00040000,
    // SHA-1 Engine IOP
    IOPSHAEN = 0x00080000,
    // Enhanced Host Interface IOP
    IOPEHCEN = 0x00100000,
    // Open Host Interface #0 IOP
    IOP0H0EN = 0x00200000,
    // Open Host Interface #1 IOP
    IOP0H1EN = 0x00400000,
    // SD Interface #0 IOP
    IOPSD0EN = 0x00800000,
    // SD Interface #1 IOP
    IOPSD1EN = 0x01000000,

    // Gives PPC full read & write access to ACR that is normally only
    // accessible to IOP; Set/cleared by syscall_54
    PPCKERN = 0x80000000,
};

#define DEF_GPIO_PIN                                                           \
    POWER = 0x000001, SHUTDOWN = 0x000002, FAN = 0x000004, DC_DC = 0x000008,   \
    DI_SPIN = 0x000010, SLOT_LED = 0x000020, EJECT_BTN = 0x000040,             \
    SLOT_IN = 0x000080, SENSOR_BAR = 0x000100, DO_EJECT = 0x000200,            \
    EEP_CS = 0x000400, EEP_CLK = 0x000800, EEP_MOSI = 0x001000,                \
    EEP_MISO = 0x002000, AVE_SCL = 0x004000, AVE_SDA = 0x008000,               \
    DEBUG0 = 0x010000, DEBUG1 = 0x020000, DEBUG2 = 0x040000,                   \
    DEBUG3 = 0x080000, DEBUG4 = 0x100000, DEBUG5 = 0x200000,                   \
    DEBUG6 = 0x400000, DEBUG7 = 0x800000,

// Restricted PPC GPIO access

enum class GPIOB_OUT : u32 {
    _ADDRESS = 0x0C0,

    DEF_GPIO_PIN
};

enum class GPIOB_DIR : u32 {
    _ADDRESS = 0x0C4,

    DEF_GPIO_PIN
};

enum class GPIOB_IN : u32 {
    _ADDRESS = 0x0C8,

    DEF_GPIO_PIN
};

// Full GPIO access

enum class GPIO_OUT : u32 {
    _ADDRESS = 0x0E0,

    DEF_GPIO_PIN
};

enum class GPIO_DIR : u32 {
    _ADDRESS = 0x0E4,

    DEF_GPIO_PIN
};

enum class GPIO_IN : u32 {
    _ADDRESS = 0x0E8,

    DEF_GPIO_PIN
};

enum class RESETS : u32 {
    _ADDRESS = 0x194,

    // System reset
    RSTBINB = 0x0000001,
    // CRST reset?
    CRSTB = 0x0000002,
    // RSTB reset?
    RSTB = 0x0000004,
    // DSKPLL reset
    RSTB_DSKPLL = 0x0000008,
    // PowerPC HRESET
    RSTB_CPU = 0x0000010,
    // PowerPC SRESET
    SRSTB_CPU = 0x0000020,
    // SYSPLL reset
    RSTB_SYSPLL = 0x0000040,
    // Unlock SYSPLL reset?
    NLCKB_SYSPLL = 0x0000080,
    // MEM reset B
    RSTB_MEMRSTB = 0x0000100,
    // PI reset
    RSTB_PI = 0x0000200,
    // Drive Interface reset B
    RSTB_DIRSTB = 0x0000400,
    // MEM reset
    RSTB_MEM = 0x0000800,
    // GFX TCPE?
    RSTB_GFXTCPE = 0x0001000,
    // GFX reset?
    RSTB_GFX = 0x0002000,
    // Audio Interface I2S3 reset
    RSTB_AI_I2S3 = 0x0004000,
    // Serial Interface I/O reset
    RSTB_IOSI = 0x0008000,
    // External Interface I/O reset
    RSTB_IOEXI = 0x0010000,
    // Drive Interface I/O reset
    RSTB_IODI = 0x0020000,
    // MEM I/O reset
    RSTB_IOMEM = 0x0040000,
    // Processor Interface I/O reset
    RSTB_IOPI = 0x0080000,
    // Video Interface reset
    RSTB_VI = 0x0100000,
    // VI1 reset?
    RSTB_VI1 = 0x0200000,
    // IOP reset
    RSTB_IOP = 0x0400000,
    // ARM AHB reset
    RSTB_AHB = 0x0800000,
    // External DRAM reset
    RSTB_EDRAM = 0x1000000,
    // Unlock external DRAM reset?
    NLCKB_EDRAM = 0x2000000,
};

} // namespace ACR
