#pragma once

#include "EventManager.hpp"
#include "System.hpp"
#include <System/OS.hpp>
#include <System/Types.h>
#include <vector>

namespace Channel
{

class WiiTDBEntry;

class GameList : public EventHandler
{
    friend class WiiTDBEntry;

public:
    struct GameEntry {
        u32 m_deviceId;
        u64 m_fileId;

        char m_titleId[6];
        u32 m_revision;
    };

    GameList();

    void Event_DeviceInsertion(u32 id);

    void Event_DeviceRemoval(u32 id);

    /**
     * Get a copy of the game list, blocking until the list is available.
     */
    std::vector<GameEntry> GetEntries()
    {
        ScopeLock lock(m_mutex);

        return m_gameList;
    }

    const WiiTDBEntry* SearchWiiTDB(const char* titleId);

private:
    std::vector<GameEntry> m_gameList;

protected:
    const u8* m_wiitdbBin;
    const u8* m_wiitdbText;
    u32 m_wiitdbCount;

private:
    Mutex m_mutex;
};

class WiiTDBEntry
{
protected:
    char m_titleId[6];

    // Controller supported flags.
    u16 m_ctrlNunchuk : 1;
    u16 m_ctrlClassic : 1;
    u16 m_ctrlGCN : 1;
    u16 m_reserved : 3;

    // Title is in language flags (as in from this database).

    u16 m_titleJA : 1; // Japanese
    u16 m_titleEN : 1; // English
    u16 m_titleDE : 1; // German
    u16 m_titleFR : 1; // French
    u16 m_titleES : 1; // Spanish
    u16 m_titleIT : 1; // Italian
    u16 m_titleNL : 1; // Dutch
    u16 m_titleZHTW : 1; // Traditional Chinese
    u16 m_titleZHCN : 1; // Simplified Chinese
    u16 m_titleKO : 1; // Korean

    struct TitleEntry {
        u8 m_data[3];

        u32 GetData() const
        {
            return (m_data[0] << 16) | (m_data[1] << 8) | m_data[2];
        }

        bool IsShortChar() const
        {
            return GetData() & 0x800000;
        }

        u32 GetOffset() const
        {
            return GetData() & ~0x800000;
        }
    } ATTRIBUTE_PACKED;

    TitleEntry m_titleEntries[10];

private:
    static const u8* GetWiiTDBText()
    {
        return System::GetGameList()->m_wiitdbText;
    }

public:
    /**
     * Get the size of the entry in bytes.
     */
    u32 GetSize() const
    {
        return offsetof(WiiTDBEntry, m_titleEntries) +
               (m_titleJA + m_titleEN + m_titleDE + m_titleFR + m_titleES +
                 m_titleIT + m_titleNL + m_titleZHTW + m_titleZHCN +
                 m_titleKO) *
                 sizeof(TitleEntry);
    }

    /**
     * Get the next entry after this one.
     */
    const WiiTDBEntry* GetNext() const
    {
        return reinterpret_cast<const WiiTDBEntry*>(
          reinterpret_cast<const u8*>(this) + GetSize());
    }

    /**
     * Get the next entry after this one.
     */
    WiiTDBEntry* GetNext()
    {
        return reinterpret_cast<WiiTDBEntry*>(
          reinterpret_cast<u8*>(this) + GetSize());
    }

    const char* GetTitleID() const
    {
        return m_titleId;
    }

    bool IsNunchukSupported() const
    {
        return m_ctrlNunchuk;
    }

    bool IsClassicControllerSupported() const
    {
        return m_ctrlClassic;
    }

    bool IsGCNControllerSupported() const
    {
        return m_ctrlGCN;
    }

private:
    static u32 ReadTitleFromOffset(TitleEntry entry, u16* out, u32 maxLen)
    {
        if (maxLen < 2) {
            return 0;
        }

        const u8* text = GetWiiTDBText();

        if (entry.IsShortChar()) {
            // Short char
            const u8* title = text + entry.GetOffset();

            for (u32 i = 0;; i++) {
                if (i == maxLen - 1) {
                    out[i] = 0;
                    return i + 1;
                }

                out[i] = title[i];

                if (out[i] == 0) {
                    return i + 1;
                }
            }
        } else {
            // Wide char
            const u16* title =
              reinterpret_cast<const u16*>(text + entry.GetOffset());

            for (u32 i = 0;; i++) {
                if (i == maxLen - 1) {
                    out[i] = 0;
                    return i + 1;
                }

                out[i] = title[i];

                if (out[i] == 0) {
                    return i + 1;
                }
            }
        }
    }

public:
    u32 GetTitleJA(u16* out, u32 maxLen) const
    {
        if (!m_titleJA) {
            return 0;
        }

        return ReadTitleFromOffset(m_titleEntries[0], out, maxLen);
    }

    u32 GetTitleEN(u16* out, u32 maxLen) const
    {
        if (!m_titleEN) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA //
        ],
          out, maxLen);
    }

    u32 GetTitleDE(u16* out, u32 maxLen) const
    {
        if (!m_titleDE) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN //
        ],
          out, maxLen);
    }

    u32 GetTitleFR(u16* out, u32 maxLen) const
    {
        if (!m_titleFR) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN + m_titleDE //
        ],
          out, maxLen);
    }

    u32 GetTitleES(u16* out, u32 maxLen) const
    {
        if (!m_titleES) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN + m_titleDE + m_titleFR //
        ],
          out, maxLen);
    }

    u32 GetTitleIT(u16* out, u32 maxLen) const
    {
        if (!m_titleIT) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN + m_titleDE + m_titleFR + m_titleES //
        ],
          out, maxLen);
    }

    u32 GetTitleNL(u16* out, u32 maxLen) const
    {
        if (!m_titleNL) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN + m_titleDE + m_titleFR + m_titleES +
            m_titleIT //
        ],
          out, maxLen);
    }

    u32 GetTitleZHTW(u16* out, u32 maxLen) const
    {
        if (!m_titleZHTW) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN + m_titleDE + m_titleFR + m_titleES +
            m_titleIT + m_titleNL //
        ],
          out, maxLen);
    }

    u32 GetTitleZHCN(u16* out, u32 maxLen) const
    {
        if (!m_titleZHCN) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN + m_titleDE + m_titleFR + m_titleES +
            m_titleIT + m_titleNL + m_titleZHTW //
        ],
          out, maxLen);
    }

    u32 GetTitleKO(u16* out, u32 maxLen) const
    {
        if (!m_titleZHCN) {
            return 0;
        }

        return ReadTitleFromOffset( //
          m_titleEntries[ //
            m_titleJA + m_titleEN + m_titleDE + m_titleFR + m_titleES +
            m_titleIT + m_titleNL + m_titleZHTW + m_titleZHCN //
        ],
          out, maxLen);
    }
};

} // namespace Channel
