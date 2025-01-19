#pragma once

#include "PatchUnit.hpp"
#include "PatchUnitRiivolution.hpp"
#include <new>
#include <type_traits>

class PatchManager
{
public:
    static void StaticInit();

    static PatchUnit* CreatePatchUnit();

    template <typename T, typename... Args>
        requires(std::is_base_of_v<PatchUnit, T>)
    static T* CreatePatchUnit(Args&&... args)
    {
        if (s_last->m_type == PatchUnit::Type::DISABLED) {
            return new (s_last) T(args...);
        }

        PatchUnit* patchUnit = s_last->m_next;
        if (patchUnit == nullptr) {
            s_last->m_next = patchUnit = s_last + 1;
        }

        s_last = patchUnit;
        return new (patchUnit) T(args...);
    }

    /**
     * Load one or multiple Riivolution XML files.
     * @param path Path to the XML file or directory containing XML files.
     */
    static bool LoadRiivolutionXML(const char* path);

    static bool LoadPatchID(const char* patchId);

    static bool HandlePatchNode(
        PatchUnitRiivolution* unit, const PatchUnitRiivolution::PatchNode& node
    );

public:
    static PatchUnit* s_first;
    static PatchUnit* s_last;
};
