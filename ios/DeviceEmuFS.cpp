// DeviceEmuFS.cpp - Emulated IOS filesystem RM
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "DeviceEmuFS.hpp"
#include <Config.hpp>
#include <DeviceStarling.hpp>
#include <FAT.h>
#include <IOS.hpp>
#include <ISFSTypes.hpp>
#include <Log.hpp>
#include <SDCard.hpp>
#include <System.hpp>
#include <Types.h>
#include <Util.h>
#include <array>
#include <climits>
#include <cstdio>
#include <cstring>
#include <new>
#include <variant>

// Must be single path depth
#define EMUFS_MOUNT_POINT "/mnt"
#define EMUFS_SD0 "/mnt/sd"
#define EMUFS_USB0 "/mnt/usb0"
#define EMUFS_USB1 "/mnt/usb1"
#define EMUFS_USB2 "/mnt/usb2"
#define EMUFS_USB3 "/mnt/usb3"
#define EMUFS_USB4 "/mnt/usb4"
#define EMUFS_USB5 "/mnt/usb5"
#define EMUFS_USB6 "/mnt/usb6"
#define EMUFS_USB7 "/mnt/usb7"
#define EMUFS_MNT_WILDCARD "/mnt/*"

constexpr const char* MOUNT_POINTS[] = {
    EMUFS_SD0,  EMUFS_USB0, EMUFS_USB1, EMUFS_USB2, EMUFS_USB3,
    EMUFS_USB4, EMUFS_USB5, EMUFS_USB6, EMUFS_USB7,
};

static char s_efsPath[ISFS::EMUFS_MAX_PATH_LENGTH];
static char s_efsPath2[ISFS::EMUFS_MAX_PATH_LENGTH];

class EmuFSHandle
{
public:
    ~EmuFSHandle()
    {
        m_inUse = false;
        CloseBackend();
    }

    s32 OpenFile(const char* path, u32 mode, u32 uid, u16 gid, bool redirect);
    s32 Reopen();
    s32 Close();
    s32 CloseBackend();
    s32 Read(void* buffer, u32 size);
    s32 Write(const void* buffer, u32 size);
    s32 Seek(s32 offset, s32 origin);
    s32
    Ioctl(ISFS::ISFSIoctl command, void* in, u32 inLen, void* out, u32 outLen);
    s32 Ioctlv(
        ISFS::ISFSIoctl command, u32 inCount, u32 outCount, IOS::TVector* vec
    );

    s32 CopyData(EmuFSHandle* source);

    s32 CreateDir(
        const char* path, u8 ownerPerm, u8 groupPerm, u8 otherPerm,
        u8 attributes
    );
    s32 ReadDir(const char* path, char* outNames, u32 outNamesSize, u32* count);
    s32 SetAttr(
        const char* path, u32 ownerId, u16 groupId, u8 ownerPerm, u8 groupPerm,
        u8 otherPerm, u8 attributes
    );
    s32 GetAttr(
        const char* path, u32* ownerId, u16* groupId, u8* ownerPerm,
        u8* groupPerm, u8* otherPerm, u8* attributes
    );
    s32 Delete(const char* path);
    s32 Rename(const char* pathOld, const char* pathNew);
    s32 CreateFile(
        const char* path, u8 ownerPerm, u8 groupPerm, u8 otherPerm,
        u8 attributes
    );
    s32 GetUsage(const char* path, u32* clusters, u32* inodes);

    // File commands
    s32 GetFileStats(u32* size, u32* position);

    s32 DirectDirOpen(const char* path);
    s32 DirectDirNext(char* name, u32* attributes);

    bool IsProxy() const
    {
        return m_proxyPath[0] != '\0';
    }

    bool IsValidFile() const
    {
        return m_inUse && !m_isManager &&
               (std::holds_alternative<ISFSFileHandle>(m_file) ||
                std::holds_alternative<FIL>(m_file));
    }

    bool IsValidDirectory() const
    {
        return m_inUse && !m_isManager &&
               (std::holds_alternative<ISFSReadDirCacheHandle>(m_file) ||
                std::holds_alternative<DIR>(m_file));
    }

    static s32 RegisterProxyHandle(const char* path);
    void FreeProxyHandle();
    static s32 FindProxyHandle(const char* path);
    static s32 FindFreeHandle();
    static s32 TryCloseProxyHandle(const char* path);

    IOS::ResourceCtrl<ISFS::ISFSIoctl> m_resource{-1};
    s32 m_fd = -1;
    u32 m_uid = 0;
    u16 m_gid = 0;
    bool m_isManager = false;
    bool m_inUse = false;
    bool m_backendFileOpened = false;
    char m_proxyPath[64] = {0};
    u32 m_accessMode = 0;
    bool m_redirect = false;
    bool m_blockExtendedInterface = false;

    struct ISFSFileHandle {
        s32 position;
        s32 size;
    };

    struct ISFSReadDirCacheHandle {
        const char* m_buffer;
        u32 m_bufferSize;
        u32 m_index;
        u32 m_offset;
    };

    std::variant<ISFSFileHandle, ISFSReadDirCacheHandle, FIL, DIR> m_file;
};

static std::array<EmuFSHandle, ISFS::MAX_OPEN_COUNT> s_handles;

static s32 IOS_OpenAsUid(const char* path, u32 mode, u32 uid, u16 gid)
{
    s32 pid = IOS_GetProcessId();
    ASSERT(pid >= 0);

    PRINT(IOS_EmuFS, INFO, "Set PID %d to UID %08X GID %04X", pid, uid, gid);

    // Security note! Interrupts will be disabled at this point
    // (IOS_Open always does), and the IPC thread can't do anything else
    // while it's waiting for a response from us, so this should be safe
    // to do to the root process..?

    s32 ret = IOS_SetUid(pid, uid);
    ASSERT(ret == IOS::IOSError::OK);
    ret = IOS_SetGid(pid, gid);
    ASSERT(ret == IOS::IOSError::OK);

    s32 fd = IOS_Open(path, mode);

    ret = IOS_SetUid(pid, 0);
    ASSERT(ret == IOS::IOSError::OK);
    ret = IOS_SetGid(pid, 0);
    ASSERT(ret == IOS::IOSError::OK);

    PRINT(IOS_EmuFS, INFO, "UID and GID restored", pid);

    return fd;
}

static s32 FResultToISFSError(FRESULT fresult)
{
    switch (fresult) {
    case FR_OK:
        return ISFS::ISFSError::OK;

    case FR_INVALID_NAME:
    case FR_INVALID_DRIVE:
    case FR_INVALID_PARAMETER:
    case FR_INVALID_OBJECT:
        return ISFS::ISFSError::INVALID;

    case FR_DISK_ERR:
    case FR_INT_ERR:
    case FR_NO_FILESYSTEM:
        return ISFS::ISFSError::CORRUPT;

    case FR_NOT_READY:
    case FR_NOT_ENABLED:
        return ISFS::ISFSError::NOT_READY;

    case FR_NO_FILE:
    case FR_NO_PATH:
        return ISFS::ISFSError::NOT_FOUND;

    case FR_DENIED:
    case FR_WRITE_PROTECTED:
        return ISFS::ISFSError::ACCESS_DENIED;

    case FR_EXIST:
        return ISFS::ISFSError::ALREADY_EXISTS;

    case FR_LOCKED:
        return ISFS::ISFSError::LOCKED;

    case FR_TOO_MANY_OPEN_FILES:
        return ISFS::ISFSError::MAX_HANDLES_OPEN;

    case FR_MKFS_ABORTED:
    case FR_NOT_ENOUGH_CORE:
    case FR_TIMEOUT:
    default:
        return ISFS::ISFSError::UNKNOWN;
    }
}

static u32 ISFSModeToFileMode(u32 mode)
{
    u32 out = 0;

    if (mode & IOS::Mode::READ) {
        out |= FA_READ;
    }

    if (mode & IOS::Mode::WRITE) {
        out |= FA_WRITE;
    }

    return out;
}

