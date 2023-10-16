// System.cpp
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0

#include "System.hpp"
#include "EventManager.hpp"
#include "GameList.hpp"
#include "Heap.hpp"
#include "ResourceManager.hpp"
#include "SceneManager.hpp"
#include <AddressMap.h>
#include <Console.hpp>
#include <Import_Rgnsel.hpp>
#include <Log.hpp>

namespace Channel
{

static EventManager* s_iEventManager;
static GameList* s_iGameList;
static ResourceManager* s_iResourceManager;
static SceneManager* s_iSceneManager;

static void* s_XFBs[2];
static void* s_currXFB;

static nw4r::lyt::DrawInfo s_drawInfo;

enum {
    AR_4_3,
    AR_16_9,
};

static u8 s_aspectRatio = AR_16_9;

/**
 * System render mode.
 */
GXRenderModeObj System::s_rmode = {};

static bool s_debugVI = false;

void System::Init()
{
    s_iEventManager = new EventManager();
    s_iResourceManager = new ResourceManager();

    WPADRegisterAllocator(&System::WPADAlloc, &System::WPADFree);
    KPADInit();

    for (u32 i = 0; i < 4; i++) {
        KPADSetPosParam(i, 0.05, 1.0);
    }

    // AXInit();
    InitVideo();

    // Init NW4R LYT

    static MEMAllocatorFunc allocFunc = {
        .alloc = &System::NW4RAlloc,
        .free = &System::NW4RFree,
    };

    static MEMAllocator alloc = {};
    alloc.functions = &allocFunc;

    nw4r::lyt::Layout::SetAllocator(&alloc);

    s_iGameList = new GameList();
    s_iSceneManager = new SceneManager();

    s_iEventManager->Start();
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

    return {
        .position = position,
        .size =
            {
                .width = sizeVec1.x,
                .height = sizeVec1.y,
            },
    };
}

static bool s_pointerValid = false;
static nw4r::math::VEC2 s_pointerPosition;

void System::Run()
{
    nw4r::lyt::ArcResourceAccessor* resAsr =
        s_iResourceManager->GetChannelArchive();

    Rgnsel::Pointer m_pointer;
    m_pointer.Init(resAsr);

    bool firstFrame = true;

    u8 pointerMissFrame[4] = {0, 0, 0, 0};

    while (true) {
        KPADStatus status;
        s32 rc = KPADRead(0, &status, 1);
        if (rc >= 1 && status.posValid >= 1) {
            pointerMissFrame[0] = 3;

            KPADVec2D projPos;
            auto rect = GetProjectionRect();
            KPADGetProjectionPos(&projPos, &status.pos, rect.f, 1.10132);

            if (s_aspectRatio == AR_16_9) {
                projPos.x *= 1.15;
                projPos.y *= 1.15;
            }

            if (projPos.x < rect.left - 100) {
                projPos.x = rect.left - 100;
            }

            if (projPos.x > rect.right + 100) {
                projPos.x = rect.right + 100;
            }

            if (projPos.y < rect.bottom - 100) {
                projPos.y = rect.bottom - 100;
            }

            if (projPos.y > rect.top + 100) {
                projPos.y = rect.top + 100;
            }

            m_pointer.SetEnabled(0);
            m_pointer.Calc(
                0, &s_drawInfo, -projPos.x, projPos.y,
                nw4r::math::Atan2FIdx(-status.angle.y, status.angle.x) * 1.40625
            );

            s_pointerValid = true;
            s_pointerPosition = nw4r::math::VEC2(-projPos.x, projPos.y);
        } else {
            if (pointerMissFrame[0] == 0) {
                m_pointer.SetDisabled(0);
                s_pointerValid = false;
            } else {
                pointerMissFrame[0]--;
            }
        }

        GXInvalidateVtxCache();
        GXInvalidateTexAll();

        float mtx[4][4];
        auto rect = GetProjectionRect();
        MTXOrtho(mtx, rect.top, rect.bottom, rect.left, rect.right, 0, 500);

        float mtx2[3][4];
        MTXIdentity(mtx2);

        GXSetProjection(mtx, 1);
        GXLoadPosMtxImm(mtx2, 0);

        GXSetCurrentMtx(0);

        GXSetLineWidth(6, 0);
        GXSetPointSize(6, 0);
        GXSetCullMode(0);
        GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);

        // Handles rendering the entire UI
        s_iSceneManager->Tick();

        m_pointer.Draw(&s_drawInfo);

        if (rc >= 1 && (status.trigger & WPAD_BUTTON_1)) {
            ConfigureVideo(true);
        }

        if (rc >= 1 && (status.release & WPAD_BUTTON_1)) {
            ConfigureVideo(false);
        }

        GXColor color = {
            .rgba = 0xA9A9A900,
        };
        GXSetCopyClear(color, 0xFFFFFF);
        GXSetZMode(1, 7, 1);

        GXSetAlphaUpdate(1);
        GXSetColorUpdate(1);

        GXCopyDisp(s_currXFB, 1);
        GXDrawDone();

        if (!s_debugVI) {
            VISetNextFrameBuffer(s_currXFB);
        }

        if (firstFrame) {
            VISetBlack(false);
            firstFrame = false;
        }

        // Flush VI data and wait for the frame to complete
        VIFlush();
        VIWaitForRetrace();

        // Swap framebuffer
        s_currXFB = s_currXFB == s_XFBs[0] ? s_XFBs[1] : s_XFBs[0];

        if (rc >= 1 && (status.trigger & WPAD_BUTTON_HOME)) {
            break;
        }
    }
}

