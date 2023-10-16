// DiskManager.cpp - I/O storage device manager
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "DiskManager.hpp"
#include "Config.hpp"
#include "DeviceStarling.hpp"
#include "SDCard.hpp"
#include "System.hpp"
#include <Console.hpp>
#include <Log.hpp>
#include <Types.h>
#include <cstdio>

DiskManager* DiskManager::s_instance;

DiskManager::DiskManager()
{
    m_logEnabled = false;

    // 64 ms repeating timer
    m_timer = IOS_CreateTimer(0, 64000, m_timerQueue.GetID(), 0);
    assert(m_timer >= 0);

    USB::s_instance = new USB(0);
    auto ret = USB::s_instance->Init();
    assert(ret);

    // Reset everything to default.
    for (u32 i = 0; i < DeviceCount; i++) {
        InitHandle(i);
    }

    for (u32 i = 0; i < USB::MaxDevices; i++) {
        m_usbDevices[i].inUse = false;
    }

    m_devices[0].disk.emplace<SDCard>();
    m_devices[0].enabled = true;

    m_thread.create(
        ThreadEntry, reinterpret_cast<void*>(this), nullptr, 0x2000, 40
    );
}

bool DiskManager::IsInserted(u32 devId)
{
    assert(devId < DeviceCount);

    return m_devices[devId].inserted & !m_devices[devId].error;
}

bool DiskManager::IsMounted(u32 devId)
{
    assert(devId < DeviceCount);

    return IsInserted(devId) && m_devices[devId].mounted;
}

void DiskManager::SetError(u32 devId)
{
    assert(devId < DeviceCount);

    m_devices[devId].error = true;
}

FATFS* DiskManager::GetFilesystem(u32 devId)
{
    assert(devId < DeviceCount);

    return &m_devices[devId].fs;
}

void DiskManager::ForceUpdate()
{
    m_timerQueue.Send(0);
}

bool DiskManager::IsLogEnabled()
{
    if (!m_logEnabled)
        return false;

    if (!IsMounted(m_logDevice))
        return false;

    if (m_devices[m_logDevice].error) {
        return false;
    }

    if (!std::get<SDCard>(m_devices[m_logDevice].disk).IsInserted()) {
        return false;
    }

    return true;
}

void DiskManager::WriteToLog(const char* str, u32 len)
{
    if (!IsLogEnabled())
        return;

    UINT bw = 0;
    f_write(&m_logFile, str, len, &bw);
    f_sync(&m_logFile);
}

