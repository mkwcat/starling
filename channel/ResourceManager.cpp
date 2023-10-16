#include "ResourceManager.hpp"
#include "Heap.hpp"
#include <Archive.hpp>
#include <IOS.hpp>
#include <ISFSTypes.hpp>
#include <ImportInfo.hpp>
#include <Log.hpp>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

// channel.arc
extern u8 ResourceArchive[];

namespace Channel
{

/**
 * ResourceManager constructor.
 */
ResourceManager::ResourceManager()
{
    m_channelArc.Attach(ResourceArchive, ".");

    PRINT(System, INFO, "Loading font...");
    LoadFont();
    PRINT(System, INFO, "Done loading font");
}

struct ContentMapEntry {
    char cid[8];
    u8 hash[0x14];
} ATTRIBUTE_PACKED;

static u8 s_contentMapBuffer[0x10000] alignas(32);

static ContentMapEntry* s_contentMap = nullptr;
static u32 s_contentMapCount = 0;

/**
 * Open content file from TMD.
 */
s32 ResourceManager::OpenTMDContent(u16 index)
{
    if (ImportInfo::s_tmd.numContents <= index) {
        return IOS::IOSError::INVALID;
    }

    if (ImportInfo::s_tmd.contents[index].type ==
        ES::TMDContent::Type::NORMAL) {
        char path[64];
        u64 titleID = ImportInfo::s_tmd.titleId;

        snprintf(
            path, 64, "/title/%08x/%08x/content/%08x.app", U64Hi(titleID),
            U64Lo(titleID), ImportInfo::s_tmd.contents[index].cid
        );

        return IOS_Open(path, IOS::Mode::READ);
    }

    if (ImportInfo::s_tmd.contents[index].type !=
        ES::TMDContent::Type::SHARED) {
        return IOS::IOSError::INVALID;
    }

    if (s_contentMap == nullptr) {
        // Load content.map
        IOS::File file("/shared1/content.map", IOS::Mode::READ);
        ASSERT(file.GetFd() >= 0);
        const u32 fileSize = file.GetSize();
        ASSERT(fileSize <= sizeof(s_contentMapBuffer));

        s32 ret = file.Read(&s_contentMapBuffer, fileSize);
        ASSERT(u32(ret) == fileSize);

        s_contentMap = reinterpret_cast<ContentMapEntry*>(&s_contentMapBuffer);
        s_contentMapCount = fileSize / sizeof(ContentMapEntry);
    }

    auto entry = std::find_if(
        s_contentMap, s_contentMap + s_contentMapCount,
        [&](const ContentMapEntry& entry) {
        return memcmp( //
                   entry.hash, ImportInfo::s_tmd.contents[index].hash, 0x14
               ) == 0;
        });

    if (entry == s_contentMap + s_contentMapCount) {
        return ISFS::ISFSError::NOT_FOUND;
    }

    char path[64];
    snprintf(path, sizeof(path), "/shared1/%.8s.app", entry->cid);

    return IOS_Open(path, IOS::Mode::READ);
}

/**
 * Load the system font from NAND.
 */
void ResourceManager::LoadFont()
{
    IOS::File file(OpenTMDContent(5));
    ASSERT(file.GetFd() >= 0);

    u8 arcHeader[0x100] alignas(32);

    s32 ret = file.Read(arcHeader, sizeof(arcHeader));
    ASSERT(u32(ret) == sizeof(arcHeader));
    ASSERT(ReadU32(arcHeader + 0xC) < 0x100);

    Archive archive(arcHeader, file.GetSize());
    auto entry = archive.get("wbf1.brfna");
    Archive::File* fontFile = std::get_if<Archive::File>(&entry);
    ASSERT(!!fontFile);

    ret = file.Seek(fontFile->offset, SEEK_SET);
    ASSERT(u32(ret) == fontFile->offset);

    void* fontData = Heap::AllocMEM2(AlignUp(fontFile->size, 32), 32);
    ASSERT(fontData != nullptr);
    ret = file.Read(fontData, AlignUp(fontFile->size, 32));
    ASSERT(u32(ret) == AlignUp(fontFile->size, 32));

    char param[] = {0, 0, 0, 0, 0, 0, 0, 0};

    u32 workBufferSize = m_archiveFont.GetRequireBufferSize(fontData, param);
    void* workBuffer = Heap::AllocMEM2(workBufferSize, 32);
    m_archiveFont.Construct(workBuffer, workBufferSize, fontData, param);

    m_fontRefLink.Set("wbf1.brfna", &m_archiveFont);
    m_channelArc.RegistFont(&m_fontRefLink);

    Heap::FreeMEM2(fontData);
}

} // namespace Channel