/**
 * Destroy everything to exit the program.
 */
void System::Shutdown()
{
    delete s_iEventManager;
    s_iEventManager = nullptr;
}

/**
 * Get a pointer to the EventManager instance.
 */
EventManager* System::GetEventManager()
{
    return s_iEventManager;
}

/**
 * Get a pointer to the ResourceManager instance.
 */
ResourceManager* System::GetResourceManager()
{
    return s_iResourceManager;
}

/**
 * Get a pointer to the GameList instance.
 */
GameList* System::GetGameList()
{
    return s_iGameList;
}

/**
 * Get a pointer to the SceneManager instance.
 */
SceneManager* System::GetSceneManager()
{
    return s_iSceneManager;
}

/**
 * Get the NW4R Layout DrawInfo struct.
 */
nw4r::lyt::DrawInfo* System::GetDrawInfo()
{
    return &s_drawInfo;
}

/**
 * Get the projection rectangle, adjusted for aspect ratio.
 */
nw4r::ut::Rect System::GetProjectionRect()
{
    if (s_aspectRatio == AR_16_9) {
        return nw4r::ut::Rect(-416, 228, 416, -228);
    } else {
        return nw4r::ut::Rect(-304, 228, 304, -228);
    }
}

/**
 * Get the current pointer position.
 */
std::optional<nw4r::math::VEC2> System::GetPointerPosition()
{
    if (s_pointerValid) {
        return s_pointerPosition;
    }

    return {};
}

/**
 * Get the current render mode.
 */
const GXRenderModeObj& System::GetCurrentRenderMode()
{
    return s_rmode;
}

/**
 * Check if we're running on Dolphin Emulator.
 */
bool System::IsDolphin()
{
    static bool s_ranCheck = false;
    static bool s_isDolphin = false;

    if (s_ranCheck) {
        return s_isDolphin;
    }

    s_ranCheck = true;

    // Modern versions
    s32 fd = IOS_Open("/dev/dolphin", 0);
    if (fd >= 0) {
        IOS_Close(fd);
        s_isDolphin = true;
        return s_isDolphin;
    }

    // Older versions
    fd = IOS_Open("/dev/sha", 0);
    if (fd == -6) {
        s_isDolphin = true;
        return s_isDolphin;
    }

    if (fd >= 0) {
        IOS_Close(fd);
    }

    s_isDolphin = false;
    return s_isDolphin;
}