s32 EmuFSHandle::RegisterProxyHandle(const char* path)
{
    s32 match = 0;

    for (s32 i = 0; i < ISFS::MAX_OPEN_COUNT; i++) {
        // If the file was already opened, reuse the descriptor
        if (path && s_handles[i].m_backendFileOpened &&
            std::strcmp(s_handles[i].m_proxyPath, path) == 0) {
            if (s_handles[i].m_inUse) {
                return ISFS::ISFSError::LOCKED;
            }

            s_handles[i].m_inUse = true;
            return i;
        }

        if (!s_handles[i].m_inUse && s_handles[match].m_inUse) {
            match = i;
        }

        if (!s_handles[i].m_backendFileOpened &&
            s_handles[match].m_backendFileOpened) {
            match = i;
        }
    }

    if (s_handles[match].m_inUse) {
        return ISFS::ISFSError::MAX_HANDLES_OPEN;
    }

    // Close and use the file descriptor

    if (s_handles[match].m_backendFileOpened) {
        s_handles[match].CloseBackend();
    }

    s_handles[match].m_backendFileOpened = false;
    s_handles[match].m_inUse = true;
    std::strncpy(s_handles[match].m_proxyPath, path, 64);

    return match;
}

void EmuFSHandle::FreeProxyHandle()
{
    if (!IsValidFile()) {
        return;
    }

    m_inUse = false;
}

s32 EmuFSHandle::FindProxyHandle(const char* path)
{
    for (s32 i = 0; i < ISFS::MAX_OPEN_COUNT; i++) {
        if (s_handles[i].m_backendFileOpened &&
            std::strcmp(path, s_handles[i].m_proxyPath) == 0) {
            return i;
        }
    }

    return ISFS::MAX_OPEN_COUNT;
}

s32 EmuFSHandle::FindFreeHandle()
{
    s32 match = 0;

    for (int i = 0; i < ISFS::MAX_OPEN_COUNT; i++) {
        if (!s_handles[i].m_inUse && s_handles[match].m_inUse) {
            match = i;
        }

        if (!s_handles[i].m_backendFileOpened &&
            s_handles[match].m_backendFileOpened) {
            match = i;
        }
    }

    if (s_handles[match].m_inUse) {
        return ISFS::ISFSError::MAX_HANDLES_OPEN;
    }

    if (s_handles[match].m_backendFileOpened) {
        s32 ret = s_handles[match].CloseBackend();
        if (ret < 0) {
            return ret;
        }
    }

    return match;
}

s32 EmuFSHandle::TryCloseProxyHandle(const char* path)
{
    // Close a cached file handle
    s32 ret = FindProxyHandle(path);
    if (ret < 0) {
        return ret;
    }
    s32 entry = ret;
    ret = ISFS::ISFSError::OK;

    if (entry != ISFS::MAX_OPEN_COUNT) {
        // Will return LOCKED if the handle is in use
        ret = s_handles[entry].CloseBackend();
    }

    return ret;
}

/**
 * Gets the number of characters in a string, excluding the null terminator, up
 * to maxLength characters.
 */
static size_t strnlen(const char* str, size_t max)
{
    size_t i;
    for (i = 0; i < max && str[i]; i++)
        ;
    return i;
}

/**
 * Compare an element in a path up to the null terminator or a separator.
 */
static s32 PathElementCompare(const char* str1, const char* str2)
{
    ASSERT(str1 != nullptr && str2 != nullptr);

    for (size_t i = 0; i < ISFS::MAX_PATH_LENGTH; i++) {
        char c1 = str1[i];
        char c2 = str2[i];
        c1 = c1 == ISFS::SEPARATOR_CHAR ? '\0' : c1;
        c2 = c2 == ISFS::SEPARATOR_CHAR ? '\0' : c2;

        if (c1 != c2 || c1 == '\0') {
            return c1 - c2;
        }
    }

    return *str1 - *str2;
}

/**
 * Checks if an ISFS path is valid.
 */
static bool IsISFSPathValid(const char* path)
{
    if (path == nullptr) {
        return false;
    }

    if (path[0] != ISFS::SEPARATOR_CHAR) {
        return false;
    }

    size_t lastSep = 0;
    for (size_t i = 1;; i++) {
        if (i >= ISFS::MAX_PATH_LENGTH) {
            return false;
        }

        char c = path[i];

        if (c == '\0') {
            if (false) {
                // Don't allow a trailing slash, disabled for now
                if (i - lastSep - 1 == 0) {
                    return false;
                }
            }

            return true;
        }

        if (c == ISFS::SEPARATOR_CHAR) {
            size_t pat = i - lastSep - 1;
            if (pat == 0 || pat > ISFS::MAX_NAME_LENGTH) {
                return false;
            }

            // Prevent path traversal
            if (pat == 1 && path[lastSep + 1] == '.') {
                return false;
            }

            if (pat == 2 && path[lastSep + 1] == '.' &&
                path[lastSep + 2] == '.') {
                return false;
            }

            lastSep = i;
        }
    }
}

/**
 * Checks if an EmuFS path is valid.
 */
static bool IsEmuFSPathValid(const char* path)
{
    if (path == nullptr) {
        return false;
    }

    if (path[0] != ISFS::SEPARATOR_CHAR) {
        return false;
    }

    return strnlen(path, ISFS::EMUFS_MAX_PATH_LENGTH) <
           ISFS::EMUFS_MAX_PATH_LENGTH;
}

/**
 * Checks if a path is redirected somewhere else by the frontend.
 */
bool DeviceEmuFS::IsPathReplaced(const char* isfsPath)
{
    // Note this function is called by an external process so we can't do any
    // I/O

    if (PathElementCompare(isfsPath + 1, EMUFS_MOUNT_POINT + 1) == 0) {
        return true;
    }

    return Config::s_instance->IsISFSPathReplaced(isfsPath);
}

/**
 * Get the FATFS path from an ISFS path.
 * @returns True if an external path was found, false if ISFS.
 */
static bool GetFATFSPath(
    const char* isfsPath, char* efsOut, u32 outLen,
    [[maybe_unused]] bool redirect = true
)
{
    if (efsOut == nullptr) {
        return false;
    }

    efsOut[0] = 0;

    // Translate ex. /mnt/sd/file.bin to 0:/file.bin
    if (PathElementCompare(isfsPath + 1, EMUFS_MOUNT_POINT + 1) == 0) {
        const char* mnt = isfsPath + sizeof(EMUFS_MOUNT_POINT);
        char drive = 0;

        for (size_t i = 0; i < std::size(MOUNT_POINTS); i++) {
            if (PathElementCompare(
                    mnt, MOUNT_POINTS[i] + sizeof(EMUFS_MOUNT_POINT)
                ) == 0) {
                drive = '0' + i;
                isfsPath += std::strlen(MOUNT_POINTS[i]);
                break;
            }
        }

        if (drive == 0) {
            return false;
        }

        std::snprintf(
            efsOut, outLen, "%c:%s", drive,
            isfsPath + sizeof(EMUFS_MOUNT_POINT) + 1
        );
        return true;
    }

    // Create and write the replaced path
    // TODO: Redirect NAND paths
    if (!IsISFSPathValid(isfsPath)) {
        return false;
    }

    std::strncpy(efsOut, isfsPath, outLen);

    return false;
}

/**
 * Copy data from the source file into this file.
 */
s32 EmuFSHandle::CopyData(EmuFSHandle* source)
{
    static u8 s_buffer[0x2000] ATTRIBUTE_ALIGN(32); // 8 KB

    if (!IsValidFile() || !source->IsValidFile()) {
        return ISFS::ISFSError::INVALID;
    }

    if (!(m_accessMode & IOS::Mode::WRITE) ||
        !(source->m_accessMode & IOS::Mode::READ)) {
        return ISFS::ISFSError::ACCESS_DENIED;
    }

    for (;;) {
        s32 ret = source->Read(s_buffer, sizeof(s_buffer));

        if (ret < 0) {
            return ret;
        }

        if (ret == 0) {
            return ISFS::ISFSError::OK;
        }

        u32 writeLen = ret;
        ret = Write(s_buffer, writeLen);

        if (ret < 0) {
            return ret;
        }

        if (static_cast<u32>(ret) != writeLen) {
            return ISFS::ISFSError::UNKNOWN;
        }
    }
}

template <typename T>
bool WriteIfNotNull(T* dest, T value)
{
    if (dest == nullptr) {
        return false;
    }

    *dest = value;
    return true;
}

/**
 * Reset a cached file handle.
 */
s32 EmuFSHandle::Reopen()
{
    s32 ret = Seek(0, IOS_SEEK_SET);
    if (ret != 0) {
        return ret;
    }

    return ISFS::ISFSError::OK;
}

