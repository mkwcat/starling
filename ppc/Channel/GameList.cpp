#include "GameList.hpp"
#include "ResourceManager.hpp"
#include "SceneManager.hpp"
#include "System.hpp"
#include <Debug/Log.hpp>
#include <System/Util.h>
#include <cassert>
#include <cstring>

namespace Channel
{

GameList::GameList()
{
    GameEntry entry = {
        .m_deviceId = 1,
        .m_fileId = 0,
        .m_titleId = {'R', 'M', 'C', 'E', '0', '1'},
        .m_revision = 0,
    };

    nw4r::lyt::ArcResourceAccessor* resAsr =
        System::GetResourceManager()->GetChannelArchive();

    m_wiitdbBin = (u8*) resAsr->GetResource(
        nw4r::lyt::ARC_TYPE_MISC, "wiitdb.bin", nullptr
    );
    assert(m_wiitdbBin);
    assert(ReadU32(m_wiitdbBin) == 0x57544442);

    m_wiitdbCount = ReadU32(m_wiitdbBin + 0x4);
    m_wiitdbText = m_wiitdbBin + ReadU32(m_wiitdbBin + 0x8);

    m_gameList.push_back(entry);

    entry = {
        .m_deviceId = 1,
        .m_fileId = 0,
        .m_titleId = {'S', 'M', 'N', 'E', '0', '1'},
        .m_revision = 0,
    };

    m_gameList.push_back(entry);
    m_gameList.push_back(entry);
    m_gameList.push_back(entry);
}

void GameList::Event_DeviceInsertion([[maybe_unused]] u32 id)
{
    ScopeLock lock(m_mutex);

    System::GetSceneManager()->SetGameListUpdate();
}

void GameList::Event_DeviceRemoval([[maybe_unused]] u32 id)
{
    ScopeLock lock(m_mutex);

    System::GetSceneManager()->SetGameListUpdate();
}

const WiiTDBEntry* GameList::SearchWiiTDB(const char* titleId)
{
    const WiiTDBEntry* entry =
        reinterpret_cast<const WiiTDBEntry*>(m_wiitdbBin + 0x10);

    for (u32 idx = 0; idx < m_wiitdbCount; idx++, entry = entry->GetNext()) {
        if (memcmp(titleId, entry->GetTitleID(), 6) == 0) {
            return entry;
        }
    }

    return nullptr;
}

} // namespace Channel