/**
 * Returns true if the aspect ratio is 16:9.
 */
bool System::IsWidescreen()
{
    return s_aspectRatio == AR_16_9;
}

/**
 * Heap allocate for NW4R.
 */
void* System::NW4RAlloc(MEMAllocator* allocator, u32 size)
{
    (void) allocator;

    u8* data = new u8[size];
    return reinterpret_cast<void*>(data);
}

/**
 * Heap free for NW4R.
 */
void System::NW4RFree(MEMAllocator* allocator, void* block)
{
    (void) allocator;

    u8* data = reinterpret_cast<u8*>(block);
    delete[] data;
}

/**
 * Heap allocate for WPAD.
 */
void* System::WPADAlloc(u32 size)
{
    return Heap::AllocMEM2(size);
}

/**
 * Heap free for WPAD.
 */
s32 System::WPADFree(void* block)
{
    Heap::FreeMEM2(block);
    return 1;
}

/**
 * Init VI and GX.
 */
void System::InitVideo()
{
    VIInit();
    VISetBlack(true);

    SetGQR(2, 0x40004);
    SetGQR(3, 0x50005);
    SetGQR(4, 0x60006);
    SetGQR(5, 0x70007);

    s_rmode = GetRenderMode();

    // Allocate XFB
    for (u32 i = 0; i < 2; i++) {
        s_XFBs[i] = Heap::AllocMEM2(
            AlignUp(s_rmode.fbWidth, 0x10) * s_rmode.xfbHeight * 2, 32
        );
        assert(s_XFBs[i] != nullptr);
    }
    s_currXFB = s_XFBs[0];

    ConfigureVideo(false);

    void* data = Heap::AllocMEM1(0x80000, 32);
    assert(data != nullptr);
    GXInit(data, 0x80000);

    GXSetViewport(0, 0, s_rmode.fbWidth, s_rmode.efbHeight, 0, 1);
    GXSetScissor(0, 0, s_rmode.fbWidth, s_rmode.efbHeight);

    float factor = GXGetYScaleFactor(s_rmode.efbHeight, s_rmode.xfbHeight);
    u16 lines = GXSetDispCopyYScale(factor);

    GXSetDispCopySrc(0, 0, s_rmode.fbWidth, s_rmode.xfbHeight);
    GXSetDispCopyDst(s_rmode.fbWidth, lines);
    GXSetCopyFilter(s_rmode.aa, s_rmode.sample[0], 0, s_rmode.vertFilter);
    GXSetPixelFmt(0, 0);

    GXSetViewport(0, 0, s_rmode.fbWidth, s_rmode.efbHeight, 0, 1);

    nw4r::math::MTX34 viewMtx;
    MTXIdentity(viewMtx.mtx);
    s_drawInfo.SetViewMtx(viewMtx);
    s_drawInfo.SetViewRect(GetProjectionRect());
}

/**
 * Configure the video for either System or the debug console. s_rmode must
 * be set before calling.
 */
void System::ConfigureVideo(bool console)
{
    if (console) {
        Console::ConfigureVideo();
        s_debugVI = true;
        return;
    }

    VIConfigure(&s_rmode);
    s_debugVI = false;
}

/**
 * Get the render mode to use for System.
 */
