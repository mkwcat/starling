#include "EventManager.hpp"
#include "System.hpp"
#include <Debug/Log.hpp>
#include <Import/RVL_OS.h>
#include <System/Hollywood.hpp>
#include <cassert>

namespace Channel
{

/**
 * EventManager constructor.
 */
EventManager::EventManager()
{
    if (System::IsDolphin()) {
        return;
    }

    if (m_rm.GetFd() == IOS::IOSError::NOT_FOUND) {
        PRINT(System, INFO, "EventRM not found, retrying...");

        do {
            OSSleepMicroseconds(5000);
            new (&m_rm) IOS::ResourceCtrl<EventRMIoctl>(EVENT_DEVICE_NAME);
        } while (m_rm.GetFd() == IOS::IOSError::NOT_FOUND);
    }

    assert(m_rm.GetFd() >= 0);
    PRINT(System, NOTICE, "EventRM opened");

    // FIXME: OSGetTime doesn't seem to return the correct time here
    EventRMTime timeInput = {
      .hwTimer = ACRReadTrusted(ACRReg::TIMER),
      .epoch = OSTicksToSeconds(OSGetTime()) + 946699200,
    };

    s32 ret = m_rm.Ioctl(
      EventRMIoctl::SetTime, &timeInput, sizeof(EventRMTime), nullptr, 0
    );
    assert(ret == IOS::IOSError::OK);
}

/**
 * EventManager destructor.
 */
EventManager::~EventManager()
{
    // Make sure this is closed first, then the Thread destructor will wait for
    // the thread to exit
    m_rm.Close();
}

/**
 * Begin dispatching events.
 */
void EventManager::Start()
{
    if (System::IsDolphin()) {
        return;
    }

    m_rmThread.Create(&RMThreadEntry, this);
}

/**
 * IOS event handling thread.
 */
void EventManager::RMThreadEntry(void* arg)
{
    EventManager* mgr = reinterpret_cast<EventManager*>(arg);

    while (true) {
        s32 result = mgr->m_rm.Ioctl(
          EventRMIoctl::RegisterEventHook, nullptr, 0, &mgr->m_rmData,
          sizeof(mgr->m_rmData)
        );

        if (result < 0) {
            PRINT(System, ERROR, "Received error from event hook: %d", result);
            break;
        }

        bool open = mgr->HandleIOSEvent(static_cast<EventRMReply>(result));

        if (!open) {
            break;
        }
    }
}

/**
 * Handle an event received from IOS.
 * @returns True if the RM is still open
 */
bool EventManager::HandleIOSEvent(EventRMReply event)
{
    switch (event) {
    case EventRMReply::Close: {
        PRINT(System, INFO, "Closing EventRM thread");
        return false;
    }

    case EventRMReply::DeviceUpdate: {
        PRINT(System, INFO, "Received device update event");
        return true;
    }

    default:
        PRINT(
          System, ERROR, "Received unknown event: %d", static_cast<s32>(event)
        );
        return false;
    }
}

} // namespace Channel