/**
 * Handle open file request from the filesystem proxy.
 * Uses s_efsPath.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::OpenFile(
    const char* path, u32 mode, u32 uid, u16 gid, bool redirect
)
{
    if (path[0] != ISFS::SEPARATOR_CHAR) {
        return ISFS::ISFSError::INVALID;
    }

    m_redirect = redirect;

    if (std::strcmp(path, "/dev/fs") == 0) {
        m_isManager = true;
        m_inUse = true;

        new (&m_resource
        ) IOS::ResourceCtrl<ISFS::ISFSIoctl>(IOS_OpenAsUid(path, mode, uid, gid)
        );

        return m_resource.GetFd();
    }

    if (PathElementCompare(path + 1, "dev")) {
        // Don't let the caller open a resource manager
        return IOS::IOSError::INVALID;
    }

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), redirect)) {
        if (s_efsPath[0] == '\0') {
            return ISFS::ISFSError::INVALID;
        }

        // Opening a NAND file through this interface
        m_file = ISFSFileHandle();
        new (&m_resource) IOS::ResourceCtrl<ISFS::ISFSIoctl>(
            IOS_OpenAsUid(s_efsPath, mode, uid, gid)
        );
        if (m_resource.GetFd() < 0) {
            return m_resource.GetFd();
        }

        m_isManager = false;
        m_inUse = true;
        m_backendFileOpened = true;

        return ISFS::ISFSError::OK;
    }

    // Open a file on the external filesystem
    m_file = FIL();
    FIL* fil = &std::get<FIL>(m_file);

    const FRESULT fresult = f_open(fil, s_efsPath, ISFSModeToFileMode(mode));
    if (fresult != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR, "Failed to open file '%s' error: %d", s_efsPath,
            fresult
        );
        return FResultToISFSError(fresult);
    }

    m_isManager = false;
    m_inUse = true;
    m_backendFileOpened = true;
    m_proxyPath[0] = '\0';

    // Check if it's a proxy file
    if (redirect && IsISFSPathValid(path) &&
        PathElementCompare(path + 1, EMUFS_MOUNT_POINT) == 0) {
        std::strncpy(m_proxyPath, path, ISFS::MAX_PATH_LENGTH);
    }

    return ISFS::ISFSError::OK;
}

/**
 * Handles direct open directory requests.
 * Uses s_efsPath.
 * @returns File descriptor, or ISFS error code.
 */
s32 EmuFSHandle::DirectDirOpen(const char* path)
{
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), m_redirect)) {
        if (s_efsPath[0] == '\0') {
            return ISFS::ISFSError::INVALID;
        }

        // NAND paths are not supported yet through this interface
        return ISFS::ISFSError::NOT_FOUND;
    }

    m_file = DIR();
    DIR* dir = &std::get<DIR>(m_file);

    const FRESULT fresult = f_opendir(dir, s_efsPath);
    if (fresult != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR, "Failed to open dir '%s' error: %d", s_efsPath,
            fresult
        );
        return FResultToISFSError(fresult);
    }

    m_inUse = true;
    m_backendFileOpened = true;

    return ISFS::ISFSError::OK;
}

/**
 * Close an open file handle.
 * @returns IOS::IOSError::OK for success, or IOS/ISFS error code.
 */
s32 EmuFSHandle::Close()
{
    if (m_resource.GetFd() >= 0) {
        [[maybe_unused]] s32 ret = m_resource.Close();
        ASSERT(ret == IOS::IOSError::OK);
    }

    m_inUse = false;

    if (m_isManager) {
        return ISFS::ISFSError::OK;
    }

    if (IsValidFile() && IsProxy()) {
        FIL* fil = &std::get<FIL>(m_file);
        if (fil == nullptr) {
            return CloseBackend();
        }

        // Leave the file open for caching purposes if it's a proxy file
        const FRESULT fresult = f_sync(fil);
        if (fresult != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "Failed to sync file, error: %d", fresult);
            return FResultToISFSError(fresult);
        }

        return ISFS::ISFSError::OK;
    }

    return CloseBackend();
}

/**
 * Close the backend file.
 * @returns IOS::IOSError::OK for success, or IOS/ISFS error code.
 */
s32 EmuFSHandle::CloseBackend()
{
    if (!m_backendFileOpened) {
        return ISFS::ISFSError::OK;
    }

    if (m_inUse) {
        return ISFS::ISFSError::LOCKED;
    }

    if (std::holds_alternative<ISFSFileHandle>(m_file)) {
        if (m_resource.GetFd() < 0) {
            return ISFS::ISFSError::OK;
        }

        const s32 ret = m_resource.Close();
        if (ret < 0) {
            return ret;
        }
    } else if (std::holds_alternative<ISFSReadDirCacheHandle>(m_file)) {
        delete[] std::get<ISFSReadDirCacheHandle>(m_file).m_buffer;
    } else if (std::holds_alternative<FIL>(m_file)) {
        const FRESULT fresult = f_close(&std::get<FIL>(m_file));
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR, "Failed to close backend file, error: %d",
                fresult
            );
            return FResultToISFSError(fresult);
        }
    } else if (std::holds_alternative<DIR>(m_file)) {
        const FRESULT fresult = f_closedir(&std::get<DIR>(m_file));
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Failed to close backend directory, error: %d", fresult
            );
            return FResultToISFSError(fresult);
        }

    } else {
        return ISFS::ISFSError::INVALID;
    }

    m_file = ISFSFileHandle();
    m_backendFileOpened = false;
    m_proxyPath[0] = '\0';

    return ISFS::ISFSError::OK;
}

/**
 * Read data from an open file handle.
 * @returns Amount read, or ISFS error code.
 */
s32 EmuFSHandle::Read(void* data, u32 len)
{
    if (!IsValidFile()) {
        return ISFS::ISFSError::INVALID;
    }

    if (!(m_accessMode & IOS::Mode::READ)) {
        return ISFS::ISFSError::ACCESS_DENIED;
    }

    if (len == 0) {
        return ISFS::ISFSError::OK;
    }

    u32 bytesRead;
    if (std::holds_alternative<FIL>(m_file)) {
        const FRESULT fresult =
            f_read(&std::get<FIL>(m_file), data, len, &bytesRead);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Failed to read %u bytes from handle %d, error: %d", len, m_fd,
                fresult
            );
            return FResultToISFSError(fresult);
        }
    } else if (std::holds_alternative<ISFSFileHandle>(m_file)) {
        const s32 ret = m_resource.Read(data, len);
        if (ret < 0) {
            return ret;
        }
        bytesRead = ret;
    } else {
        return ISFS::ISFSError::INVALID;
    }

    return bytesRead;
}

/**
 * Write data to an open file handle.
 * @returns Amount wrote, or ISFS error code.
 */
s32 EmuFSHandle::Write(const void* data, u32 len)
{
    if (!IsValidFile()) {
        return ISFS::ISFSError::INVALID;
    }

    if (!(m_accessMode & IOS::Mode::WRITE)) {
        return ISFS::ISFSError::ACCESS_DENIED;
    }

    if (len == 0) {
        return ISFS::ISFSError::OK;
    }

    u32 bytesWrote;
    if (std::holds_alternative<FIL>(m_file)) {
        const FRESULT fresult =
            f_write(&std::get<FIL>(m_file), data, len, &bytesWrote);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Failed to write %u bytes to handle %d, error: %d", len, m_fd,
                fresult
            );
            return FResultToISFSError(fresult);
        }
    } else if (std::holds_alternative<ISFSFileHandle>(m_file)) {
        const s32 ret = m_resource.Write(data, len);
        if (ret < 0) {
            return ret;
        }
        bytesWrote = ret;
    } else {
        return ISFS::ISFSError::INVALID;
    }

    return bytesWrote;
}

/**
 * Moves the file read/write position of an open file descriptor.
 * @returns New offset, or an ISFS error code.
 */
s32 EmuFSHandle::Seek(s32 where, s32 whence)
{
    if (!IsValidFile()) {
        return ISFS::ISFSError::INVALID;
    }

    if (whence < IOS_SEEK_SET || whence > IOS_SEEK_END) {
        return ISFS::ISFSError::INVALID;
    }

    if (std::holds_alternative<FIL>(m_file)) {
        FIL* fil = &std::get<FIL>(m_file);
        FSIZE_t offset = f_tell(fil);
        FSIZE_t endPosition = f_size(fil);

        switch (whence) {
        case IOS_SEEK_SET: {
            offset = 0;
            break;
        }
        case IOS_SEEK_CUR: {
            break;
        }
        case IOS_SEEK_END: {
            offset = endPosition;
            break;
        }
        }

        offset += where;
        if (offset > endPosition) {
            return ISFS::ISFSError::INVALID;
        }

        if (offset == f_tell(fil)) {
            PRINT(IOS_EmuFS, INFO, "Skipping seek");
            return offset;
        }

        const FRESULT fresult = f_lseek(fil, offset);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Failed to seek to position 0x%08X in handle %d", offset, m_fd
            );
            return FResultToISFSError(fresult);
        }

        return offset;
    } else if (std::holds_alternative<ISFSFileHandle>(m_file)) {
        return m_resource.Seek(where, whence);
    } else {
        return ISFS::ISFSError::INVALID;
    }
}

