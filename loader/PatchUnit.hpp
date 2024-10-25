#pragma once

#include <DeviceStarlingTypes.hpp>
#include <Types.h>
#include <cstring>

class PatchUnitRiivolution;

class PatchUnit
{
public:
    enum class Type {
        NONE = 0,
        RIIVOLUTION = 1,
    };

    PatchUnit(DiskID diskId)
    {
        m_blockSize = sizeof(PatchUnit);
        m_diskId = diskId;
        m_type = Type::NONE;
    }

    PatchUnit* Next()
    {
        return reinterpret_cast<PatchUnit*>(
            reinterpret_cast<u8*>(this) + m_blockSize
        );
    }

    PatchUnitRiivolution* AsRiivolution()
    {
        if (m_type != Type::RIIVOLUTION) {
            return nullptr;
        }

        return reinterpret_cast<PatchUnitRiivolution*>(this);
    }

protected:
    static u8* AddString(u8* ptr, const char* str)
    {
        u32 len = std::strlen(str) + 1;
        if (len <= 0xFF) {
            ptr[0] = len;
            std::memcpy(ptr + 1, str, len);
            return ptr + 1 + len;
        } else {
            ptr[0] = 0x00;
            ptr[1] = len >> 24;
            ptr[2] = (len >> 16) & 0xFF;
            ptr[3] = (len >> 8) & 0xFF;
            ptr[4] = len & 0xFF;
            std::memcpy(ptr + 3, str, len);
            return ptr + 3 + len;
        }
    }

    void AddString(const char* str)
    {
        u8* ptr = reinterpret_cast<u8*>(this) + m_blockSize;

        m_blockSize = AddString(ptr, str) - ptr;
    }

    static u8* GetString(u8* ptr, const char** str = nullptr)
    {
        u32 len = ptr[0];
        if (len == 0) {
            len = (ptr[1] << 24) | (ptr[2] << 16) | (ptr[3] << 8) | ptr[4];
            if (str) {
                *str = reinterpret_cast<const char*>(ptr + 5);
            }
            return ptr + 5 + len;
        } else {
            if (str) {
                *str = reinterpret_cast<const char*>(ptr + 1);
            }
            return ptr + 1 + len;
        }
    }

public:
    u32 m_blockSize;
    DiskID m_diskId;
    Type m_type;
};
