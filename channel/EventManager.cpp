#include "EventManager.hpp"
#include "System.hpp"
#include <DeviceStarlingTypes.hpp>
#include <HWReg/ACR.hpp>
#include <Import_RVL_OS.h>
#include <Log.hpp>
#include <StarlingIOS.hpp>
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

    StarlingIOS::RMOpen();
    m_open = true;

    PRINT(System, NOTICE, "Starling RM opened");
}

/**
 * EventManager destructor.
 */
EventManager::~EventManager()
{
    // Make sure this is closed first, then the Thread destructor will wait for
    // the thread to exit
    m_open = false;
    StarlingIOS::RMClose();
}

/**
 * Begin dispatching events.
 */
void EventManager::Start()
{
    if (System::IsDolphin()) {
        return;
    }

    // Why would you run this if it's closed?
    assert(m_open);

    if (m_open) {
        m_rmThread.Create(&RMThreadEntry, this);
    }
}

/**
 * IOS event handling thread.
 */
void EventManager::RMThreadEntry(void* arg)
{
    EventManager* mgr = reinterpret_cast<EventManager*>(arg);

    while (mgr->m_open) {
        StarlingIOS::RMHandleCommands();
    }
}

} // namespace Channel
