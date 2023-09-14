// EventRM.cpp - IOS event resource manager
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "EventRM.hpp"
#include <Debug/Log.hpp>
#include <IOS/Kernel.hpp>
#include <IOS/System.hpp>
#include <cassert>
#include <cstring>

/**
 * EventRM constructor.
 */
EventRM::EventRM()
  : m_ipcQueue(8)
  , m_responseQueue(1)
{
    s32 ret =
        IOS_RegisterResourceManager(EVENT_DEVICE_NAME, m_ipcQueue.GetID());
    assert(ret >= 0);
}

/**
 * Notify the channel that a device was inserted or removed.
 */
void EventRM::NotifyDeviceUpdate(EventRMData::DeviceUpdate* param)
{
    IOS::Request* req = m_responseQueue.Receive();
    System::UnalignedMemcpy(
        req->ioctl.out, param, sizeof(EventRMData::DeviceUpdate)
    );
    req->Reply(s32(EventRMReply::DeviceUpdate));
}

/**
 * Handle request from IPC.
 */
void EventRM::HandleRequest(IOS::Request* req)
{
    switch (req->cmd) {
    case IOS::Cmd::OPEN:
        if (strcmp(req->open.path, EVENT_DEVICE_NAME) != 0) {
            req->Reply(IOS::IOSError::NOT_FOUND);
            break;
        }

        if (m_opened) {
            req->Reply(IOS::IOSError::INVALID);
            break;
        }

        m_opened = true;
        req->Reply(IOS::IOSError::OK);
        break;

    case IOS::Cmd::CLOSE:
        if (m_eventRequested) {
            // Wait for any ongoing requests to finish
            System::SleepUsec(10000);
            m_responseQueue.Receive()->Reply(s32(EventRMReply::Close));
            m_eventRequested = false;
            m_opened = false;
        }

        req->Reply(IOS::IOSError::OK);
        break;

    case IOS::Cmd::IOCTL:
        switch (static_cast<EventRMIoctl>(req->ioctl.cmd)) {
        case EventRMIoctl::RegisterEventHook:
            if (req->ioctl.out_len != sizeof(EventRMData) ||
                !IsAligned(req->ioctl.out, 32)) {
                req->Reply(IOS::IOSError::INVALID);
                break;
            }

            // Will reply on next event
            m_responseQueue.Send(req);
            m_eventRequested = true;
            break;

        case EventRMIoctl::StartGameEvent:
            // Start game IOS command
            Kernel::PatchIOSOpen();
            Log::g_viLogEnabled = false;
            req->Reply(IOS::IOSError::OK);
            break;

        case EventRMIoctl::SetTime:
            if (req->ioctl.in_len != sizeof(u32) + sizeof(u64) ||
                !IsAligned(req->ioctl.in, 4)) {
                req->Reply(IOS::IOSError::INVALID);
                break;
            }

            System::SetTime(
                *reinterpret_cast<u32*>(req->ioctl.in),
                *reinterpret_cast<u64*>(req->ioctl.in + 4)
            );
            req->Reply(IOS::IOSError::OK);
            break;

        default:
            req->Reply(IOS::IOSError::INVALID);
            break;
        }
        break;

    default:
        req->Reply(IOS::IOSError::INVALID);
        break;
    }
}

/**
 * Start the resource manager loop.
 */
void EventRM::Run()
{
    while (true) {
        IOS::Request* req = m_ipcQueue.Receive();
        HandleRequest(req);
    }
}