/**
 * Create a new directory.
 * Uses s_efsPath.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::CreateDir(
    const char* path, u8 ownerPerm, u8 groupPerm, u8 otherPerm, u8 attributes
)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), m_redirect)) {
        if (s_efsPath[0] == 0) {
            return ISFS::ISFSError::NOT_FOUND;
        }

        // NAND path
        ISFS::AttrBlock attr = {m_uid,     m_gid,     {},         ownerPerm,
                                groupPerm, otherPerm, attributes, {}};
        std::strncpy(attr.path, path, sizeof(attr.path));
        return m_resource.Ioctl(
            ISFS::ISFSIoctl::CREATE_DIR, &attr, sizeof(ISFS::AttrBlock),
            nullptr, 0
        );
    }

    const FRESULT fresult = f_mkdir(path);
    if (fresult != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR, "CreateDir: Failed to create directory '%s'",
            s_efsPath
        );
        return FResultToISFSError(fresult);
    }

    PRINT(IOS_EmuFS, INFO, "CreateDir: Created directory '%s'", s_efsPath);

    return ISFS::ISFSError::OK;
}

/**
 * Read the contents of a directory using the ISFS interface.
 * Uses s_efsPath.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::ReadDir(
    const char* path, char* outNames, u32 outNamesSize, u32* count
)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    if (count == nullptr) {
        return ISFS::ISFSError::INVALID;
    }

    if (outNames != nullptr && outNamesSize != *count * 13) {
        return ISFS::ISFSError::INVALID;
    }

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), m_redirect)) {
        if (s_efsPath[0] == 0) {
            return ISFS::ISFSError::NOT_FOUND;
        }

        // NAND path
        if (outNames == nullptr) {
            u32 tempCount = 0;

            IOS::IOVector<1, 1> vec;
            vec.in[0].data = s_efsPath;
            vec.in[0].len = ISFS::MAX_PATH_LENGTH;
            vec.out[0].data = &tempCount;
            vec.out[0].len = sizeof(u32);

            s32 ret = m_resource.Ioctlv(ISFS::ISFSIoctl::READ_DIR, vec);
            if (ret != ISFS::ISFSError::OK) {
                return ret;
            }

            *count = tempCount;
        } else {
            u32 maxCount = *count;
            u32 tempCount = 0;

            IOS::IOVector<2, 2> vec;
            vec.in[0].data = s_efsPath;
            vec.in[0].len = ISFS::MAX_PATH_LENGTH;
            vec.in[1].data = &maxCount;
            vec.in[1].len = sizeof(u32);
            vec.out[0].data = outNames;
            vec.out[0].len = outNamesSize;
            vec.out[1].data = &tempCount;
            vec.out[1].len = sizeof(u32);

            s32 ret = m_resource.Ioctlv(ISFS::ISFSIoctl::READ_DIR, vec);
            if (ret != ISFS::ISFSError::OK) {
                return ret;
            }

            *count = tempCount;
        }

        return ISFS::ISFSError::OK;
    }

    DIR dir = {};
    FRESULT fresult = f_opendir(&dir, s_efsPath);
    if (fresult != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "Failed to open directory, error: %d", fresult);
        return FResultToISFSError(fresult);
    }

    FILINFO info = {};
    u32 entry = 0;
    u32 maxCount = *count;
    for (; (fresult = f_readdir(&dir, &info)) == FR_OK; entry++) {
        ASSERT(entry < INT_MAX);

        const char* name = info.fname;
        auto len = std::strlen(name);
        if (len <= 0) {
            break;
        }

        if (len > 12) {
            if (std::strlen(info.altname) < 1 ||
                !std::strcmp(info.altname, "?")) {
                continue;
            }

            name = info.altname;
        }

        if (entry < maxCount) {
            char nameData[13] = {0};
            std::strncpy(nameData, name, sizeof(nameData));
            std::memcpy(outNames + entry * 13, nameData, sizeof(nameData));
        }
    }

    const FRESULT fresult2 = f_closedir(&dir);
    if (fresult2 != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "f_closedir error: %d", fresult2);
        return ISFS::ISFSError::UNKNOWN;
    }

    if (fresult != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "f_readdir error: %d", fresult);
        return FResultToISFSError(fresult);
    }

    PRINT(IOS_EmuFS, INFO, "Count: %u", entry);
    *count = entry;

    return ISFS::ISFSError::OK;
}

/**
 * Set attributes for a file or directory.
 * Uses s_efsPath.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::SetAttr(
    const char* path, u32 ownerId, u16 groupId, u8 ownerPerm, u8 groupPerm,
    u8 otherPerm, u8 attributes
)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath))) {
        if (s_efsPath[0] == 0) {
            return ISFS::ISFSError::NOT_FOUND;
        }

        // NAND path
        ISFS::AttrBlock attr = {ownerId,   groupId,   {},         ownerPerm,
                                groupPerm, otherPerm, attributes, {}};
        std::strncpy(attr.path, path, sizeof(attr.path));
        return m_resource.Ioctl(
            ISFS::ISFSIoctl::SET_ATTR, &attr, sizeof(attr), nullptr, 0
        );
    }

    const FRESULT fresult = f_stat(s_efsPath, nullptr);
    if (fresult != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR,
            "SetAttr: Failed to set attributes for file or directory '%s'",
            s_efsPath
        );
        return FResultToISFSError(fresult);
    }

    PRINT(
        IOS_EmuFS, INFO, "SetAttr: Set attributes for file or directory '%s'",
        s_efsPath
    );

    return ISFS::ISFSError::OK;
}

/**
 * Get attributes for a file or directory.
 * Uses s_efsPath.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::GetAttr(
    const char* path, u32* ownerId, u16* groupId, u8* ownerPerm, u8* groupPerm,
    u8* otherPerm, u8* attributes
)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    static constexpr u8 OWNER_PERM = 3;
    static constexpr u8 GROUP_PERM = 3;
    static constexpr u8 OTHER_PERM = 1;
    static constexpr u8 ATTRIBUTES = 0;

    ISFS::AttrBlock attr = {m_uid,      m_gid,      {},         OWNER_PERM,
                            GROUP_PERM, OTHER_PERM, ATTRIBUTES, {}};

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), m_redirect)) {
        if (s_efsPath[0] == 0) {
            return ISFS::ISFSError::NOT_FOUND;
        }

        // NAND path
        s32 ret = m_resource.Ioctl(
            ISFS::ISFSIoctl::GET_ATTR, nullptr, 0, &attr,
            sizeof(ISFS::AttrBlock)
        );
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }
    } else {
        // Test that the file exists
        const FRESULT fresult = f_stat(s_efsPath, nullptr);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Failed to get attributes for file or directory '%s'", s_efsPath
            );
            return FResultToISFSError(fresult);
        }
    }

    WriteIfNotNull(ownerId, attr.ownerId);
    WriteIfNotNull(groupId, attr.groupId);
    WriteIfNotNull(ownerPerm, attr.ownerPerm);
    WriteIfNotNull(groupPerm, attr.groupPerm);
    WriteIfNotNull(otherPerm, attr.otherPerm);
    WriteIfNotNull(attributes, attr.attributes);

    return ISFS::ISFSError::OK;
}

/**
 * Delete a file or directory.
 * Uses s_efsPath.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::Delete(const char* path)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), m_redirect)) {
        if (s_efsPath[0] == 0) {
            return ISFS::ISFSError::NOT_FOUND;
        }

        // NAND path
        return m_resource.Ioctl(
            ISFS::ISFSIoctl::DELETE, path, ISFS::MAX_PATH_LENGTH, nullptr, 0
        );
    }

    // Check if the path is a mounted path or a proxied NAND path
    if (IsISFSPathValid(path)) {
        s32 ret = TryCloseProxyHandle(path);
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }
    }

    const FRESULT fresult = f_unlink(s_efsPath);
    if (fresult != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR, "Failed to delete file or directory '%s'",
            s_efsPath
        );
        return FResultToISFSError(fresult);
    }

    PRINT(IOS_EmuFS, INFO, "Deleted file or directory '%s'", s_efsPath);

    return ISFS::ISFSError::OK;
}

/**
 * Rename a file or directory.
 * Uses s_efsPath and s_efsPath2.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::Rename(const char* pathOld, const char* pathNew)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    // Use one character path to get the device
    char *efsOldPath = s_efsPath, *efsNewPath = s_efsPath2;

    if (!GetFATFSPath(pathOld, efsOldPath, sizeof(efsOldPath), m_redirect) &&
        efsOldPath[0] == 0) {
        return ISFS::ISFSError::NOT_FOUND;
    }

    if (!GetFATFSPath(pathNew, efsNewPath, sizeof(efsNewPath), m_redirect) &&
        efsNewPath[0] == 0) {
        return ISFS::ISFSError::NOT_FOUND;
    }

    if (efsOldPath[0] == '/' && efsNewPath[0] == '/') {
        // Both paths are NAND paths
        ISFS::RenameBlock renameBlock = {};
        std::strncpy(
            renameBlock.pathOld, efsOldPath, sizeof(renameBlock.pathOld)
        );
        std::strncpy(
            renameBlock.pathNew, efsNewPath, sizeof(renameBlock.pathNew)
        );
        return m_resource.Ioctl(
            ISFS::ISFSIoctl::RENAME, &renameBlock, sizeof(renameBlock), nullptr,
            0
        );
    }

    if (m_redirect) {
        s32 ret = TryCloseProxyHandle(pathOld);
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }

        ret = TryCloseProxyHandle(pathNew);
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }
    }

    if (efsOldPath[0] == efsNewPath[0]) {
        // Same external device
        const FRESULT fresult = f_rename(efsOldPath, efsNewPath);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Failed to rename file or directory '%s' -> '%s'", efsOldPath,
                efsNewPath
            );
        }
        return FResultToISFSError(fresult);
    }

    // Cross filesystem rename
    EmuFSHandle oldHandle, newHandle;
    s32 ret = oldHandle.OpenFile(
        pathOld, IOS::Mode::READ | IOS::Mode::READ_WRITE, m_uid, m_gid,
        m_redirect
    );
    if (ret != ISFS::ISFSError::OK) {
        return ret;
    }

    u32 ownerId;
    u16 groupId;
    u8 ownerPerm, groupPerm, otherPerm, attributes;
    ret = GetAttr(
        pathOld, &ownerId, &groupId, &ownerPerm, &groupPerm, &otherPerm,
        &attributes
    );
    if (ret != ISFS::ISFSError::OK) {
        return ret;
    }

    ret =
        newHandle.OpenFile(pathNew, IOS::Mode::WRITE, m_uid, m_gid, m_redirect);
    if (ret != ISFS::ISFSError::OK) {
        if (ret != ISFS::ISFSError::NOT_FOUND) {
            return ret;
        }

        // File doesn't exist, create it
        ret = CreateFile(pathNew, 0, 0, 0, 0);
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }

        ret = newHandle.OpenFile(pathNew, IOS::Mode::WRITE, 0, 0, m_redirect);
    }

    ret = newHandle.CopyData(&oldHandle);
    if (ret != ISFS::ISFSError::OK) {
        return ret;
    }

    oldHandle.CloseBackend();
    newHandle.CloseBackend();

    ret = SetAttr(
        pathNew, ownerId, groupId, ownerPerm, groupPerm, otherPerm, attributes
    );
    if (ret != ISFS::ISFSError::OK) {
        return ret;
    }

    return Delete(pathOld);
}

/**
 * Create a new file.
 * Uses s_efsPath.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::CreateFile(
    const char* path, u8 ownerPerm, u8 groupPerm, u8 otherPerm, u8 attributes
)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), m_redirect)) {
        if (s_efsPath[0] == 0) {
            return ISFS::ISFSError::NOT_FOUND;
        }

        // NAND path
        ISFS::AttrBlock attr = {m_uid,     m_gid,     {},         ownerPerm,
                                groupPerm, otherPerm, attributes, {}};
        std::strncpy(attr.path, path, sizeof(attr.path));
        return m_resource.Ioctl(
            ISFS::ISFSIoctl::DELETE, path, ISFS::MAX_PATH_LENGTH, nullptr, 0
        );
    }

    FIL fil;
    const FRESULT fresult =
        f_open(&fil, s_efsPath, FA_CREATE_NEW | FA_READ | FA_WRITE);
    if (fresult != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "Failed to create file '%s'", s_efsPath);
        return FResultToISFSError(fresult);
    }

    f_sync(&fil);

    // Cache the file handle
    if (m_redirect && IsISFSPathValid(path) &&
        PathElementCompare(path + 1, EMUFS_MOUNT_POINT) == 0) {
        s32 ret = FindFreeHandle();
        if (ret < 0 || ret >= ISFS::MAX_OPEN_COUNT) {
            f_close(&fil);
            return ISFS::ISFSError::OK;
        }

        EmuFSHandle* handle = &s_handles[ret];

        // Reset the file handle
        {
            handle->~EmuFSHandle();
            new (handle) EmuFSHandle();
        }

        handle->m_backendFileOpened = true;
        handle->m_file = fil;
        std::strncpy(handle->m_proxyPath, path, ISFS::MAX_PATH_LENGTH);

        return ISFS::ISFSError::OK;
    }

    f_close(&fil);

    return ISFS::ISFSError::OK;
}

s32 EmuFSHandle::GetUsage(const char* path, u32* clusters, u32* inodes)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    // Get the replaced path
    if (!GetFATFSPath(path, s_efsPath, sizeof(s_efsPath), m_redirect)) {
        if (s_efsPath[0] == 0) {
            return ISFS::ISFSError::NOT_FOUND;
        }

        // NAND path
        u32 tmpClusters = 0, tmpInodes = 0;
        IOS::IOVector<1, 2> vec;
        vec.in[0].data = s_efsPath;
        vec.in[0].len = ISFS::MAX_PATH_LENGTH;
        vec.out[0].data = &tmpClusters;
        vec.out[0].len = sizeof(u32);
        vec.out[1].data = &tmpInodes;
        vec.out[1].len = sizeof(u32);

        s32 ret = m_resource.Ioctlv(ISFS::ISFSIoctl::GET_USAGE, vec);
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }

        WriteIfNotNull(clusters, tmpClusters);
        WriteIfNotNull(inodes, tmpInodes);
    }

    // TODO
    WriteIfNotNull<u32>(clusters, 0);
    WriteIfNotNull<u32>(inodes, 0);

    return ISFS::ISFSError::OK;
}

/**
 * Cycle to the next entry in a directory.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::DirectDirNext(char* name, u32* attributes)
{
    (void) name;
    (void) attributes;

    return ISFS::ISFSError::INVALID;
}

/**
 * Get the file size and current position.
 * @returns ISFS error code.
 */
