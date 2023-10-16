#include "SceneManager.hpp"
#include "ResourceManager.hpp"
#include "System.hpp"
#include <Import_Rgnsel.hpp>
#include <Log.hpp>
#include <cmath>
#include <cstring>
#include <utility>

namespace Channel
{

SceneManager::SceneManager()
{
    m_gameListStage = GameListStage::LOADING;
    m_gameListThread.Create(&GameList_Entry, this);
    m_gameListUpdate = false;
    m_frameNum = 0;
}

/**
 * Run once per frame.
 */
void SceneManager::Tick()
{
    switch (m_gameListStage) {
    case GameListStage::FADE_OUT:
        // TODO: Have an actual fade here, start the thread once the fade has
        // completed.
        m_gameListStage = GameListStage::LOADING;
        m_gameListThread.Cancel();
        m_gameListThread.Create(&GameList_Entry, this);
        break;

    case GameListStage::LOADING:
        break;

    case GameListStage::FADE_IN:
        m_gameListStage = GameListStage::READY;
        // Fall through

    case GameListStage::READY:
        if (m_gameListUpdate) {
            m_gameListUpdate = false;
            m_gameListStage = GameListStage::FADE_OUT;
        }

        Draw_GameList();
        break;
    }

    m_frameNum++;
}

struct GlobalPosition {
    nw4r::math::VEC3 position;
    nw4r::lyt::Size size;
};

static GlobalPosition GetGlobalPanePos(nw4r::lyt::Pane* pane)
{
    auto mtx = pane->GetGlobalMtx();
    auto size = pane->GetSize();

    nw4r::math::VEC3 vec0, vec1, vec2;

    switch (pane->GetBasePositionH()) {
    case 0:
        vec0.x = size.width / 2;
        vec1.x = 0;
        vec2.x = size.width;
        break;

    case 1:
        vec0.x = 0;
        vec1.x = -(size.width / 2);
        vec2.x = size.width / 2;
        break;

    case 2:
        vec0.x = size.height / 2;
        vec1.x = -size.width;
        vec2.x = 0;
        break;
    }

    switch (pane->GetBasePositionV()) {
    case 0:
        vec0.y = -(size.height / 2);
        vec1.y = 0;
        vec2.y = 0;
        break;

    case 1:
        vec0.y = 0;
        vec1.y = size.height / 2;
        vec2.y = size.height / 2;
        break;

    case 2:
        vec0.y = size.height / 2;
        vec1.y = size.height;
        vec2.y = size.height;
        break;
    }

    vec0.z = 0;
    vec1.z = 0;
    vec2.z = 0;

    nw4r::math::VEC3 position, sizeVec0, sizeVec1;

    MTXMultVec(mtx.mtx, vec0.f, position.f);
    MTXMultVec(mtx.mtx, vec1.f, sizeVec0.f);
    MTXMultVec(mtx.mtx, vec2.f, sizeVec1.f);

    sizeVec1.x -= position.x;
    if (sizeVec1.x <= 0) {
        sizeVec1.x *= -1;
    }

    sizeVec0.x -= position.x;
    if (sizeVec0.x <= 0) {
        sizeVec0.x *= -1;
    }

    if (sizeVec1.x < sizeVec0.x) {
        sizeVec1.x = sizeVec0.x;
    }

    sizeVec1.y -= position.y;
    if (sizeVec1.y <= 0) {
        sizeVec1.y *= -1;
    }

    sizeVec0.y -= position.y;
    if (sizeVec0.y <= 0) {
        sizeVec0.y *= -1;
    }

    if (sizeVec1.y < sizeVec0.y) {
        sizeVec1.y = sizeVec0.y;
    }

    position.y += sizeVec1.y;

    return {
        .position = position,
        .size =
            {
                .width = sizeVec1.x,
                .height = sizeVec1.y,
            },
    };
}

/**
 * Draw the game select buttons to the screen.
 */
void SceneManager::Draw_GameList()
{
    for (auto& entry : m_gameList) {
        if (entry.m_hasIcon) {
            auto bannerPos = GetGlobalPanePos(entry.m_bannerWindowPane);
            auto bannerRootPane = entry.m_iconLayout->GetRootPane();
            auto rect = System::GetProjectionRect();
            auto rmode = System::GetCurrentRenderMode();

            entry.m_iconAnimation->SetFrame(
                std::fmod(m_frameNum, entry.m_iconAnimation->GetFrameSize())
            );

            bannerRootPane->SetPosition(nw4r::math::VEC3(
                bannerPos.position.x, bannerPos.position.y, -50
            ));

            bannerRootPane->SetScale(nw4r::math::VEC2(
                bannerPos.size.height / 45.714, bannerPos.size.height / 45.714
            ));

            float bannerX =
                // Adjust position to left side of frame
                (bannerPos.position.x - (bannerPos.size.height * 2))
                    // Convert from widescreen pixel count to actual framebuffer
                    // pixel count
                    * ((rmode.fbWidth / 2) / rect.right)
                // Move base from center to leftmost pixel
                + (rmode.fbWidth / 2);

            float bannerXSize =
                // Get full width based on height
                (bannerPos.size.height * 4) *
                ((rmode.fbWidth / 2) / rect.right);

            GXSetScissor(
                bannerX + 1,
                229 - (bannerPos.position.y + bannerPos.size.height),
                bannerXSize - 2, bannerPos.size.height * 2 - 2
            );

            entry.m_iconLayout->Animate();
            entry.m_iconLayout->CalculateMtx(System::GetDrawInfo());
            entry.m_iconLayout->Draw(System::GetDrawInfo());

            // Reset scissor
            GXSetScissor(0, 0, rmode.fbWidth, rmode.efbHeight);
        }

        entry.m_buttonCtrl.Calc();
        entry.m_buttonLayout->Animate();
        entry.m_buttonLayout->CalculateMtx(System::GetDrawInfo());
        entry.m_buttonLayout->Draw(System::GetDrawInfo());
    }
}

void SceneManager::GameList_CheckUpdate()
{
    if (m_gameListStage != GameListStage::LOADING) {
        return;
    }

    // Clear the old game list
    m_gameList.clear();

    PRINT(System, INFO, "Retrieving new game list...");
    auto gameList = System::GetGameList()->GetEntries();
    PRINT(System, INFO, "Done!");

    u32 idx = 0;
    for (auto entry : gameList) {
        float posY = 140 - (float(idx) * 93.333);
        m_gameList.emplace_back(entry, 0, posY);
        idx++;
    }

    m_gameListStage = GameListStage::FADE_IN;
}

void SceneManager::GameList_Entry(void* arg)
{
    SceneManager* sceneManager = reinterpret_cast<SceneManager*>(arg);

    sceneManager->GameList_CheckUpdate();
}

void SceneManager::Button::Create(
    nw4r::lyt::Layout* layout, const char* touchPane,
    const char* anim_freeToSelect, const char* anim_selectToFree
)
{
    nw4r::lyt::ArcResourceAccessor* resAsr =
        System::GetResourceManager()->GetChannelArchive();

    assert(layout);
    m_layout = layout;

    auto layoutRootPane = m_layout->GetRootPane();
    assert(layoutRootPane);

    m_touchPane = nullptr;
    if (touchPane != nullptr) {
        m_touchPane = layoutRootPane->FindPaneByName(touchPane);
        assert(m_touchPane);
    }

    m_animFreeToSelect = nullptr;
    if (anim_freeToSelect) {
        void* anim = resAsr->GetResource(
            nw4r::lyt::ARC_TYPE_ANIM, anim_freeToSelect, nullptr
        );
        assert(anim);

        m_animFreeToSelect = m_layout->CreateAnimTransform(anim, resAsr);
        assert(m_animFreeToSelect);
    }

    m_animSelectToFree = nullptr;
    if (anim_selectToFree) {
        void* anim = resAsr->GetResource(
            nw4r::lyt::ARC_TYPE_ANIM, anim_selectToFree, nullptr
        );
        assert(anim);

        m_animSelectToFree = m_layout->CreateAnimTransform(anim, resAsr);
        assert(m_animSelectToFree);
    }
}

void SceneManager::Button::Calc()
{
    GlobalPosition touchPos = GetGlobalPanePos(m_touchPane);

    bool touch = false;

    auto pointerPos = System::GetPointerPosition();
    if (pointerPos) {
        if (pointerPos->x > (touchPos.position.x - touchPos.size.width) &&
            pointerPos->x < (touchPos.position.x + touchPos.size.width) &&
            pointerPos->y > (touchPos.position.y - touchPos.size.height) &&
            pointerPos->y < (touchPos.position.y + touchPos.size.height)) {
            touch = true;
        }
    }

    switch (m_animTag) {
    case AnimTag::FREE:
        if (touch) {
            FreeToSelect();
        }
        break;

    case AnimTag::FREE_TO_SELECT:
        if (m_animFreeToSelect) {
            if (m_animFreeToSelect->GetFrame() ==
                m_animFreeToSelect->GetFrameSize() - 1) {
                m_animTag = AnimTag::SELECT;
            }

            m_animFreeToSelect->SetFrame(m_animFreeToSelect->GetFrame() + 1);
        } else if (m_animSelectToFree) {
            // Reverse
            if (m_animSelectToFree->GetFrame() < 1) {
                m_animTag = AnimTag::SELECT;
            }

            m_animSelectToFree->SetFrame(m_animSelectToFree->GetFrame() - 1);
        }
        break;

    case AnimTag::SELECT:
        if (!touch) {
            SelectToFree();
        }
        break;

    case AnimTag::SELECT_TO_FREE:
        if (m_animSelectToFree) {
            if (m_animSelectToFree->GetFrame() ==
                m_animSelectToFree->GetFrameSize() - 1) {
                m_animTag = AnimTag::FREE;
            }

            m_animSelectToFree->SetFrame(m_animSelectToFree->GetFrame() + 1);
        } else if (m_animFreeToSelect) {
            // Reverse
            if (m_animFreeToSelect->GetFrame() < 1) {
                m_animTag = AnimTag::FREE;
            }

            m_animFreeToSelect->SetFrame(m_animFreeToSelect->GetFrame() - 1);
        }
        break;
    }
}

void SceneManager::Button::FreeToSelect()
{
    UnbindAnimation();

    m_animTag = AnimTag::FREE_TO_SELECT;

    if (m_animFreeToSelect) {
        m_animFreeToSelect->SetFrame(0);
        m_layout->BindAnimation(m_animFreeToSelect);
    } else if (m_animSelectToFree) {
        // Do the other but reverse
        m_animSelectToFree->SetFrame(m_animSelectToFree->GetFrameSize());
        m_layout->BindAnimation(m_animSelectToFree);
    } else {
        m_animTag = AnimTag::SELECT;
    }
}

void SceneManager::Button::SelectToFree()
{
    UnbindAnimation();

    m_animTag = AnimTag::SELECT_TO_FREE;

    if (m_animSelectToFree) {
        m_animSelectToFree->SetFrame(0);
        m_layout->BindAnimation(m_animSelectToFree);
    } else if (m_animFreeToSelect) {
        // Do the other but reverse
        m_animFreeToSelect->SetFrame(m_animFreeToSelect->GetFrameSize() - 1);
        m_layout->BindAnimation(m_animFreeToSelect);
    } else {
        m_animTag = AnimTag::FREE;
    }
}

void SceneManager::Button::UnbindAnimation()
{
    if (m_animFreeToSelect) {
        m_layout->UnbindAnimation(m_animFreeToSelect);
    }

    if (m_animSelectToFree) {
        m_layout->UnbindAnimation(m_animSelectToFree);
    }
}

SceneManager::LayoutGameEntry::LayoutGameEntry(
    GameList::GameEntry from, float x, float y
)
{
    m_deviceId = from.m_deviceId;
    memcpy(m_titleId, from.m_titleId, sizeof(m_titleId));
    m_revision = from.m_revision;

    nw4r::lyt::ArcResourceAccessor* resAsr =
        System::GetResourceManager()->GetChannelArchive();

    void* blyt = resAsr->GetResource(
        nw4r::lyt::ARC_TYPE_BLYT, "game_button.brlyt", nullptr
    );
    assert(blyt);

    m_buttonLayout = std::make_unique<nw4r::lyt::Layout>();

    bool result = m_buttonLayout->Build(blyt, resAsr);
    assert(result);

    m_buttonCtrl.Create(
        m_buttonLayout.get(), "Window_BG", "game_button_FreeToSelect.brlan",
        "game_button_SelectToFree.brlan"
    );

    auto buttonRootPane = m_buttonLayout->GetRootPane();
    assert(buttonRootPane);

    if (!System::IsWidescreen()) {
        buttonRootPane->SetScale(nw4r::math::VEC2(0.9, 0.9));
    }

    blyt = resAsr->GetResource(nw4r::lyt::ARC_TYPE_BLYT, "icon.brlyt", nullptr);

    m_iconLayout = std::make_unique<nw4r::lyt::Layout>();
    result = m_iconLayout->Build(blyt, resAsr);
    assert(result);

    void* anim =
        resAsr->GetResource(nw4r::lyt::ARC_TYPE_ANIM, "icon.brlan", nullptr);
    assert(anim);

    m_iconAnimation = m_iconLayout->CreateAnimTransform(anim, resAsr);
    assert(m_iconAnimation);
    m_iconLayout->BindAnimation(m_iconAnimation);

    m_bannerWindowPane = buttonRootPane->FindPaneByName("Pic_IconWindow");
    assert(m_bannerWindowPane);

    buttonRootPane->SetPosition(nw4r::math::VEC3(x, y, 0));

    auto tdbEntry = System::GetGameList()->SearchWiiTDB(from.m_titleId);
    assert(tdbEntry);

    u16 title[128];
    u32 titleLen = tdbEntry->GetTitleEN(title, 128);

    if (titleLen != 0) {
        Rgnsel::SetPaneText(m_buttonLayout.get(), "Txt_Title", title);
    }

    m_hasIcon = true;
}

} // namespace Channel