GXRenderModeObj System::GetRenderMode()
{
    GXRenderModeObj rmode = {};

    bool dtv = VIGetDTVStatus() != 0;
    bool pal60 = SCGetEuRgb60Mode();
    bool progressive = SCGetProgressiveMode();
    u8 aspect = SCGetAspectRatio();
    u32 mode = VIGetTvFormat();

    s_aspectRatio = aspect;

    switch (mode) {
    case VI_NTSC:
    default:
        if (dtv && progressive) {
            // NTSC 480p
            rmode = {
                .tvMode = (VI_NTSC << 2) | 2,
                .fbWidth = 608,
                .efbHeight = 456,
                .xfbHeight = 456,
                .viX = aspect ? 17 : 25,
                .viY = 12,
                .viWidth = aspect ? 686 : 670,
                .viHeight = 456,
                .viXFB = 0,
                .field = 0,
                .aa = 0,
                .sample = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                           6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
                .vertFilter = {0, 0, 21, 22, 21, 0, 0},
            };
            break;
        } else {
            // NTSC 480i
            rmode = {
                .tvMode = VI_NTSC << 2,
                .fbWidth = 608,
                .efbHeight = 456,
                .xfbHeight = 456,
                .viX = aspect ? 17 : 25,
                .viY = 12,
                .viWidth = aspect ? 686 : 670,
                .viHeight = 456,
                .viXFB = 1,
                .field = 0,
                .aa = 0,
                .sample = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                           6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
                .vertFilter = {7, 7, 12, 12, 12, 7, 7},
            };
            break;
        }

    case VI_PAL:
    case VI_EURGB60:
        if (dtv && progressive) {
            // PAL60 progressive
            rmode = {
                .tvMode = (VI_EURGB60 << 2) | 2,
                .fbWidth = 608,
                .efbHeight = 456,
                .xfbHeight = 456,
                .viX = aspect ? 17 : 25,
                .viY = 12,
                .viWidth = aspect ? 686 : 670,
                .viHeight = 456,
                .viXFB = 0,
                .field = 0,
                .aa = 0,
                .sample = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                           6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
                .vertFilter = {0, 0, 21, 22, 21, 0, 0},
            };
            break;
        }

        if (pal60) {
            // PAL60 interlaced
            rmode = {
                .tvMode = VI_EURGB60 << 2,
                .fbWidth = 608,
                .efbHeight = 456,
                .xfbHeight = 456,
                .viX = aspect ? 17 : 25,
                .viY = 12,
                .viWidth = aspect ? 686 : 670,
                .viHeight = 456,
                .viXFB = 1,
                .field = 0,
                .aa = 0,
                .sample = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                           6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
                .vertFilter = {7, 7, 12, 12, 12, 7, 7},
            };
            break;
        } else {
            // PAL50 interlaced
            rmode = {
                .tvMode = VI_PAL << 2,
                .fbWidth = 608,
                .efbHeight = 456,
                .xfbHeight = 542,
                .viX = aspect ? 19 : 27,
                .viY = 16,
                .viWidth = aspect ? 682 : 666,
                .viHeight = 542,
                .viXFB = 1,
                .field = 0,
                .aa = 0,
                .sample = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                           6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
                .vertFilter = {7, 7, 12, 12, 12, 7, 7},
            };
            break;
        }

    case VI_MPAL:
        if (dtv && progressive) {
            // PAL-M 480p
            rmode = {
                .tvMode = (VI_MPAL << 2) | 2,
                .fbWidth = 608,
                .efbHeight = 456,
                .xfbHeight = 456,
                .viX = aspect ? 17 : 25,
                .viY = 12,
                .viWidth = aspect ? 686 : 670,
                .viHeight = 456,
                .viXFB = 0,
                .field = 0,
                .aa = 0,
                .sample = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                           6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
                .vertFilter = {0, 0, 21, 22, 21, 0, 0},
            };
            break;
        } else {
            // PAL-M 480i
            rmode = {
                .tvMode = VI_MPAL << 2,
                .fbWidth = 608,
                .efbHeight = 456,
                .xfbHeight = 456,
                .viX = aspect ? 17 : 25,
                .viY = 12,
                .viWidth = aspect ? 686 : 670,
                .viHeight = 456,
                .viXFB = 1,
                .field = 0,
                .aa = 0,
                .sample = {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                           6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
                .vertFilter = {7, 7, 12, 12, 12, 7, 7},
            };
            break;
        }
    }

    return rmode;
}

} // namespace Channel
