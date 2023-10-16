// EventManager.cpp
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0

#pragma once

#include <OS.hpp>
#include <Types.h>

namespace Channel
{

/**
 * Frontend for classes to handle events.
 */
class EventHandler
{
    virtual void Event_DeviceInsertion([[maybe_unused]] u32 id)
    {
    }

    virtual void Event_DeviceRemoval([[maybe_unused]] u32 id)
    {
    }
};

/**
 * Manages signaling various I/O events, such as device insertion or removal.
 */
class EventManager
{
public:
    /**
     * EventManager constructor.
     */
    EventManager();

    /**
     * EventManager destructor.
     */
    ~EventManager();

    /**
     * Begin dispatching events.
     */
    void Start();

private:
    /**
     * IOS event handling thread.
     */
    static void RMThreadEntry(void* arg);

private:
    bool m_open = false;
    Thread m_rmThread;
};

} // namespace Channel