bool DiskManager::DeviceInit(u32 devId)
{
    assert(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        SDCard& disk = std::get<SDCard>(dev->disk);
        if (disk.Init())
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "SDCard::Init failed");
        return false;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        USBStorage& disk = std::get<USBStorage>(dev->disk);
        if (disk.Init())
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "USBStorage::Init failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

bool DiskManager::DeviceRead(u32 devId, void* data, u32 sector, u32 count)
{
    assert(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        SDCard& disk = std::get<SDCard>(dev->disk);
        if (disk.ReadSectors(sector, count, data))
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "SDCard::ReadSectors failed");
        return false;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        USBStorage& disk = std::get<USBStorage>(dev->disk);
        if (disk.ReadSectors(sector, count, data))
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "USBStorage::ReadSectors failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

bool DiskManager::DeviceWrite(
    u32 devId, const void* data, u32 sector, u32 count
)
{
    assert(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        SDCard& disk = std::get<SDCard>(dev->disk);
        if (disk.WriteSectors(sector, count, data))
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "SDCard::WriteSectors failed");
        return false;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        USBStorage& disk = std::get<USBStorage>(dev->disk);
        if (disk.WriteSectors(sector, count, data))
            return true;

        SetError(devId);
        PRINT(IOS_DevMgr, ERROR, "USBStorage::WriteSectors failed");
        return false;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

bool DiskManager::DeviceSync(u32 devId)
{
    assert(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled || dev->error) {
        PRINT(IOS_DevMgr, ERROR, "Device not enabled: %u", devId);
        return false;
    }

    if (std::holds_alternative<SDCard>(dev->disk)) {
        return true;
    }

    if (std::holds_alternative<USBStorage>(dev->disk)) {
        return true;
    }

    PRINT(IOS_DevMgr, ERROR, "Device not recognized: %u", devId);
    return false;
}

void DiskManager::Run()
{
    PRINT(IOS_DevMgr, INFO, "Entering DiskManager...");
    PRINT(IOS_DevMgr, INFO, "DiskManager thread ID: %d", IOS_GetThreadId());

    auto usbDevices = (USB::DeviceEntry*) IOS::Alloc(
        sizeof(USB::DeviceEntry) * USB::MaxDevices
    );
    IOS::Request usbReq = {};
    if (!USB::s_instance->EnqueueDeviceChange(
            usbDevices, &m_timerQueue, &usbReq
        ))
        USBFatal();

    while (true) {
        // Wait for 64 ms.
        auto req = m_timerQueue.Receive();

        if (req == &usbReq) {
            PRINT(IOS_DevMgr, INFO, "USB device change");
            assert(req->cmd == IOS::Cmd::REPLY);

            u32 count = req->result;
            USBChange(usbDevices, count);
            usbReq = {};
            if (!USB::s_instance->EnqueueDeviceChange(
                    usbDevices, &m_timerQueue, &usbReq
                ))
                USBFatal();
        }

        for (u32 i = 0; i < DeviceCount; i++) {
            UpdateHandle(i);
        }
    }
}

s32 DiskManager::ThreadEntry(void* arg)
{
    DiskManager* that = reinterpret_cast<DiskManager*>(arg);
    that->Run();

    return 0;
}

void DiskManager::USBFatal()
{
    assert(!"USBFatal() was called!");
}

void DiskManager::USBChange(USB::DeviceEntry* devices, u32 count)
{
    if (count > USB::MaxDevices) {
        PRINT(IOS_DevMgr, ERROR, "USB GetDeviceChange error: %d", (s32) count);
        USBFatal();
        return;
    }

    // Scan for device changes
    bool foundMap[USB::MaxDevices] = {};

    for (u32 i = 0; i < USB::MaxDevices; i++) {
        if (!m_usbDevices[i].inUse)
            continue;

        // Invalidate device's intId if it errored
        u32 intId = m_usbDevices[i].intId;
        if (intId < DeviceCount && m_devices[intId].error)
            m_usbDevices[i].intId = DeviceCount;

        u32 j = 0;
        // Sometimes the first 16 bits "device index?" will change
        while (j < count && (m_usbDevices[i].usbId & 0xFFFF) !=
                                (devices[j].devId & 0xFFFF)) {
            j++;
        }

        if (j < count) {
            foundMap[j] = true;
            continue;
        }

        PRINT(
            IOS_DevMgr, INFO, "Device with id %X was removed",
            m_usbDevices[i].usbId
        );

        // Set device to not inserted
        if (intId < DeviceCount)
            m_devices[intId].inserted = false;

        m_usbDevices[i].inUse = false;
    }

    // Search for new devices
    for (u32 i = 0; i < count; i++) {
        if (foundMap[i] == true)
            continue;

        PRINT(
            IOS_DevMgr, INFO, "Device with id %X was added", devices[i].devId
        );

        // Search for an open handle
        u32 j = 0;
        for (; j < USB::MaxDevices; j++) {
            if (!m_usbDevices[j].inUse)
                break;
        }
        assert(j < USB::MaxDevices);

        m_usbDevices[j].inUse = true;
        m_usbDevices[j].usbId = devices[i].devId;
        m_usbDevices[j].intId = DeviceCount; // Invalid

        if (USB::s_instance->Attach(devices[i].devId) != USB::USBError::OK) {
            PRINT(
                IOS_DevMgr, ERROR, "Failed to attach device %X",
                devices[i].devId
            );
            continue;
        }

        USB::DeviceInfo info;
        u8 alt = 0;
        for (; alt < devices[i].altSetCount; alt++)
            if (USB::s_instance->GetDeviceInfo(devices[i].devId, &info, alt) ==
                USB::USBError::OK)
                break;
        if (alt == devices[i].altSetCount) {
            PRINT(
                IOS_DevMgr, ERROR, "Failed to get info from device %X",
                devices[i].devId
            );
            continue;
        }
        assert(info.devId == devices[i].devId);

        if (info.interface.ifClass != USB::ClassCode::MassStorage ||
            info.interface.ifSubClass != USB::SubClass::MassStorage_SCSI ||
            info.interface.ifProtocol != USB::Protocol::MassStorage_BulkOnly) {
            PRINT(
                IOS_DevMgr, WARN,
                "USB device is not a (compatible) storage device (%X:%X:%X)",
                info.interface.ifClass, info.interface.ifSubClass,
                info.interface.ifProtocol
            );
            continue;
        }

        // Find open device ID
        u32 k = 0;
        for (; k < DeviceCount; k++) {
            if (!m_devices[k].enabled)
                break;
        }
        if (k >= DeviceCount) {
            PRINT(IOS_DevMgr, ERROR, "No open devices available");
            continue;
        }

        PRINT(IOS_DevMgr, INFO, "Using device %u", k);

        auto dev = &m_devices[k];
        dev->disk = USBStorage(USB::s_instance, info);

        m_usbDevices[j].intId = k;
        dev->inserted = true;
        dev->error = false;
        dev->mounted = false;
        dev->enabled = true;
    }
}

void DiskManager::InitHandle(u32 devId)
{
    assert(devId < DeviceCount);

    m_devices[devId].enabled = false;
    m_devices[devId].inserted = false;
    m_devices[devId].error = false;
    m_devices[devId].mounted = false;
}

void DiskManager::UpdateHandle(u32 devId)
{
    assert(devId < DeviceCount);
    DeviceHandle* dev = &m_devices[devId];

    if (!dev->enabled)
        return;

    if (std::holds_alternative<SDCard>(dev->disk)) {
        dev->inserted = std::get<SDCard>(dev->disk).IsInserted();
    }

    // Clear error if the device has been ejected, so we can try again if it's
    // reinserted.
    if (!dev->inserted)
        dev->error = false;

    if (!dev->inserted && dev->mounted) {
        // Disable file log if it was writing to this device
        if (m_logEnabled && std::holds_alternative<SDCard>(dev->disk)) {
            m_logEnabled = false;
            m_logDevice = DeviceCount;
        }

        PRINT(IOS_DevMgr, INFO, "Unmount device %d", devId);

        dev->error = false;
        dev->mounted = false;

        // Create drv str.
        char str[16] = "0:";
        str[0] = devId + '0';

        FRESULT fret = f_unmount(str);
        if (fret != FR_OK) {
            PRINT(
                IOS_DevMgr, ERROR, "Failed to unmount device %d: %d", devId,
                fret
            );
            dev->error = true;
            return;
        }

        PRINT(IOS_DevMgr, INFO, "Successfully unmounted device %d", devId);

        if (std::holds_alternative<USBStorage>(dev->disk)) {
            dev->enabled = false;
        }

        // System::GetEventRM()->NotifyDeviceRemoval(devId);
    }

    if (dev->inserted && !dev->mounted && !dev->error) {
        // Mount the device.
        PRINT(IOS_DevMgr, INFO, "Mount device %d", devId);

        dev->error = false;

        // Create drv str.
        char str[16] = "0:";
        str[0] = devId + '0';

        FRESULT fret = f_mount(&dev->fs, str, 0);
        if (fret != FR_OK) {
            PRINT(
                IOS_DevMgr, ERROR, "Failed to mount device %d: %d", devId, fret
            );
            dev->error = true;
            dev->enabled = false;
            return;
        }

        PRINT(IOS_DevMgr, INFO, "Successfully mounted device %d", devId);

        dev->mounted = true;
        dev->error = false;

        // System::GetEventRM()->NotifyDeviceInsertion(devId);

        // Open log file if it's enabled. By notifying the channel first, the
        // system time should be set by now.
        if (!m_logEnabled && Config::s_instance->IsFileLogEnabled() &&
            std::holds_alternative<SDCard>(dev->disk)) {
            m_logDevice = devId;
            OpenLogFile();
        }
    }
}

bool DiskManager::OpenLogFile()
{
    PRINT(IOS_DevMgr, INFO, "Opening log file");

    char path[16] = "0:log.txt";
    path[0] = m_logDevice + '0';

    auto fret = f_open(&m_logFile, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(IOS_DevMgr, ERROR, "Failed to open log file: %d", fret);
        return false;
    }

    m_logEnabled = true;

    PRINT(IOS_DevMgr, INFO, "Log file opened");
    return true;
}