s32 EmuFSHandle::GetFileStats(u32* size, u32* position)
{
    if (!IsValidFile()) {
        return ISFS::ISFSError::INVALID;
    }

    if (std::holds_alternative<FIL>(m_file)) {
        FIL* fil = &std::get<FIL>(m_file);

        if (size != nullptr) {
            *size = f_size(fil);
        }

        if (position != nullptr) {
            *position = f_tell(fil);
        }

        return ISFS::ISFSError::OK;
    } else if (std::holds_alternative<ISFSFileHandle>(m_file)) {
        IOS::File::Stats stats;
        s32 ret = m_resource.Ioctl(
            ISFS::ISFSIoctl::GET_FILE_STATS, nullptr, 0, &stats, sizeof(stats)
        );
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }

        if (size != nullptr) {
            *size = stats.size;
        }

        if (position != nullptr) {
            *position = stats.pos;
        }

        return ISFS::ISFSError::OK;
    }

    return ISFS::ISFSError::INVALID;
}

template <typename T>
T* ipc_vector_cast(void* ptr, u32 len)
{
    if (len < sizeof(T)) {
        return nullptr;
    }

    if (!IsAligned(ptr, alignof(T))) {
        return nullptr;
    }

    return reinterpret_cast<T*>(ptr);
}

/**
 * Handles filesystem ioctl commands.
 * @returns ISFS::ISFSError result.
 */
