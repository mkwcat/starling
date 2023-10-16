// System.hpp
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0

#pragma once

#include <Import_NW4R.hpp>
#include <Import_RVL_OS.h>
#include <Types.h>
#include <optional>

namespace Channel
{

class EventManager;
class GameList;
class ResourceManager;
class SceneManager;

class System
{
public:
    System() = delete;

    static void Init();
    static void Run();
    static void Shutdown();

    /**
     * Check if we're running on Dolphin Emulator.
     */
    static bool IsDolphin();

    /**
     * Returns true if the aspect ratio is 16:9.
     */
    static bool IsWidescreen();

    /**
     * Get a pointer to the EventManager instance.
     */
    static EventManager* GetEventManager();

    /**
     * Get a pointer to the ResourceManager instance.
     */
    static ResourceManager* GetResourceManager();

    /**
     * Get a pointer to the GameList instance.
     */
    static GameList* GetGameList();

    /**
     * Get a pointer to the SceneManager instance.
     */
    static SceneManager* GetSceneManager();

    /**
     * Get the projection rectangle, adjusted for aspect ratio.
     */
    static nw4r::ut::Rect GetProjectionRect();

    /**
     * Get the current pointer position.
     */
    static std::optional<nw4r::math::VEC2> GetPointerPosition();

    /**
     * Get the NW4R Layout DrawInfo struct.
     */
    static nw4r::lyt::DrawInfo* GetDrawInfo();

    /**
     * Get the current render mode.
     */
    static const GXRenderModeObj& GetCurrentRenderMode();

private:
    /**
     * Heap allocate for NW4R.
     */
    static void* NW4RAlloc(MEMAllocator* allocator, u32 size);

    /**
     * Heap free for NW4R.
     */
    static void NW4RFree(MEMAllocator* allocator, void* block);

    /**
     * Heap allocate for WPAD.
     */
    static void* WPADAlloc(u32 size);

    /**
     * Heap free for WPAD.
     */
    static s32 WPADFree(void* block);

    /**
     * Init VI and GX.
     */
    static void InitVideo();

    /**
     * Configure the video for either System or the debug console. s_rmode must
     * be set before calling.
     */
    static void ConfigureVideo(bool console);

    /**
     * Get the render mode to use for System.
     */
    static GXRenderModeObj GetRenderMode();

private:
    /**
     * System render mode.
     */
    static GXRenderModeObj s_rmode;
};

} // namespace Channel
