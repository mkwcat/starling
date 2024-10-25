#pragma once

#include "PatchUnit.hpp"

class PatchUnitRiivolution : public PatchUnit
{
public:
    // struct Choice {
    //     const char* m_name;
    //     const char** m_patches;
    // };

    // struct Option {
    //     const char* m_name;
    //     const char* m_id;
    //     const char* m_default;
    //     Choice* m_choices;
    // };

    // struct Section {
    //     const char* m_name;
    //     Option* m_options;
    // };

    // struct {
    //     bool m_valid;
    //     const char* m_root;

    //     struct {
    //         const char* m_game;
    //         const char* m_developer;
    //         const char* m_disc;
    //         const char* m_version;
    //         const char** m_regions;
    //     } m_id;

    //     Section* m_sections;
    // } m_wiidisc;

    PatchUnitRiivolution(DiskID diskId, const char* path, const char* xml)
      : PatchUnit(diskId)
    {
        (void) path;
        (void) xml;

        m_type = Type::RIIVOLUTION;
    }
};
