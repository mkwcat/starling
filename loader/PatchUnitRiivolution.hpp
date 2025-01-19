#pragma once

#include "PatchUnit.hpp"
#include <IOS.hpp>
#include <functional>
#include <variant>

class PatchUnitRiivolution : public PatchUnit
{
public:
    static PatchUnitRiivolution* Get(PatchUnit* patchUnit)
    {
        if (patchUnit->GetType() != PatchUnit::Type::RIIVOLUTION) {
            return nullptr;
        }

        return static_cast<PatchUnitRiivolution*>(patchUnit);
    }

    PatchUnitRiivolution(DiskID diskId, const char* xml, s32 size);

    PatchUnitRiivolution(DiskID diskId, IOS::File& file);

    const char* GetXML() const
    {
        return reinterpret_cast<const char*>(GetData()) + sizeof(*this) -
               sizeof(PatchUnit);
    }

    bool IsValid() const
    {
        return m_valid;
    }

    bool IsGameID(const char* gameId) const;

    bool IsRegion(char region) const;

    bool IsShiftFiles() const;

    struct FileNode {
        bool resize = false;
        bool create = false;
        const char* disc = nullptr;
        u32 offset = 0;
        const char* external = nullptr;
        u32 fileoffset = 0;
        u32 length = 0;
    };

    struct FolderNode {
        bool create = false;
        bool resize = false;
        bool recursive = false;
        u32 length = 0;
        const char* disc = nullptr;
        const char* external = nullptr;
    };

    struct ShiftNode {
        const char* source = nullptr;
        const char* destination = nullptr;
    };

    struct SavegameNode {
        const char* external = nullptr;
        bool clone = false;
    };

    struct DLCNode {
        const char* external = nullptr;
    };

    struct MemoryNode {
        u32 offset = 0;
        bool search = false;
        bool ocarina = false;
        u32 align = 1;
        const char* valuefile = nullptr;
        const char* value = nullptr;
        const char* original = nullptr;
    };

    using PatchNode = std::variant<
        FileNode, FolderNode, ShiftNode, SavegameNode, DLCNode, MemoryNode>;

    bool HandlePatch(
        const char* patchId, std::function<bool(const PatchNode&)> callback
    );

private:
    void Init();

    char m_gameId[4];
    bool m_valid = false;
};