s32 EmuFSHandle::Ioctl(
    ISFS::ISFSIoctl command, void* in, u32 inLen, void* out, u32 outLen
)
{
    if (inLen == 0) {
        in = nullptr;
    }

    if (outLen == 0) {
        out = nullptr;
    }

    // File commands
    if (!m_isManager) {
        if (command != ISFS::ISFSIoctl::GET_FILE_STATS) {
            PRINT(
                IOS_EmuFS, ERROR, "Unknown file ioctl: %u",
                static_cast<s32>(command)
            );
            return ISFS::ISFSError::INVALID;
        }

        IOS::File::Stats* stats =
            ipc_vector_cast<IOS::File::Stats>(out, outLen);
        if (stats == nullptr) {
            return ISFS::ISFSError::INVALID;
        }

        u32 size, position;
        s32 ret = GetFileStats(&size, &position);
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }

        stats->size = size;
        stats->pos = position;

        return ISFS::ISFSError::OK;
    }

    // Manager commands
    ISFS::AttrBlock* isfsAttrBlock = nullptr;

    // TODO Add ISFS_Shutdown
    switch (command) {
    // [ISFS_Format]
    // in: not used
    // out: not used
    case ISFS::ISFSIoctl::FORMAT:
        PRINT(IOS_EmuFS, ERROR, "Format: Attempt to use ISFS_Format!");
        return ISFS::ISFSError::ACCESS_DENIED;

    // [ISFS_CreateDir]
    // in: Accepts ISFS::AttrBlock. Reads path, ownerPerm, groupPerm, otherPerm,
    // and attributes.
    // out: not used
    case ISFS::ISFSIoctl::CREATE_DIR: {
        if ((isfsAttrBlock = ipc_vector_cast<ISFS::AttrBlock>(in, inLen)) ==
            nullptr) {
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH] = {};
        std::memcpy(path, isfsAttrBlock->path, ISFS::MAX_PATH_LENGTH);
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            return ISFS::ISFSError::INVALID;
        }

        // Check if the path is valid
        if (!IsISFSPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        return CreateDir(
            path, isfsAttrBlock->ownerPerm, isfsAttrBlock->groupPerm,
            isfsAttrBlock->otherPerm, isfsAttrBlock->attributes
        );
    }

    // [ISFS_SetAttr]
    // in: Accepts ISFS::AttrBlock. All fields are read. If the caller's UID is
    // not zero, ownerID and groupID must be equal to the caller's. Otherwise,
    // throw ISFS::ISFSError::ACCESS_DENIED.
    // out: not used
    case ISFS::ISFSIoctl::SET_ATTR: {
        if ((isfsAttrBlock = ipc_vector_cast<ISFS::AttrBlock>(in, inLen)) ==
            nullptr) {
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH] = {};
        std::memcpy(path, isfsAttrBlock->path, ISFS::MAX_PATH_LENGTH);
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            return ISFS::ISFSError::INVALID;
        }

        // Check if the path is valid
        if (!IsISFSPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        return SetAttr(
            path, isfsAttrBlock->ownerId, isfsAttrBlock->groupId,
            isfsAttrBlock->ownerPerm, isfsAttrBlock->groupPerm,
            isfsAttrBlock->otherPerm, isfsAttrBlock->attributes
        );
    }

    // [ISFS_GetAttr]
    // in: Path to a file or directory.
    // out: File/directory's attributes (ISFS::AttrBlock).
    case ISFS::ISFSIoctl::GET_ATTR: {
        if ((isfsAttrBlock = ipc_vector_cast<ISFS::AttrBlock>(out, outLen)) ==
            nullptr) {
            return ISFS::ISFSError::INVALID;
        }

        if (inLen == 0) {
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH] = {};
        std::memcpy(path, in, std::min(inLen, ISFS::MAX_PATH_LENGTH));
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            return ISFS::ISFSError::INVALID;
        }

        // Check if the path is valid
        if (!IsISFSPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        u32 ownerId;
        u16 groupId;
        u8 ownerPerm, groupPerm, otherPerm, attributes;
        s32 ret = GetAttr(
            path, &ownerId, &groupId, &ownerPerm, &groupPerm, &otherPerm,
            &attributes
        );
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }

        // Make a temporary AttrBlock on the stack first, in case the output is
        // in MEM1
        ISFS::AttrBlock attr = {ownerId,   groupId,   {},         ownerPerm,
                                groupPerm, otherPerm, attributes, {}};
        std::strncpy(attr.path, path, sizeof(attr.path));

        std::memcpy(isfsAttrBlock, &attr, sizeof(ISFS::AttrBlock));

        return ISFS::ISFSError::OK;
    }

    // [ISFS_Delete]
    // in: Path to the file or directory to delete.
    // out: not used
    case ISFS::ISFSIoctl::DELETE: {
        if (inLen == 0) {
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH] = {};
        std::memcpy(path, in, std::min(inLen, ISFS::MAX_PATH_LENGTH));
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            return ISFS::ISFSError::INVALID;
        }

        // Check if the path is valid
        if (!IsISFSPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        return Delete(path);
    }

    // [ISFS_Rename]
    // in: ISFS::RenameBlock.
    // out: not used
    case ISFS::ISFSIoctl::RENAME: {
        ISFS::RenameBlock* isfsRenameBlock;
        if ((isfsRenameBlock = ipc_vector_cast<ISFS::RenameBlock>(in, inLen)) ==
            nullptr) {
            return ISFS::ISFSError::INVALID;
        }

        char pathOld[64], pathNew[64];
        std::memcpy(pathOld, isfsRenameBlock->pathOld, ISFS::MAX_PATH_LENGTH);
        if (pathOld[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            return ISFS::ISFSError::INVALID;
        }

        std::memcpy(pathNew, isfsRenameBlock->pathNew, ISFS::MAX_PATH_LENGTH);
        if (pathNew[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            return ISFS::ISFSError::INVALID;
        }

        // Check if the old and new paths are valid
        if (!IsISFSPathValid(pathOld) || !IsISFSPathValid(pathNew)) {
            return ISFS::ISFSError::INVALID;
        }

        return Rename(pathOld, pathNew);
    }

    // [ISFS_CreateFile]
    // in: Accepts ISFS::AttrBlock. Reads path, ownerPerm, groupPerm, otherPerm,
    // and attributes.
    // out: not used
    case ISFS::ISFSIoctl::CREATE_FILE: {
        if ((isfsAttrBlock = ipc_vector_cast<ISFS::AttrBlock>(in, inLen)) ==
            nullptr) {
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH] = {};
        std::memcpy(path, isfsAttrBlock->path, ISFS::MAX_PATH_LENGTH);
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            return ISFS::ISFSError::INVALID;
        }

        // Check if the path is valid
        if (!IsISFSPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        return CreateFile(
            path, isfsAttrBlock->ownerPerm, isfsAttrBlock->groupPerm,
            isfsAttrBlock->otherPerm, isfsAttrBlock->attributes
        );
    }

    // [ISFS_Shutdown]
    case ISFS::ISFSIoctl::SHUTDOWN: {
        // This command is called to wait for any in-progress file operations to
        // be completed before shutting down
        PRINT(IOS_EmuFS, INFO, "Shutdown: ISFS_Shutdown()");
        return ISFS::ISFSError::OK;
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "Unknown manager ioctl: %u", command);
        return ISFS::ISFSError::INVALID;
    }
}

/**
 * Handles filesystem ioctlv commands using the Starling direct interface.
 * @returns ISFS::ISFSError result.
 */
