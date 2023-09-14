#pragma once

#include "GameList.hpp"
#include <Import/NW4R.hpp>
#include <System/OS.hpp>
#include <memory>
#include <vector>

namespace Channel
{

class SceneManager
{
public:
    SceneManager();

    /**
     * Run once per frame.
     */
    void Tick();

    /**
     * Called from GameList to notify SceneManager that the game list needs an
     * update.
     */
    void SetGameListUpdate()
    {
        m_gameListUpdate = true;
    }

private:
    /**
     * Draw the game select buttons to the screen.
     */
    void Draw_GameList();

    void GameList_CheckUpdate();

    static void GameList_Entry(void* sceneManager);

private:
    class Button
    {
    public:
        Button() = default;

        Button(nw4r::lyt::Layout* layout, const char* touchPane,
          const char* anim_freeToSelect, const char* anim_selectToFree)
        {
            Create(layout, touchPane, anim_freeToSelect, anim_selectToFree);
        }

        void Create(nw4r::lyt::Layout* layout, const char* touchPane,
          const char* anim_freeToSelect, const char* anim_selectToFree);
        void Calc();
        void FreeToSelect();
        void SelectToFree();

    private:
        void UnbindAnimation();

    private:
        nw4r::lyt::Layout* m_layout = nullptr;
        nw4r::lyt::Pane* m_touchPane = nullptr;

        nw4r::lyt::AnimTransform* m_animFreeToSelect = nullptr;
        nw4r::lyt::AnimTransform* m_animSelectToFree = nullptr;

        enum class AnimTag {
            FREE,
            FREE_TO_SELECT,
            SELECT,
            SELECT_TO_FREE,
        };

        AnimTag m_animTag = AnimTag::FREE;
    };

    struct LayoutGameEntry {
        LayoutGameEntry(GameList::GameEntry from, float x, float y);

        u8 m_deviceId;
        char m_titleId[6];
        u32 m_revision;

        std::unique_ptr<nw4r::lyt::Layout> m_buttonLayout;
        Button m_buttonCtrl;

        bool m_hasIcon;
        std::unique_ptr<nw4r::lyt::Layout> m_iconLayout;

        nw4r::lyt::AnimTransform* m_iconAnimation;
        nw4r::lyt::Pane* m_bannerWindowPane;
    };

    float m_frameNum;

    std::vector<LayoutGameEntry> m_gameList;
    Thread m_gameListThread;

    bool m_gameListUpdate;

    enum class GameListStage {
        FADE_OUT,
        LOADING,
        FADE_IN,
        READY,
    };

    GameListStage m_gameListStage;
};

} // namespace Channel
