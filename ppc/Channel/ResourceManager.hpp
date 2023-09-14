#pragma once

#include <Import/NW4R.hpp>
#include <System/Types.h>

namespace Channel
{

/**
 * Manages loading assets for the channel UI.
 */
class ResourceManager
{
public:
    /**
     * ResourceManager constructor.
     */
    ResourceManager();

    /**
     * Get a pointer to the channel.arc ArcResourceAccessor.
     */
    nw4r::lyt::ArcResourceAccessor* GetChannelArchive()
    {
        return &m_channelArc;
    }

private:
    /**
     * Open content file from TMD.
     */
    s32 OpenTMDContent(u16 index);

    /**
     * Load the system font from NAND.
     */
    void LoadFont();

private:
    nw4r::lyt::ArcResourceAccessor m_channelArc;

    nw4r::ut::ArchiveFont m_archiveFont;
    nw4r::lyt::FontRefLink m_fontRefLink;
};

} // namespace Channel
