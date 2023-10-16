// DeviceStarling.cpp - Starling IOS resource manager
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "DeviceStarling.hpp"
#include <DiskManager.hpp>
#include <Kernel.hpp>
#include <Log.hpp>
#include <System.hpp>
#include <Util.h>
#include <array>
#include <cassert>
#include <cstring>

/**
 * DeviceStarling constructor.
 */
DeviceStarling::DeviceStarling()
{
    s32 ret = IOS_RegisterResourceManager(
        DeviceStarlingTypes::RM_PATH, m_ipcQueue.GetID()
    );
    assert(ret >= 0);
}

/**
 * Notify the loader that a disk was inserted.
 */
void DeviceStarling::InsertDisk(u32 diskId)
{
    // Notify the loader that the disk was inserted and select it.
    m_commandQueue.Send({
        .command = DeviceStarlingTypes::Command::SELECT_DISK,
        .data = DeviceStarlingTypes::CommandData::FromDiskID(diskId),
    });

    u32 drv = DiskManager::DevIDToDrv(diskId);

    static const char* const s_scanDirs[] = {
        "0:/riivolution",
        "0:/apps/riivolution",
    };

    for (u32 i = 0; i < std::size(s_scanDirs); i++) {
        char scanDir[64] = {0};
        strncpy(scanDir, s_scanDirs[i], sizeof(scanDir));
        scanDir[0] = '0' + drv;

        DIR dir;
        auto fret = f_opendir(&dir, scanDir);
        if (fret != FR_OK) {
            PRINT(IOS, INFO, "Couldn't open '%s'", scanDir, fret);
            continue;
        }

        PRINT(IOS, INFO, "Scanning '%s'", scanDir);

        FILINFO info;
        while ((fret = f_readdir(&dir, &info)) == FR_OK) {
            if (!StrNoCaseEndsWith(info.fname, ".xml")) {
                continue;
            }

            // Send the Riivolution XML to the loader.
            m_commandQueue.Send({
                .command = DeviceStarlingTypes::Command::INSERT_RIIVOLUTION_XML,
                .data = info,
            });
        }
    }
}

static constexpr s32 NO_REPLY = -99;

/**
 * Handle IOCTL request from the loader.
 */
s32 DeviceStarling::HandleIoctl(IOS::Request* request)
{
    auto command = DeviceStarlingTypes::Ioctl(request->ioctl.cmd);

    u32 inLen = request->ioctl.in_len;
    [[maybe_unused]] const void* in = inLen != 0 ? request->ioctl.in : nullptr;
    u32 outLen = request->ioctl.out_len;
    [[maybe_unused]] void* out = outLen != 0 ? request->ioctl.out : nullptr;

    switch (command) {
    case DeviceStarlingTypes::Ioctl::RECEIVE_COMMAND: {
        if (outLen != COMMAND_DATA_MAXLEN) {
            PRINT(IOS, ERROR, "RECEIVE_COMMAND: Invalid output length");
            return IOS::IOSError::INVALID;
        }

        if (!IsAligned(out, 32)) {
            PRINT(IOS, ERROR, "RECEIVE_COMMAND: Invalid output alignment");
            return IOS::IOSError::INVALID;
        }

        // Enqueue the request to reply on the next command
        m_responseQueue.Send(request);
        m_commandRequested = true;
        // Internal code to skip replying
        return NO_REPLY;
    }

    case DeviceStarlingTypes::Ioctl::START_GAME: {
        PRINT(IOS, INFO, "START_GAME from loader");
        Kernel::PatchIOSOpen();
        Log::g_viLogEnabled = false;
        return IOS::IOSError::INVALID;
    }

    default:
        PRINT(
            IOS, ERROR, "Received invalid IOCTL for /dev/starling: %d",
            request->ioctl.cmd
        );
        return IOS::IOSError::INVALID;
    }
}

/**
 * Handle IPC request from the loader.
 */
s32 DeviceStarling::HandleRequest(IOS::Request* request)
{
    switch (request->cmd) {
    case IOS::Cmd::OPEN: {
        if (strcmp(request->open.path, DeviceStarlingTypes::RM_PATH) != 0) {
            return IOS::IOSError::NOT_FOUND;
        }

        if (m_opened) {
            PRINT(
                IOS, ERROR,
                "Attempt to open more than one instance of /dev/starling"
            );
            return IOS::IOSError::INVALID;
        }

        m_opened = true;
        return 0;
    }

    case IOS::Cmd::CLOSE: {
        PRINT(IOS, INFO, "Loader closed /dev/starling");

        if (m_commandRequested) {
            m_responseQueue.Receive()->Reply(
                s32(DeviceStarlingTypes::Command::CLOSE_REPLY)
            );
            m_commandRequested = false;
        }

        m_opened = false;
        return IOS::IOSError::OK;
    }

    case IOS::Cmd::IOCTL:
        return HandleIoctl(request);

    default:
        PRINT(
            IOS, ERROR, "Received invalid IPC command for /dev/starling: %d",
            request->cmd
        );
        return IOS::IOSError::INVALID;
    }
}

/**
 * Start the resource manager loop.
 */
void DeviceStarling::Run()
{
    while (true) {
        IOS::Request* request = m_ipcQueue.Receive();
        s32 result = HandleRequest(request);

        if (result != NO_REPLY) {
            request->Reply(result);
        }
    }
}
