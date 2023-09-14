// EventManager.cpp
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0

#pragma once

#include <IOS/EventTypes.hpp>
#include <System/IOS.hpp>
#include <System/OS.hpp>
#include <System/Types.h>

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

    /**
     * Handle an event received from IOS.
     * @returns True if the RM is still open
     */
    bool HandleIOSEvent(EventRMReply event);

private:
    IOS::ResourceCtrl<EventRMIoctl> m_rm{EVENT_DEVICE_NAME};

    Thread m_rmThread;
    EventRMData m_rmData alignas(32);
};

} // namespace Channel
