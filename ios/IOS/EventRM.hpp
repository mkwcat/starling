// EventRM.hpp - IOS event resource manager
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <IOS/EventTypes.hpp>
#include <System/IOS.hpp>
#include <System/Types.h>

class EventRM
{
public:
    /**
     * EventRM constructor.
     */
    EventRM();

    /**
     * Start the resource manager loop.
     */
    void Run();

    /**
     * Notify the channel that a device was inserted or removed.
     */
    void NotifyDeviceUpdate(EventRMData::DeviceUpdate* param);

protected:
    /**
     * Handle request from IPC.
     */
    void HandleRequest(IOS::Request* req);

    Queue<IOS::Request*> m_ipcQueue;
    Queue<IOS::Request*> m_responseQueue;
    bool m_opened = false;
    bool m_eventRequested = false;
};
