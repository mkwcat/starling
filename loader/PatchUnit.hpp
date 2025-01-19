#pragma once

#include <DeviceStarlingTypes.hpp>
#include <Types.h>
#include <Util.h>
#include <cassert>
#include <cstring>

class PatchUnit
{
    friend class PatchManager;

public:
    enum class Type {
        DISABLED,
        GENERIC,
        RIIVOLUTION,
    };

    PatchUnit(u32 size, Type type, DiskID diskId)
      : m_diskId(diskId)
      , m_type(type)
    {
        m_next =
            reinterpret_cast<PatchUnit*>(reinterpret_cast<u8*>(this) + size);
        m_next->m_type = Type::DISABLED;
    }

    const u8* GetData() const
    {
        return reinterpret_cast<const u8*>(this) + sizeof(*this);
    }

    u32 GetDataSize() const
    {
        return m_next != nullptr ? reinterpret_cast<const u8*>(m_next) -
                                       reinterpret_cast<const u8*>(GetData())
                                 : 0;
    }

    PatchUnit* GetNext() const
    {
        return m_next;
    }

    Type GetType() const
    {
        return m_type;
    }

    u32 GetSize() const
    {
        return sizeof(*this) + GetDataSize();
    }

protected:
    u8* GetData()
    {
        return reinterpret_cast<u8*>(this) + sizeof(*this);
    }

    u8* ExpandData(u32 size)
    {
        assert(m_next == nullptr || m_next->m_type == Type::DISABLED);

        size = AlignUp(size, 4);

        u8* data = const_cast<u8*>(GetData()) + GetDataSize();
        m_next = reinterpret_cast<PatchUnit*>(data + size);
        m_next->m_type = Type::DISABLED;
        m_next->m_next = nullptr;

        return data;
    }

    PatchUnit* m_next;
    DiskID m_diskId;
    Type m_type = Type::GENERIC;
};