#if 0
static s32 Direct_ReqIoctlv(
    s32 fd, ISFS::ISFSIoctl cmd, u32 in_count, u32 outCount, IOS::TVector*
    vec
)
{
    switch (cmd) {
    default:
        PRINT(
            IOS_EmuFS, ERROR, "Unknown direct ioctl: %u",
            static_cast<s32>(cmd)
        );
        return ISFS::ISFSError::INVALID;

    case ISFS::ISFSIoctl::DIRECT_OPEN: {
        if (in_count != 2 || outCount != 0) {
            PRINT(IOS_EmuFS, ERROR, "Direct_Open: Wrong vector count!");
            return ISFS::ISFSError::INVALID;
        }

        if (vec[0].len < 1 || vec[0].len > ISFS::EMUFS_MAX_PATH_LENGTH) {
            PRINT(
                IOS_EmuFS, ERROR, "Direct_Open: Invalid path length: %d",
                vec[0].len
            );
            return ISFS::ISFSError::INVALID;
        }

        if (vec[1].len != sizeof(u32)) {
            PRINT(
                IOS_EmuFS, ERROR, "Direct_Open: Invalid open mode length: %d", vec[1].len
            );
            return ISFS::ISFSError::INVALID;
        }

        if (!IsAligned(vec[1].data, 4)) {
            PRINT(IOS_EmuFS, ERROR, "Direct_Open: Invalid open mode alignment");
            return ISFS::ISFSError::INVALID;
        }

        // Check if the supplied file path length is valid.
        if (strnlen(reinterpret_cast<const char*>(vec[0].data), vec[0].len)
        ==
            vec[0].len) {
            PRINT(IOS_EmuFS, ERROR, "Direct_Open: Path does not terminate");
            return ISFS::ISFSError::INVALID;
        }

        // Check if the file is already open
        if (s_directFileArray[GetHandleIndex(fd)].fd !=
            ISFS::ISFSError::NOT_FOUND) {
            PRINT(IOS_EmuFS, ERROR, "Direct_Open: File already open");
            return ISFS::ISFSError::INVALID;
        }

        s32 realFd = ReqDirectOpen(
            reinterpret_cast<const char*>(vec[0].data),
            *reinterpret_cast<u32*>(vec[1].data)
        );
        if (realFd < 0) {
            return realFd;
        }

        s_directFileArray[GetHandleIndex(fd)].inUse = true;
        s_directFileArray[GetHandleIndex(fd)].fd = realFd;

        return ISFS::ISFSError::OK;
    }

    case ISFS::ISFSIoctl::DIRECT_DIR_OPEN: {
        if (in_count != 1 || outCount != 0) {
            PRINT(IOS_EmuFS, ERROR, "Direct_DirOpen: Wrong vector count!");
            return ISFS::ISFSError::INVALID;
        }

        if (vec[0].len < 1 || vec[0].len > ISFS::EMUFS_MAX_PATH_LENGTH) {
            PRINT(
                IOS_EmuFS, ERROR, "Direct_DirOpen: Invalid path length: %d",
                vec[0].len
            );
            return ISFS::ISFSError::INVALID;
        }

        // Check if the supplied file path length is valid.
        if (strnlen(reinterpret_cast<const char*>(vec[0].data), vec[0].len)
        ==
            vec[0].len) {
            PRINT(IOS_EmuFS, ERROR, "Direct_Open: Path does not terminate");
            return ISFS::ISFSError::INVALID;
        }

        // Check if the file is already open
        if (s_directFileArray[GetHandleIndex(fd)].fd !=
            ISFS::ISFSError::NOT_FOUND) {
            PRINT(IOS_EmuFS, ERROR, "Direct_DirOpen: File already open");
            return ISFS::ISFSError::INVALID;
        }

        s32 realFd =
            ReqDirectOpenDir(reinterpret_cast<const char*>(vec[0].data));
        if (realFd < 0) {
            return realFd;
        }

        s_directFileArray[GetHandleIndex(fd)].inUse = true;
        s_directFileArray[GetHandleIndex(fd)].fd = realFd;

        return ISFS::ISFSError::OK;
    }

    case ISFS::ISFSIoctl::DIRECT_DIR_NEXT: {
        if (in_count != 0 || outCount != 1) {
            PRINT(IOS_EmuFS, ERROR, "Direct_DirNext: Wrong vector count!");
            return ISFS::ISFSError::INVALID;
        }

        if (vec[0].len != sizeof(ISFS::Direct_Stats)) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Direct_DirNext: Wrong ISFS::Direct_Stats length: %u",
                vec[0].len
            );
            return ISFS::ISFSError::INVALID;
        }

        auto stat = (ISFS::Direct_Stats*) vec[0].data;
        memset(stat, 0, vec[0].len);

        if (!s_directFileArray[GetHandleIndex(fd)].inUse ||
            s_directFileArray[GetHandleIndex(fd)].fd ==
                ISFS::ISFSError::NOT_FOUND) {
            PRINT(IOS_EmuFS, ERROR, "Direct_DirNext: File not open!");
            return ISFS::ISFSError::INVALID;
        }

        s32 realFd = s_directFileArray[GetHandleIndex(fd)].fd;
        if (!s_fileArray[realFd].isDir) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Direct_DirNext: Requested FD is not a directory!"
            );
            return ISFS::ISFSError::INVALID;
        }

        FILINFO fno = {};
        auto fresult = f_readdir(&s_fileArray[realFd].dir, &fno);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR, "Direct_DirNext: f_readdir error: %d",
                fresult
            );
            return FResultToISFSError(fresult);
        }

        if (fno.fname[0] == '\0') {
            PRINT(IOS_EmuFS, INFO, "Direct_DirNext: Reached end of directory");
            // Caller should recognize a blank filename as the end of the
            // directory.
            return ISFS::ISFSError::OK;
        }

        ISFS::Direct_Stats tmpStat = {};

        tmpStat.dirOffset = 0; // TODO
        tmpStat.attribute = fno.fattrib;
        tmpStat.size = fno.fsize;
        strncpy(tmpStat.name, fno.fname, ISFS::EMUFS_MAX_PATH_LENGTH);
        System::UnalignedMemcpy(stat, &tmpStat, sizeof(ISFS::Direct_Stats));
        return ISFS::ISFSError::OK;
    }
    }
}
#endif

/**
 * Handles filesystem ioctlv commands.
 * @returns ISFS::ISFSError result.
 */
s32 EmuFSHandle::Ioctlv(
    ISFS::ISFSIoctl command, u32 inCount, u32 outCount, IOS::TVector* vec
)
{
    if (!m_isManager) {
        return ISFS::ISFSError::INVALID;
    }

    if (inCount >= 32 || outCount >= 32) {
        return ISFS::ISFSError::INVALID;
    }

    // NULL any zero length vectors to prevent any accidental writes.
    for (u32 i = 0; i < inCount + outCount; i++) {
        if (vec[i].len == 0) {
            vec[i].data = nullptr;
        }
    }

    auto fCheckVectorCount = [command, inCount, outCount](
                                 const char* cmdName, u32 expectedInCount,
                                 u32 expectedOutCount
                             ) -> bool {
        if (inCount != expectedInCount || outCount != expectedOutCount) {
            PRINT(IOS_EmuFS, ERROR, "%s: Wrong vector count", cmdName);
            return false;
        }

        return true;
    };

    switch (command) {
    // [ISFS_ReadDir]
    // vec[0]: path
    // todo
    case ISFS::ISFSIoctl::READ_DIR: {
        if (inCount != outCount || inCount < 1 || inCount > 2) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: Wrong vector count");
            return ISFS::ISFSError::INVALID;
        }

        if (vec[0].len == 0) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: Invalid input path vector");
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH] = {};
        std::memcpy(
            path, vec[0].data, std::min(vec[0].len, ISFS::MAX_PATH_LENGTH)
        );
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: Path does not terminate");
            return ISFS::ISFSError::INVALID;
        }

        PRINT(IOS_EmuFS, INFO, "ReadDir: ISFS_ReadDir(\"%s\")", path);

        u32 inMaxCount = 0;
        char* outNames = nullptr;
        u32* outCountPtr = nullptr;

        if (inCount == 2) {
            u32* inMaxCountPtr = ipc_vector_cast<u32>(vec[1].data, vec[1].len);
            if (inMaxCountPtr == nullptr) {
                PRINT(
                    IOS_EmuFS, ERROR, "ReadDir: Invalid input max count vector"
                );
                return ISFS::ISFSError::INVALID;
            }

            inMaxCount = *inMaxCountPtr;

            if (vec[2].len < inMaxCount * 13) {
                PRINT(
                    IOS_EmuFS, ERROR,
                    "ReadDir: Invalid output file names vector"
                );
                return ISFS::ISFSError::INVALID;
            }

            outCountPtr = ipc_vector_cast<u32>(vec[3].data, vec[3].len);
            if (outCountPtr == nullptr) {
                PRINT(
                    IOS_EmuFS, ERROR,
                    "ReadDir: Invalid output file count vector"
                );
                return ISFS::ISFSError::INVALID;
            }
        } else {
            outCountPtr = ipc_vector_cast<u32>(vec[1].data, vec[1].len);
            if (outCountPtr == nullptr) {
                PRINT(
                    IOS_EmuFS, ERROR,
                    "ReadDir: Invalid output file count vector"
                );
                return ISFS::ISFSError::INVALID;
            }
        }

        u32 tempCount = inMaxCount;
        s32 ret = ReadDir(path, outNames, inMaxCount * 13, &tempCount);
        if (ret != ISFS::ISFSError::OK) {
            return ret;
        }

        if (outCountPtr != nullptr) {
            *outCountPtr = tempCount;
        }

        return ISFS::ISFSError::OK;
    }

    // [ISFS_GetUsage]
    // vec[0](in): Path
    // vec[1](out): Used clusters
    // vec[2](out): Used inodes
    case ISFS::ISFSIoctl::GET_USAGE: {
        if (!fCheckVectorCount("GetUsage", 1, 2)) {
            return ISFS::ISFSError::INVALID;
        }

        if (vec[0].len == 0) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Invalid input path vector");
            return ISFS::ISFSError::INVALID;
        }

        u32* usedClusters = ipc_vector_cast<u32>(vec[1].data, vec[1].len);
        if (usedClusters == nullptr) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Invalid used clusters vector");
            return ISFS::ISFSError::INVALID;
        }

        u32* usedInodes = ipc_vector_cast<u32>(vec[2].data, vec[2].len);
        if (usedInodes == nullptr) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Invalid used inodes vector");
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH] = {};
        memcpy(path, vec[0].data, std::min(vec[0].len, ISFS::MAX_PATH_LENGTH));
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Path does not terminate");
            return ISFS::ISFSError::INVALID;
        }

        return GetUsage(path, usedClusters, usedInodes);
    }

    // [ISFS_ExOpen]
    // vec[0](in): Path
    // vec[1](in): Mode
    case ISFS::ISFSIoctl::EX_OPEN: {
        // This command is added to support opening files with paths longer than
        // 63 characters
        if (!fCheckVectorCount("ExOpen", 2, 0)) {
            return ISFS::ISFSError::INVALID;
        }

        if (!m_isManager) {
            PRINT(IOS_EmuFS, ERROR, "ExOpen: Not a manager handle");
            return ISFS::ISFSError::INVALID;
        }

        if (vec[0].len == 0 || vec[0].len > ISFS::EMUFS_MAX_PATH_LENGTH) {
            PRINT(IOS_EmuFS, ERROR, "ExOpen: Invalid input path vector");
            return ISFS::ISFSError::INVALID;
        }

        u32* mode = ipc_vector_cast<u32>(vec[1].data, vec[1].len);
        if (mode == nullptr) {
            PRINT(IOS_EmuFS, ERROR, "ExOpen: Invalid mode vector");
            return ISFS::ISFSError::INVALID;
        }

        std::memcpy(s_efsPath2, vec[0].data, vec[0].len);
        if (!IsEmuFSPathValid(s_efsPath2)) {
            PRINT(IOS_EmuFS, ERROR, "ExOpen: Invalid path");
            return ISFS::ISFSError::INVALID;
        }

        if (PathElementCompare(s_efsPath2 + 1, "dev")) {
            // Don't let the caller open a resource manager
            return IOS::IOSError::INVALID;
        }

        // Reset the handle as we're converting it to a file handle
        CloseBackend();

        return OpenFile(s_efsPath2, *mode, m_uid, m_gid, false);
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "Unknown manager ioctlv: %u", command);
        return ISFS::ISFSError::INVALID;
    }
}

