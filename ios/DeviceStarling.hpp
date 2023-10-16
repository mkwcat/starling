// DeviceStarling.hpp - IOS event resource manager
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <DeviceStarlingTypes.hpp>
#include <FAT.h>
#include <IOS.hpp>
#include <Types.h>
#include <variant>

class DeviceStarling
{
public:
    /**
     * DeviceStarling constructor.
     */
    DeviceStarling();

    /**
     * Start the resource manager loop.
     */
    void Run();

    /**
     * Notify the channel that a disk was inserted.
     */
    void InsertDisk(u32 deviceId);

    /**
     * Notify the channel that a disk was removed.
     */
    void RemoveDisk(u32 deviceId);

protected:
    /**
     * Handle IOCTL request from the loader.
     */
    s32 HandleIoctl(IOS::Request* req);

    /**
     * Handle request from IPC.
     */
    s32 HandleRequest(IOS::Request* req);

    struct InternalCommandData {
        DeviceStarlingTypes::Command command;

        // Data is freed after sending.
        std::variant<
            std::monostate, DeviceStarlingTypes::CommandData, const void*,
            FILINFO>
            data;
    };

    Queue<IOS::Request*> m_ipcQueue;
    Queue<IOS::Request*> m_responseQueue{1};
    Queue<InternalCommandData> m_commandQueue;

    bool m_opened = false;
    bool m_commandRequested = false;
};
