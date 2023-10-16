#pragma once

#include <Types.h>
#include <Util.h>

/**
 * Base for the hardware registers. The 0x00800000 bit gets masked out when
 * PPC tries to access a register.
 */
#ifdef TARGET_IOS
constexpr u32 HW_BASE = 0x0D800000;
#else
constexpr u32 HW_BASE = 0xCD800000;
#endif

template <typename HWReg>
constexpr u32 HWRegRead()
{
    static_assert(
        sizeof(HWReg::_ACCEPT_VALUE),
        "Hardware register does not accept raw value"
    );

    static_assert(
        sizeof(HWReg) == sizeof(u8) || sizeof(HWReg) == sizeof(u16) ||
            sizeof(HWReg) == sizeof(u32),
        "Invalid hardware register size"
    );

    switch (sizeof(HWReg)) {
    case sizeof(u8):
        return ReadU8(HW_BASE | u32(HWReg::_ADDRESS));

    case sizeof(u16):
        return ReadU16(HW_BASE | u32(HWReg::_ADDRESS));

    case sizeof(u32):
        return ReadU32(HW_BASE | u32(HWReg::_ADDRESS));
    }
}

template <typename HWReg>
constexpr void HWRegWrite(u32 value)
{
    static_assert(
        sizeof(HWReg::_ACCEPT_VALUE),
        "Hardware register does not accept raw value"
    );

    static_assert(
        sizeof(HWReg) == sizeof(u8) || sizeof(HWReg) == sizeof(u16) ||
            sizeof(HWReg) == sizeof(u32),
        "Invalid hardware register size"
    );

    switch (sizeof(HWReg)) {
    case sizeof(u8):
        WriteU8(HW_BASE | u32(HWReg::_ADDRESS), value);
        break;

    case sizeof(u16):
        WriteU16(HW_BASE | u32(HWReg::_ADDRESS), value);
        break;

    case sizeof(u32):
        WriteU32(HW_BASE | u32(HWReg::_ADDRESS), value);
        break;
    }
}

template <typename HWReg>
constexpr bool HWRegReadFlag(HWReg flag)
{
    static_assert(
        sizeof(HWReg) == sizeof(u8) || sizeof(HWReg) == sizeof(u16) ||
            sizeof(HWReg) == sizeof(u32),
        "Invalid hardware register size"
    );

    switch (sizeof(HWReg)) {
    case sizeof(u8):
        return ReadU8(HW_BASE | u32(HWReg::_ADDRESS)) & static_cast<u8>(flag);

    case sizeof(u16):
        return ReadU16(HW_BASE | u32(HWReg::_ADDRESS)) & static_cast<u16>(flag);

    case sizeof(u32):
        return ReadU32(HW_BASE | u32(HWReg::_ADDRESS)) & static_cast<u32>(flag);
    }
}

template <typename HWReg>
constexpr void HWRegSetFlag(HWReg flag)
{
    static_assert(
        sizeof(HWReg) == sizeof(u8) || sizeof(HWReg) == sizeof(u16) ||
            sizeof(HWReg) == sizeof(u32),
        "Invalid hardware register size"
    );

    switch (sizeof(HWReg)) {
    case sizeof(u8):
        MaskU8(HW_BASE | u32(HWReg::_ADDRESS), 0, static_cast<u8>(flag));
        break;

    case sizeof(u16):
        MaskU16(HW_BASE | u32(HWReg::_ADDRESS), 0, static_cast<u16>(flag));
        break;

    case sizeof(u32):
        MaskU32(HW_BASE | u32(HWReg::_ADDRESS), 0, static_cast<u32>(flag));
        break;
    }
}

template <typename HWReg>
constexpr void HWRegSetFlag(HWReg flag, HWReg flag2)
{
    HWRegSetFlag(
        static_cast<HWReg>(static_cast<u32>(flag) | static_cast<u32>(flag2))
    );
}

template <typename HWReg, typename... Extra>
constexpr void HWRegSetFlag(HWReg flag, HWReg flag2, Extra... extra)
{
    HWRegSetFlag(
        static_cast<HWReg>(static_cast<u32>(flag) | static_cast<u32>(flag2)),
        extra...
    );
}

template <typename HWReg>
constexpr void HWRegClearFlag(HWReg flag)
{
    static_assert(
        sizeof(HWReg) == sizeof(u8) || sizeof(HWReg) == sizeof(u16) ||
            sizeof(HWReg) == sizeof(u32),
        "Invalid hardware register size"
    );

    switch (sizeof(HWReg)) {
    case sizeof(u8):
        MaskU8(HW_BASE | u32(HWReg::_ADDRESS), static_cast<u8>(flag), 0);
        break;

    case sizeof(u16):
        MaskU16(HW_BASE | u32(HWReg::_ADDRESS), static_cast<u16>(flag), 0);
        break;

    case sizeof(u32):
        MaskU32(HW_BASE | u32(HWReg::_ADDRESS), static_cast<u32>(flag), 0);
        break;
    }
}

template <typename HWReg>
constexpr void HWRegClearFlag(HWReg flag, HWReg flag2)
{
    HWClearFlag(
        static_cast<HWReg>(static_cast<u32>(flag) | static_cast<u32>(flag2))
    );
}

template <typename HWReg, typename... Extra>
constexpr void HWRegClearFlag(HWReg flag, HWReg flag2, Extra... extra)
{
    HWClearFlag(
        static_cast<HWReg>(static_cast<u32>(flag) | static_cast<u32>(flag2)),
        extra...
    );
}