static s32 HandleRequest(IOS::Request* req)
{
    s32 ret = ISFS::ISFSError::INVALID;
    s32 fd = req->fd;
    EmuFSHandle* handle = nullptr;
    if (req->cmd != IOS::Cmd::OPEN) {
        ASSERT(fd >= 0 && fd < ISFS::MAX_OPEN_COUNT);

        handle = &s_handles[fd];
        ASSERT(handle != nullptr);
        ASSERT(handle->m_inUse);
    }

    switch (req->cmd) {
    case IOS::Cmd::OPEN: {
        char path[ISFS::MAX_PATH_LENGTH] = {};
        std::memcpy(path, req->open.path, ISFS::MAX_PATH_LENGTH);
        if (path[ISFS::MAX_PATH_LENGTH - 1] != 0) {
            ret = ISFS::ISFSError::INVALID;
            break;
        }

        if (path[0] != ISFS::SEPARATOR_CHAR && path[0] != '$') {
            // Not handled here, fall through to the next resource
            ret = IOS::IOSError::NOT_FOUND;
            break;
        }
        path[0] = ISFS::SEPARATOR_CHAR;

        if (PathElementCompare(path + 1, "dev") == 0 &&
            (path[4] == '\0' || PathElementCompare(path + 5, "fs") != 0)) {
            // Not handled here, fall through to the next resource
            ret = IOS::IOSError::NOT_FOUND;
            break;
        }

        // Basic sanity check
        if (req->open.mode > IOS::Mode::READ_WRITE) {
            ret = ISFS::ISFSError::INVALID;
            break;
        }

        // Check if the file is already open
        s32 ret = EmuFSHandle::FindProxyHandle(path);
        if (ret < 0) {
            break;
        }
        fd = ret;

        if (fd < ISFS::MAX_OPEN_COUNT) {
            if (s_handles[fd].m_inUse) {
                ret = ISFS::ISFSError::LOCKED;
                break;
            }

            // Reopen cached file
            handle = &s_handles[fd];
            ret = handle->Reopen();
            if (ret != ISFS::ISFSError::OK) {
                break;
            }

            ret = fd;
            break;
        }

        // Open a new file
        ret = EmuFSHandle::FindFreeHandle();
        if (ret < 0) {
            break;
        }
        fd = ret;

        if (fd >= ISFS::MAX_OPEN_COUNT) {
            ret = ISFS::ISFSError::MAX_HANDLES_OPEN;
            break;
        }
        handle = &s_handles[fd];

        // Reset the handle
        {
            handle->~EmuFSHandle();
            new (handle) EmuFSHandle();
            handle->m_fd = fd;
        }

        ret = handle->OpenFile(
            path, req->open.mode, req->open.uid, req->open.gid, true
        );
        if (ret != ISFS::ISFSError::OK) {
            handle->~EmuFSHandle();
            break;
        }

        ret = fd;
        break;
    }

    case IOS::Cmd::CLOSE:
        PRINT(IOS_EmuFS, INFO, "IOS_Close(%d)", fd);
        ret = handle->Close();
        break;

    case IOS::Cmd::READ:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Read(%d, 0x%08X, 0x%X)", fd, req->read.data,
            req->read.len
        );
        ret = handle->Read(req->read.data, req->read.len);
        break;

    case IOS::Cmd::WRITE:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Write(%d, 0x%08X, 0x%X)", fd, req->write.data,
            req->write.len
        );
        ret = handle->Write(req->write.data, req->write.len);
        break;

    case IOS::Cmd::SEEK:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Seek(%d, %d, %d)", fd, req->seek.where,
            req->seek.whence
        );
        ret = handle->Seek(req->seek.where, req->seek.whence);
        break;

    case IOS::Cmd::IOCTL:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Ioctl(%d, %d, 0x%08X, 0x%X, 0x%08X, 0x%X)",
            fd, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len,
            req->ioctl.out, req->ioctl.out_len
        );
        ret = handle->Ioctl(
            static_cast<ISFS::ISFSIoctl>(req->ioctl.cmd), req->ioctl.in,
            req->ioctl.in_len, req->ioctl.out, req->ioctl.out_len
        );
        break;

    case IOS::Cmd::IOCTLV:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Ioctlv(%d, %d, %d, %d, 0x%08X)", fd,
            req->ioctlv.cmd, req->ioctlv.in_count, req->ioctlv.out_count,
            req->ioctlv.vec
        );
        ret = handle->Ioctlv(
            static_cast<ISFS::ISFSIoctl>(req->ioctlv.cmd), req->ioctlv.in_count,
            req->ioctlv.out_count, req->ioctlv.vec
        );
        break;

    default:
        PRINT(
            IOS_EmuFS, ERROR, "Unknown command: %u", static_cast<u32>(req->cmd)
        );
        ret = ISFS::ISFSError::INVALID;
        break;
    }

    PRINT(IOS_EmuFS, INFO, "Reply: %d", ret);

    return ret;
}

static Queue<IOS::Request*> s_ipcQueue;

static s32 ThreadEntry([[maybe_unused]] void* arg)
{
    PRINT(IOS_EmuFS, INFO, "Starting FS...");
    PRINT(IOS_EmuFS, INFO, "EmuFS thread ID: %d", IOS_GetThreadId());

    while (true) {
        IOS::Request* request = s_ipcQueue.Receive();
        request->Reply(HandleRequest(request));
    }

    // Can never reach here
    return 0;
}

void DeviceEmuFS::Init()
{
    // The IOS_Open patch changes the first `/` to `$`
    s32 ret = IOS_RegisterResourceManager("$", s_ipcQueue.GetID());
    if (ret != IOS::IOSError::OK) {
        PRINT(IOS_EmuFS, ERROR, "IOS_RegisterResourceManager failed: %d", ret);
        System::Abort();
    }

    // Reset handles
    for (u32 i = 0; i < s_handles.size(); i++) {
        s_handles[i].~EmuFSHandle();
    }

    new Thread(ThreadEntry, nullptr, nullptr, 0x2000, 80);
}
