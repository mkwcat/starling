// DeviceEmuFS.cpp - Emulated IOS filesystem RM
//   Written by Star
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

/*
 * ISFS (Internal Storage FileSystem) is split into two parts:
 *
 * Files; these are opened directly using for example
 * IOS_Open("/tmp/file.bin"). These can be interacted with using seek, read,
 * write, and one ioctl (GetFileStats).
 *
 * Manager (/dev/fs); this takes ioctl commands to do general tasks like create
 * file, read directory, etc.
 *
 * File descriptor: {
 * 0 .. 99: Reserved for proxy/replaced files
 * 100 .. 199: Reserved for real FS files (not used)
 * 200 .. 232: Proxy /dev/fs
 * 300 .. 399: Reserved for direct file access
 * }
 *
 * The manager is blocked from using read, write, seek automatically from the
 * Proxy_IsValidFile check.
 */

constexpr int HANDLE_PROXY_BASE = 0;
constexpr int HANDLE_PROXY_NUM = ISFS::MAX_OPEN_COUNT;

constexpr int REAL_HANDLE_BASE = 100;
constexpr int REAL_HANDLE_MAX = ISFS::MAX_OPEN_COUNT;

constexpr int MANAGER_HANDLE_BASE = 200;
// Not sure the actual limit so we'll allow up to 32 (the actual limit will be
// enforced by real FS after this check).
constexpr int MANAGER_HANDLE_MAX = 32;

constexpr int DIRECT_HANDLE_BASE = 300;
constexpr int DIRECT_HANDLE_MAX = ISFS::MAX_OPEN_COUNT;

#define EFS_DRIVE "0:"

static char s_efsPath[ISFS::EMUFS_MAX_PATH_LENGTH];
static char s_efsPath2[ISFS::EMUFS_MAX_PATH_LENGTH];

struct ProxyFile {
    bool ipcFile;
    bool inUse;
    bool filOpened;
    char path[64];
    u32 mode;
    // TODO: Use a std::variant for this
    bool isDir;

    union {
        FIL fil;
        DIR dir;
    };
};

static std::array<ProxyFile, ISFS::MAX_OPEN_COUNT> s_fileArray;

struct DirectFile {
    bool inUse;
    int fd;
};

static std::array<DirectFile, DIRECT_HANDLE_MAX> s_directFileArray;

static std::array<IOS::ResourceCtrl<ISFS::ISFSIoctl>, MANAGER_HANDLE_MAX>
    s_managers;

enum class HandleType {
    PROXY,
    REAL,
    MANAGER,
    DIRECT,
    UNKNOWN,
};

static HandleType GetHandleType(s32 fd)
{
    if (fd >= HANDLE_PROXY_BASE && fd < HANDLE_PROXY_BASE + HANDLE_PROXY_NUM)
        return HandleType::PROXY;

    if (fd >= REAL_HANDLE_BASE && fd < REAL_HANDLE_BASE + REAL_HANDLE_MAX)
        return HandleType::REAL;

    if (fd >= MANAGER_HANDLE_BASE &&
        fd < MANAGER_HANDLE_BASE + MANAGER_HANDLE_MAX)
        return HandleType::MANAGER;

    if (fd >= DIRECT_HANDLE_BASE && fd < DIRECT_HANDLE_BASE + DIRECT_HANDLE_MAX)
        return HandleType::DIRECT;

    return HandleType::UNKNOWN;
}

static s32 GetHandleIndex(s32 fd)
{
    switch (GetHandleType(fd)) {
    case HandleType::PROXY:
        return fd - HANDLE_PROXY_BASE;

    case HandleType::REAL:
        return fd - REAL_HANDLE_BASE;

    case HandleType::MANAGER:
        return fd - MANAGER_HANDLE_BASE;

    case HandleType::DIRECT:
        return GetHandleIndex(fd);

    default:
        ASSERT(!"Invalid file descriptor");
        return -1;
    }
}

static s32 FResultToISFSError(FRESULT fret)
{
    switch (fret) {
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

    if (mode & IOS::Mode::READ)
        out |= FA_READ;

    if (mode & IOS::Mode::WRITE)
        out |= FA_WRITE;

    return out;
}

static IOS::ResourceCtrl<ISFS::ISFSIoctl>* GetManagerResource(s32 fd)
{
    if (GetHandleType(fd) != HandleType::MANAGER) {
        return nullptr;
    }

    auto resource = &s_managers[GetHandleIndex(fd)];
    ASSERT(resource->GetFd() >= 0);

    return resource;
}

/**
 * Check if a file descriptor is valid.
 */
static bool Proxy_IsValidFile(int fd)
{
    if (fd < 0 || fd >= static_cast<int>(s_fileArray.size()))
        return false;

    if (!s_fileArray[fd].inUse)
        return false;

    if (s_fileArray[fd].isDir)
        return false;

    return true;
}

/**
 * Check if a file descriptor is a valid directory.
 */
static bool Proxy_IsValidDir(int fd)
{
    if (fd < 0 || fd >= static_cast<int>(s_fileArray.size()))
        return false;

    if (!s_fileArray[fd].inUse)
        return false;

    if (!s_fileArray[fd].isDir)
        return false;

    return true;
}

static int Proxy_RegisterHandle(const char* path)
{
    int match = 0;

    for (int i = 0; i < ISFS::MAX_OPEN_COUNT; i++) {
        // If the file was already opened, reuse the descriptor
        if (s_fileArray[i].filOpened && s_fileArray[i].ipcFile &&
            !strcmp(s_fileArray[i].path, path)) {

            if (s_fileArray[i].inUse)
                return ISFS::ISFSError::LOCKED;

            s_fileArray[i].inUse = true;
            return i;
        }

        if (!s_fileArray[i].inUse && s_fileArray[match].inUse)
            match = i;

        if (!s_fileArray[i].filOpened && s_fileArray[match].filOpened)
            match = i;
    }

    if (s_fileArray[match].inUse)
        return ISFS::ISFSError::MAX_HANDLES_OPEN;

    // Close and use the file descriptor

    if (s_fileArray[match].filOpened)
        f_close(&s_fileArray[match].fil);

    s_fileArray[match].filOpened = false;
    s_fileArray[match].inUse = true;
    s_fileArray[match].ipcFile = true;
    strncpy(s_fileArray[match].path, path, 64);

    return match;
}

static void Proxy_FreeHandle(int fd)
{
    if (!Proxy_IsValidFile(fd))
        return;

    s_fileArray[fd].inUse = false;
}

static int Proxy_FindHandle(const char* path)
{
    for (int i = 0; i < ISFS::MAX_OPEN_COUNT; i++) {
        if (s_fileArray[i].filOpened && !strcmp(path, s_fileArray[i].path))
            return i;
    }

    return ISFS::MAX_OPEN_COUNT;
}

static int Proxy_FindFreeHandle()
{
    int match = 0;

    for (int i = 0; i < ISFS::MAX_OPEN_COUNT; i++) {
        if (!s_fileArray[i].inUse && s_fileArray[match].inUse)
            match = i;

        if (!s_fileArray[i].filOpened && s_fileArray[match].filOpened)
            match = i;
    }

    if (s_fileArray[match].inUse)
        return ISFS::ISFSError::MAX_HANDLES_OPEN;

    return match;
}

static s32 Proxy_TryCloseHandle(int fd)
{
    if (s_fileArray[fd].inUse)
        return ISFS::ISFSError::LOCKED;

    if (!s_fileArray[fd].filOpened)
        return ISFS::ISFSError::OK;

    const FRESULT fret = f_close(&s_fileArray[fd].fil);
    if (fret != FR_OK) {
        PRINT(IOS_EmuFS, ERROR, "Failed to close file, error: %d", fret);
        return FResultToISFSError(fret);
    }

    s_fileArray[fd].filOpened = false;
    return FR_OK;
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
 * Checks if an ISFS path is valid.
 */
static bool IsPathValid(const char* path)
{
    if (!path) {
        return false;
    }

    if (path[0] != ISFS::SEPARATOR_CHAR) {
        return false;
    }

    return strnlen(path, ISFS::MAX_PATH_LENGTH) < ISFS::MAX_PATH_LENGTH;
}

/**
 * Checks if a path is redirected somewhere else by the frontend.
 */
static bool IsPathReplaced(const char* path)
{
    if (!IsPathValid(path)) {
        return false;
    }

    return Config::s_instance->IsISFSPathReplaced(path);
}

/**
 * Get the replaced path from an ISFS path.
 */
static const char* GetRedirectedPath(const char* path, char* out, u32 outLen)
{
    if (!IsPathValid(path)) {
        return nullptr;
    }

    if (out == nullptr) {
        return nullptr;
    }

    // Create and write the replaced path
    path = strchr(path, ISFS::SEPARATOR_CHAR);
    snprintf(out, outLen, EFS_DRIVE "%s", path);

    return out;
}

/**
 * Get the redirected path if it's replaced.
 * @returns true if replaced, false if not.
 */
static bool GetPathIfReplaced(const char* path, char* out, u32 outLen)
{
    if (!IsPathReplaced(path)) {
        return false;
    }

    auto ret = GetRedirectedPath(path, out, outLen);
    ASSERT(ret != nullptr);

    return true;
}

static u8 efsCopyBuffer[0x2000] ATTRIBUTE_ALIGN(32); // 8 KB

/**
 * Copy a file from ISFS to an external filesystem.
 */
static s32 CopyFromNandToEFS(const char* nandPath, FIL& fil)
{
    // Only allow renaming files from /tmp
    if (strncmp(nandPath, "/tmp", 4) != 0) {
        PRINT(
            IOS_EmuFS, ERROR, "Attempting to rename a file from outside of /tmp"
        );
        return ISFS::ISFSError::ACCESS_DENIED;
    }

    IOS::File isfsFile(nandPath, IOS::Mode::READ);

    if (isfsFile.GetFd() < 0) {
        PRINT(
            IOS_EmuFS, ERROR, "Failed to open ISFS file: %d", isfsFile.GetFd()
        );
        return isfsFile.GetFd();
    }

    s32 size = isfsFile.GetSize();
    PRINT(IOS_EmuFS, INFO, "File size: 0x%X", size);

    for (s32 pos = 0; pos < size; pos += sizeof(efsCopyBuffer)) {
        u32 readlen = size - pos;
        if (readlen > sizeof(efsCopyBuffer))
            readlen = sizeof(efsCopyBuffer);

        s32 ret = isfsFile.Read(efsCopyBuffer, readlen);

        if ((u32) ret != readlen) {
            f_close(&fil);
            PRINT(
                IOS_EmuFS, ERROR, "Failed to read from ISFS file: %d != %d",
                ret, readlen
            );
            if (ret < 0)
                return ret;
            return ISFS::ISFSError::UNKNOWN;
        }

        UINT bw;
        auto fret = f_write(&fil, efsCopyBuffer, readlen, &bw);

        if (fret != FR_OK || (u32) bw != readlen) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Failed to write to EFS file: %d != 0 OR %d != %d", fret,
                readlen, bw
            );
            if (fret != FR_OK)
                return FResultToISFSError(fret);
            return ISFS::ISFSError::UNKNOWN;
        }
    }

    return ISFS::ISFSError::OK;
}

/**
 * Reset a cached file handle.
 */
static s32 ReopenFile(s32 fd)
{
    const FRESULT fret = f_lseek(&s_fileArray[fd].fil, 0);
    if (fret != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR,
            "Failed to seek to position 0x%08X in file descriptor %d", 0, fd
        );

        Proxy_FreeHandle(fd);
        return FResultToISFSError(fret);
    }
    return fd;
}

/**
 * Handle open file request from the filesystem proxy.
 * @returns File descriptor, or ISFS error code.
 */
static s32 ReqProxyOpen(const char* path, u32 mode)
{
    if (mode > IOS::Mode::READ_WRITE) {
        return ISFS::ISFSError::INVALID;
    }

    // Get the replaced path
    if (!GetRedirectedPath(path, s_efsPath, sizeof(s_efsPath)))
        return ISFS::ISFSError::INVALID;

    int fd = Proxy_RegisterHandle(path);
    if (fd < 0) {
        PRINT(IOS_EmuFS, ERROR, "Could not register handle: %d", fd);
        return fd;
    }
    PRINT(IOS_EmuFS, INFO, "Registered handle %d", fd);

    ASSERT(Proxy_IsValidFile(fd));

    s_fileArray[fd].mode = mode;

    if (s_fileArray[fd].filOpened) {
        PRINT(IOS_EmuFS, INFO, "File already open, reusing handle");
        return ReopenFile(fd);
    }

    const FRESULT fret =
        f_open(&s_fileArray[fd].fil, s_efsPath, FA_READ | FA_WRITE);
    if (fret != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR, "Failed to open file '%s', error: %d", s_efsPath,
            fret
        );

        Proxy_FreeHandle(fd);
        return FResultToISFSError(fret);
    }

    s_fileArray[fd].filOpened = true;

    PRINT(
        IOS_EmuFS, INFO, "Opened file '%s' (fd=%d, mode=%u)", s_efsPath, fd,
        mode
    );

    return fd;
}

/**
 * Handles direct open file requests.
 * @returns File descriptor, or ISFS error code.
 */
static s32 ReqDirectOpen(const char* path, u32 mode)
{
    int fd = Proxy_FindFreeHandle();
    if (fd < 0) {
        PRINT(IOS_EmuFS, ERROR, "Could not find an open handle");
        return fd;
    }

    s_fileArray[fd].inUse = false;
    s_fileArray[fd].filOpened = false;
    memset(s_fileArray[fd].path, 0, 64);

    const FRESULT fret =
        f_open(&s_fileArray[fd].fil, path, ISFSModeToFileMode(mode));
    if (fret != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR, "Failed to open file '%s', mode: %X, error: %d",
            path, mode, fret
        );
        return FResultToISFSError(fret);
    }

    s_fileArray[fd].mode = mode;
    s_fileArray[fd].inUse = true;
    s_fileArray[fd].isDir = false;
    s_fileArray[fd].filOpened = true;

    PRINT(IOS_EmuFS, INFO, "Opened file '%s' (fd=%d, mode=%u)", path, fd, mode);

    return fd;
}

/**
 * Handles direct open directory requests.
 * @returns File descriptor, or ISFS error code.
 */
static s32 ReqDirectOpenDir(const char* path)
{
    int fd = Proxy_FindFreeHandle();
    if (fd < 0) {
        PRINT(IOS_EmuFS, ERROR, "Could not find an open handle");
        return fd;
    }

    s_fileArray[fd].inUse = false;
    s_fileArray[fd].filOpened = false;

    const FRESULT fret = f_opendir(&s_fileArray[fd].dir, path);
    if (fret != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR, "Failed to open dir '%s' error: %d", path, fret
        );
        return FResultToISFSError(fret);
    }

    s_fileArray[fd].inUse = true;
    s_fileArray[fd].isDir = true;

    PRINT(IOS_EmuFS, INFO, "Opened directory '%s' (fd=%d)", path, fd);

    return fd;
}

/**
 * Close open file descriptor.
 * @returns IOS::IOSError::OK for success, or IOS/ISFS error code.
 */
static s32 ReqClose(s32 fd)
{
    if (GetHandleType(fd) == HandleType::MANAGER) {
        s32 ret = GetManagerResource(fd)->Close();
        ASSERT(ret == IOS::IOSError::OK);
        return IOS::IOSError::OK;
    }

    auto type = GetHandleType(fd);

    if (type == HandleType::DIRECT) {
        PRINT(IOS_EmuFS, INFO, "Closing direct handle %d", fd);
        if (!s_directFileArray[GetHandleIndex(fd)].inUse)
            return IOS::IOSError::OK;

        s32 realFd = s_directFileArray[GetHandleIndex(fd)].fd;
        s_directFileArray[GetHandleIndex(fd)].inUse = false;
        s_directFileArray[GetHandleIndex(fd)].fd = ISFS::ISFSError::NOT_FOUND;

        if (s_fileArray[realFd].isDir)
            return IOS::IOSError::OK;

        fd = realFd;

        if (!Proxy_IsValidFile(fd))
            return ISFS::ISFSError::INVALID;

        if (f_close(&s_fileArray[fd].fil) != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "Failed to close handle %d", fd);
            return ISFS::ISFSError::UNKNOWN;
        }

        s_fileArray[fd].filOpened = false;
        Proxy_FreeHandle(fd);
    } else {
        if (!Proxy_IsValidFile(fd))
            return ISFS::ISFSError::INVALID;

        if (f_sync(&s_fileArray[fd].fil) != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "Failed to sync handle %d", fd);
            return ISFS::ISFSError::UNKNOWN;
        }

        Proxy_FreeHandle(fd);
    }

    PRINT(IOS_EmuFS, INFO, "Successfully closed handle %d", fd);

    return ISFS::ISFSError::OK;
}

/**
 * Read data from open file descriptor.
 * @returns Amount read, or ISFS error code.
 */
static s32 ReqRead(s32 fd, void* data, u32 len)
{
    if (!Proxy_IsValidFile(fd)) {
        return ISFS::ISFSError::INVALID;
    }

    if (len == 0) {
        return ISFS::ISFSError::OK;
    }

    if (!(s_fileArray[fd].mode & IOS::Mode::READ)) {
        return ISFS::ISFSError::ACCESS_DENIED;
    }

    unsigned int bytesRead;
    const FRESULT fret = f_read(&s_fileArray[fd].fil, data, len, &bytesRead);
    if (fret != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR,
            "Failed to read %u bytes from handle %d, error: %d", len, fd, fret
        );
        return FResultToISFSError(fret);
    }

    PRINT(IOS_EmuFS, INFO, "Read %u bytes from handle %d", bytesRead, fd);

    return bytesRead;
}

/**
 * Write data to open file descriptor.
 * @returns Amount wrote, or ISFS error code.
 */
static s32 ReqWrite(s32 fd, const void* data, u32 len)
{
    if (!Proxy_IsValidFile(fd)) {
        return ISFS::ISFSError::INVALID;
    }

    if (len == 0) {
        return ISFS::ISFSError::OK;
    }

    if (!(s_fileArray[fd].mode & IOS::Mode::WRITE)) {
        return ISFS::ISFSError::ACCESS_DENIED;
    }

    u32 bytesWrote;
    const FRESULT fret = f_write(&s_fileArray[fd].fil, data, len, &bytesWrote);
    if (fret != FR_OK) {
        PRINT(
            IOS_EmuFS, ERROR,
            "Failed to write %u bytes to handle %d, error: %d", len, fd, fret
        );
        return FResultToISFSError(fret);
    }

    PRINT(IOS_EmuFS, INFO, "Wrote %u bytes to handle %d", bytesWrote, fd);

    return bytesWrote;
}

/**
 * Moves the file read/write position of an open file descriptor.
 * @returns New offset, or an ISFS error code.
 */
static s32 ReqSeek(s32 fd, s32 where, s32 whence)
{
    if (!Proxy_IsValidFile(fd)) {
        return ISFS::ISFSError::INVALID;
    }

    if (whence < IOS_SEEK_SET || whence > IOS_SEEK_END) {
        return ISFS::ISFSError::INVALID;
    }

    FIL* fil = &s_fileArray[fd].fil;
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
            IOS_EmuFS, ERROR, "Failed to seek to position 0x%08X in handle %d",
            offset, fd
        );
        return FResultToISFSError(fresult);
    }

    PRINT(
        IOS_EmuFS, INFO, "Successfully seeked to position 0x%08X in handle %d",
        offset, fd
    );

    return offset;
}

/**
 * Handles filesystem ioctl commands.
 * @returns ISFS::ISFSError result.
 */
static s32 ReqIoctl(
    s32 fd, ISFS::ISFSIoctl cmd, void* in, u32 in_len, void* io, u32 io_len
)
{
    if (in_len == 0)
        in = nullptr;
    if (io_len == 0)
        io = nullptr;

    // File commands
    if (Proxy_IsValidFile(fd)) {
        if (cmd == ISFS::ISFSIoctl::GET_FILE_STATS) {
            if (io_len < sizeof(IOS::File::Stats))
                return ISFS::ISFSError::INVALID;
            // Real FS doesn't seem to even check alignment before writing, but
            // I'd rather not have the whole of IOS panic over an alignment
            // exception.
            if (!IsAligned(io, 4)) {
                PRINT(IOS_EmuFS, ERROR, "Invalid GetFileStats input alignment");
                return ISFS::ISFSError::INVALID;
            }
            IOS::File::Stats* stat = reinterpret_cast<IOS::File::Stats*>(io);
            stat->size = f_size(&s_fileArray[fd].fil);
            stat->pos = f_tell(&s_fileArray[fd].fil);
            return ISFS::ISFSError::OK;
        }

        PRINT(
            IOS_EmuFS, ERROR, "Unknown file ioctl: %u", static_cast<s32>(cmd)
        );
        return ISFS::ISFSError::INVALID;
    }

    // Manager commands!
    if (GetHandleType(fd) != HandleType::MANAGER) {
        // ...oh, nevermind :(
        return ISFS::ISFSError::INVALID;
    }

    auto manager = GetManagerResource(fd);

    // TODO Add ISFS_Shutdown
    switch (cmd) {
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
        if (!IsAligned(in, 4)) {
            return ISFS::ISFSError::INVALID;
        }

        if (in_len < sizeof(ISFS::AttrBlock)) {
            return ISFS::ISFSError::INVALID;
        }

        ISFS::AttrBlock* isfsAttrBlock = (ISFS::AttrBlock*) in;

        const char* path = isfsAttrBlock->path;

        // Check if the path is valid
        if (!IsPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        // Get the replaced path
        if (!GetPathIfReplaced(path, s_efsPath, sizeof(s_efsPath))) {
            return manager->Ioctl(
                ISFS::ISFSIoctl::CREATE_DIR, in, in_len, io, io_len
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

    // [ISFS_SetAttr]
    // in: Accepts ISFS::AttrBlock. All fields are read. If the caller's UID is
    // not zero, ownerID and groupID must be equal to the caller's. Otherwise,
    // throw ISFS::ISFSError::ACCESS_DENIED.
    // out: not used
    case ISFS::ISFSIoctl::SET_ATTR: {
        if (!IsAligned(in, 4)) {
            return ISFS::ISFSError::INVALID;
        }

        if (in_len < sizeof(ISFS::AttrBlock)) {
            return ISFS::ISFSError::INVALID;
        }

        ISFS::AttrBlock* isfsAttrBlock = (ISFS::AttrBlock*) in;

        const char* path = isfsAttrBlock->path;

        // Check if the path is valid
        if (!IsPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        // Get the replaced path
        if (!GetPathIfReplaced(path, s_efsPath, sizeof(s_efsPath))) {
            return manager->Ioctl(
                ISFS::ISFSIoctl::SET_ATTR, in, in_len, io, io_len
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
            IOS_EmuFS, INFO,
            "SetAttr: Set attributes for file or directory '%s'", s_efsPath
        );

        return ISFS::ISFSError::OK;
    }

    // [ISFS_GetAttr]
    // in: Path to a file or directory.
    // out: File/directory's attributes (ISFS::AttrBlock).
    case ISFS::ISFSIoctl::GET_ATTR: {
        const u8 OWNER_PERM = 3;
        const u8 GROUP_PERM = 3;
        const u8 OTHER_PERM = 1;
        const u8 ATTRIBUTES = 0;

        if (!IsAligned(in, 4) || !IsAligned(io, 4)) {
            return ISFS::ISFSError::INVALID;
        }

        if (in_len < ISFS::MAX_PATH_LENGTH ||
            io_len < sizeof(ISFS::AttrBlock)) {
            return ISFS::ISFSError::INVALID;
        }

        const char* path = (const char*) in;

        // Check if the path is valid
        if (!IsPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        // Get the replaced path
        if (!GetPathIfReplaced(path, s_efsPath, sizeof(s_efsPath))) {
            return manager->Ioctl(
                ISFS::ISFSIoctl::GET_ATTR, in, in_len, io, io_len
            );
        }

        const FRESULT fresult = f_stat(s_efsPath, nullptr);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "GetAttr: Failed to get attributes for file or directory '%s'",
                s_efsPath
            );
            return FResultToISFSError(fresult);
        }

        ISFS::AttrBlock* isfsAttrBlock = (ISFS::AttrBlock*) io;
        isfsAttrBlock->ownerId = IOS_GetUid();
        isfsAttrBlock->groupId = IOS_GetGid();
        strcpy(isfsAttrBlock->path, path);
        isfsAttrBlock->ownerPerm = OWNER_PERM;
        isfsAttrBlock->groupPerm = GROUP_PERM;
        isfsAttrBlock->otherPerm = OTHER_PERM;
        isfsAttrBlock->attributes = ATTRIBUTES;

        PRINT(
            IOS_EmuFS, INFO,
            "GetAttr: Got attributes for file or directory '%s'", s_efsPath
        );

        return ISFS::ISFSError::OK;
    }

    // [ISFS_Delete]
    // in: Path to the file or directory to delete.
    // out: not used
    case ISFS::ISFSIoctl::DELETE: {
        if (!IsAligned(in, 4)) {
            return ISFS::ISFSError::INVALID;
        }

        if (in_len < ISFS::MAX_PATH_LENGTH) {
            return ISFS::ISFSError::INVALID;
        }

        const char* path = (const char*) in;

        // Check if the path is valid
        if (!IsPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        // Check if the path should be replaced
        if (!IsPathReplaced(path)) {
            return manager->Ioctl(
                ISFS::ISFSIoctl::DELETE, in, in_len, io, io_len
            );
        }

        s32 ret = Proxy_FindHandle(path);
        if (ret < 0) {
            return ret;
        }

        if (ret != ISFS::MAX_OPEN_COUNT) {
            ret = Proxy_TryCloseHandle(ret);
            if (ret != ISFS::ISFSError::OK) {
                return ret;
            }
        }

        // Get the replaced path
        if (!GetRedirectedPath(path, s_efsPath, sizeof(s_efsPath))) {
            return ISFS::ISFSError::INVALID;
        }

        const FRESULT fresult = f_unlink(s_efsPath);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Delete: Failed to delete file or directory '%s'", s_efsPath
            );
            return FResultToISFSError(fresult);
        }

        PRINT(
            IOS_EmuFS, INFO, "Delete: Deleted file or directory '%s'", s_efsPath
        );

        return ISFS::ISFSError::OK;
    }

    // [ISFS_Rename]
    // in: ISFS::RenameBlock.
    // out: not used
    case ISFS::ISFSIoctl::RENAME: {
        if (!IsAligned(in, 4)) {
            return ISFS::ISFSError::INVALID;
        }

        if (in_len < sizeof(ISFS::RenameBlock)) {
            return ISFS::ISFSError::INVALID;
        }

        ISFS::RenameBlock* isfsRenameBlock = (ISFS::RenameBlock*) in;

        const char* pathOld = isfsRenameBlock->pathOld;
        const char* pathNew = isfsRenameBlock->pathNew;
        PRINT(
            IOS_EmuFS, INFO, "Rename: ISFS_Rename(\"%s\", \"%s\")", pathOld,
            pathNew
        );

        // Check if the old and new paths are valid
        if (!IsPathValid(pathOld) || !IsPathValid(pathNew)) {
            return ISFS::ISFSError::INVALID;
        }

        const bool isOldPathReplaced = IsPathReplaced(pathOld);
        const bool isNewPathReplaced = IsPathReplaced(pathNew);

        // Neither of the paths are replaced
        if (!isOldPathReplaced && !isNewPathReplaced) {
            return manager->Ioctl(
                ISFS::ISFSIoctl::RENAME, in, in_len, io, io_len
            );
        }

        char* efsOldPath = s_efsPath;
        char* efsNewPath = s_efsPath2;

        // Rename from NAND to EFS file
        if (!isOldPathReplaced && isNewPathReplaced) {
            // Check if the file is already open somewhere
            int openFd = Proxy_FindHandle(pathNew);

            s32 ret;
            if (openFd < 0 || openFd >= static_cast<int>(s_fileArray.size())) {
                // File is not open
                if (!GetRedirectedPath(
                        pathNew, efsNewPath, ISFS::EMUFS_MAX_PATH_LENGTH
                    )) {
                    return ISFS::ISFSError::INVALID;
                }

                FIL destFil;
                auto fret =
                    f_open(&destFil, efsNewPath, FA_WRITE | FA_CREATE_ALWAYS);
                if (fret != FR_OK) {
                    return FResultToISFSError(fret);
                }

                ret = CopyFromNandToEFS(pathOld, destFil);
                fret = f_close(&destFil);
                ASSERT(fret == FR_OK);
            } else {
                // File is open
                ASSERT(s_fileArray[openFd].filOpened);

                if (s_fileArray[openFd].inUse) {
                    return ISFS::ISFSError::LOCKED;
                }

                // Seek back to the beginning
                auto fret = f_lseek(&s_fileArray[openFd].fil, 0);
                if (fret != FR_OK) {
                    return FResultToISFSError(fret);
                }

                // Truncate from the beginning of the file
                fret = f_truncate(&s_fileArray[openFd].fil);
                if (fret != FR_OK) {
                    return FResultToISFSError(fret);
                }

                ret = CopyFromNandToEFS(pathOld, s_fileArray[openFd].fil);
                fret = f_sync(&s_fileArray[openFd].fil);
                ASSERT(fret == FR_OK);
            }

            if (ret != ISFS::ISFSError::OK) {
                return ret;
            }

            ret = manager->Ioctl(
                ISFS::ISFSIoctl::DELETE, const_cast<char*>(pathOld),
                ISFS::MAX_PATH_LENGTH, nullptr, 0
            );
            return ret;
        }

        // Other way not supported (yet?)
        if (isOldPathReplaced ^ isNewPathReplaced) {
            return ISFS::ISFSError::INVALID;
        }

        // Both of the paths are replaced

        // Get the replaced paths
        if (!GetRedirectedPath(
                pathOld, efsOldPath, ISFS::EMUFS_MAX_PATH_LENGTH
            ) ||
            !GetRedirectedPath(
                pathNew, efsNewPath, ISFS::EMUFS_MAX_PATH_LENGTH
            )) {
            return ISFS::ISFSError::INVALID;
        }

        const FRESULT fresult = f_rename(efsOldPath, efsNewPath);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "Rename: Failed to rename file or directory '%s' to '%s'",
                efsOldPath, efsNewPath
            );
            return FResultToISFSError(fresult);
        }

        PRINT(
            IOS_EmuFS, INFO, "Rename: Renamed file or directory '%s' to '%s'",
            efsOldPath, efsNewPath
        );

        return ISFS::ISFSError::OK;
    }

    // [ISFS_CreateFile]
    // in: Accepts ISFS::AttrBlock. Reads path, ownerPerm, groupPerm, otherPerm,
    // and attributes.
    // out: not used
    case ISFS::ISFSIoctl::CREATE_FILE: {
        if (!IsAligned(in, 4)) {
            return ISFS::ISFSError::INVALID;
        }

        if (in_len < sizeof(ISFS::AttrBlock)) {
            return ISFS::ISFSError::INVALID;
        }

        ISFS::AttrBlock* isfsAttrBlock = (ISFS::AttrBlock*) in;

        const char* path = isfsAttrBlock->path;

        PRINT(IOS_EmuFS, INFO, "CreateFile: ISFS_CreateFile(\"%s\")", path);

        // Check if the path is valid
        if (!IsPathValid(path)) {
            return ISFS::ISFSError::INVALID;
        }

        // Get the replaced path
        if (!GetPathIfReplaced(path, s_efsPath, sizeof(s_efsPath))) {
            return manager->Ioctl(
                ISFS::ISFSIoctl::CREATE_FILE, in, in_len, io, io_len
            );
        }

        FIL fil;
        const FRESULT fresult =
            f_open(&fil, s_efsPath, FA_CREATE_NEW | FA_READ | FA_WRITE);
        if (fresult != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR, "CreateFile: Failed to create file '%s'",
                s_efsPath
            );
            return FResultToISFSError(fresult);
        }

        f_sync(&fil);

        s32 ret = Proxy_FindFreeHandle();
        if (ret >= 0 && ret < ISFS::MAX_OPEN_COUNT) {
            s_fileArray[ret].filOpened = true;
            s_fileArray[ret].fil = fil; // Copy
        }

        PRINT(IOS_EmuFS, INFO, "CreateFile: Created file '%s'", s_efsPath);

        return ISFS::ISFSError::OK;
    }

    // [ISFS_Shutdown]
    case ISFS::ISFSIoctl::SHUTDOWN: {
        // This command is called to wait for any in-progress file operations to
        // be completed before shutting down
        PRINT(IOS_EmuFS, INFO, "Shutdown: ISFS_Shutdown()");
        return ISFS::ISFSError::OK;
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "Unknown manager ioctl: %u", cmd);
        return ISFS::ISFSError::INVALID;
    }
}

/**
 * Handles filesystem ioctlv commands.
 * @returns ISFS::ISFSError result.
 */
static s32 ReqIoctlv(
    s32 fd, ISFS::ISFSIoctl cmd, u32 in_count, u32 out_count, IOS::TVector* vec
)
{
    if (in_count >= 32 || out_count >= 32)
        return ISFS::ISFSError::INVALID;

    // NULL any zero length vectors to prevent any accidental writes.
    for (u32 i = 0; i < in_count + out_count; i++) {
        if (vec[i].len == 0)
            vec[i].data = nullptr;
    }

    // Open a direct file
    if (GetHandleType(fd) == HandleType::DIRECT) {
        switch (cmd) {
        default:
            PRINT(
                IOS_EmuFS, ERROR, "Unknown direct ioctl: %u",
                static_cast<s32>(cmd)
            );
            return ISFS::ISFSError::INVALID;

        case ISFS::ISFSIoctl::DIRECT_OPEN: {
            if (in_count != 2 || out_count != 0) {
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
                    IOS_EmuFS, ERROR,
                    "Direct_Open: Invalid open mode length: %d", vec[1].len
                );
                return ISFS::ISFSError::INVALID;
            }

            if (!IsAligned(vec[1].data, 4)) {
                PRINT(
                    IOS_EmuFS, ERROR, "Direct_Open: Invalid open mode alignment"
                );
                return ISFS::ISFSError::INVALID;
            }

            // Check if the supplied file path length is valid.
            if (strnlen(
                    reinterpret_cast<const char*>(vec[0].data), vec[0].len
                ) == vec[0].len) {
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
            if (in_count != 1 || out_count != 0) {
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
            if (strnlen(
                    reinterpret_cast<const char*>(vec[0].data), vec[0].len
                ) == vec[0].len) {
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
            if (in_count != 0 || out_count != 1) {
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
            auto fret = f_readdir(&s_fileArray[realFd].dir, &fno);
            if (fret != FR_OK) {
                PRINT(
                    IOS_EmuFS, ERROR, "Direct_DirNext: f_readdir error: %d",
                    fret
                );
                return FResultToISFSError(fret);
            }

            if (fno.fname[0] == '\0') {
                PRINT(
                    IOS_EmuFS, INFO, "Direct_DirNext: Reached end of directory"
                );
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

    if (GetHandleType(fd) != HandleType::MANAGER) {
        return ISFS::ISFSError::INVALID;
    }

    auto manager = GetManagerResource(fd);

    switch (cmd) {
    // [ISFS_ReadDir]
    // vec[0]: path
    // todo
    case ISFS::ISFSIoctl::READ_DIR: {
        if (in_count != out_count || in_count < 1 || in_count > 2) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: Wrong vector count");
            return ISFS::ISFSError::INVALID;
        }

        if (!IsAligned(vec[0].data, 4) || vec[0].len < ISFS::MAX_PATH_LENGTH) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: Invalid input path vector");
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH];
        memcpy(path, vec[0].data, ISFS::MAX_PATH_LENGTH);
        PRINT(IOS_EmuFS, INFO, "ReadDir: ISFS_ReadDir(\"%s\")", path);

        u32 inMaxCount = 0;
        char* outNames = nullptr;
        u32* outCountPtr = nullptr;

        if (in_count == 2) {
            if (!IsAligned(vec[1].data, 4) || vec[1].len < sizeof(u32)) {
                PRINT(
                    IOS_EmuFS, ERROR,
                    "ReadDir: Invalid input max file count vector"
                );
                return ISFS::ISFSError::INVALID;
            }

            inMaxCount = *reinterpret_cast<u32*>(vec[1].data);

            if (!IsAligned(vec[2].data, 4) || vec[2].len < inMaxCount * 13) {
                PRINT(
                    IOS_EmuFS, ERROR,
                    "ReadDir: Invalid output file names vector"
                );
                return ISFS::ISFSError::INVALID;
            }

            outNames = reinterpret_cast<char*>(vec[2].data);
            memset(outNames, 0, inMaxCount * 13);

            if (!IsAligned(vec[3].data, 4) || vec[3].len < sizeof(u32)) {
                PRINT(
                    IOS_EmuFS, ERROR,
                    "ReadDir: Invalid output file count vector"
                );
                return ISFS::ISFSError::INVALID;
            }

            outCountPtr = reinterpret_cast<u32*>(vec[3].data);
        } else {
            if (!IsAligned(vec[1].data, 4) || vec[1].len < sizeof(u32)) {
                PRINT(
                    IOS_EmuFS, ERROR,
                    "ReadDir: Invalid output file count vector"
                );
                return ISFS::ISFSError::INVALID;
            }

            outCountPtr = reinterpret_cast<u32*>(vec[1].data);
        }

        // Get the replaced path
        if (!GetPathIfReplaced(path, s_efsPath, sizeof(s_efsPath))) {
            return manager->Ioctlv(
                ISFS::ISFSIoctl::READ_DIR, in_count, out_count, vec
            );
        }

        DIR dir;
        auto fret = f_opendir(&dir, s_efsPath);
        if (fret != FR_OK) {
            PRINT(
                IOS_EmuFS, ERROR,
                "ReadDir: Failed to open replaced directory: %d", fret
            );
            return FResultToISFSError(fret);
        }

        FILINFO info;
        u32 count = 0;
        while ((fret = f_readdir(&dir, &info)) == FR_OK) {
            const char* name = info.fname;
            auto len = strlen(name);

            if (len <= 0) {
                break;
            }

            if (len > 12) {
                if (strlen(info.altname) < 1 || !strcmp(info.altname, "?"))
                    continue;
                name = info.altname;
            }

            if (count < inMaxCount) {
                char nameData[13] = {0};
                strncpy(nameData, name, sizeof(nameData));
                System::UnalignedMemcpy(
                    outNames + count * 13, nameData, sizeof(nameData)
                );
            }

            ASSERT(count < INT_MAX);
            count++;
        }

        auto fret2 = f_closedir(&dir);
        if (fret2 != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: f_closedir error: %d", fret2);
            return ISFS::ISFSError::UNKNOWN;
        }

        if (fret != FR_OK) {
            PRINT(IOS_EmuFS, ERROR, "ReadDir: f_readdir error: %d", fret);
            return FResultToISFSError(fret);
        }

        PRINT(IOS_EmuFS, INFO, "ReadDir: count: %u", count);
        *outCountPtr = count;

        return ISFS::ISFSError::OK;
    }

    // [ISFS_GetUsage]
    // vec[0](in): Path
    // vec[1](out): Used clusters
    // vec[2](out): Used inodes
    case ISFS::ISFSIoctl::GET_USAGE: {
        if (in_count != 1 || out_count != 2) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Wrong vector count");
            return ISFS::ISFSError::INVALID;
        }

        if (!IsAligned(vec[0].data, 4) || vec[0].len < ISFS::MAX_PATH_LENGTH) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Invalid input path vector");
            return ISFS::ISFSError::INVALID;
        }

        if (!IsAligned(vec[1].data, 4) || vec[1].len < sizeof(u32)) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Invalid used clusters vector");
            return ISFS::ISFSError::INVALID;
        }

        if (!IsAligned(vec[2].data, 4) || vec[2].len < sizeof(u32)) {
            PRINT(IOS_EmuFS, ERROR, "GetUsage: Invalid used inodes vector");
            return ISFS::ISFSError::INVALID;
        }

        char path[ISFS::MAX_PATH_LENGTH];
        memcpy(path, vec[0].data, ISFS::MAX_PATH_LENGTH);
        PRINT(IOS_EmuFS, INFO, "GetUsage: ISFS_GetUsage(\"%s\")", path);

        // Get the replaced path
        if (!GetPathIfReplaced(path, s_efsPath, sizeof(s_efsPath))) {
            return manager->Ioctlv(
                ISFS::ISFSIoctl::GET_USAGE, in_count, out_count, vec
            );
        }

        // TODO?
        WriteU32(vec[1].data, 0);
        WriteU32(vec[2].data, 0);
        return ISFS::ISFSError::OK;
    }

    default:
        PRINT(IOS_EmuFS, ERROR, "Unknown manager ioctlv: %u", cmd);
        return ISFS::ISFSError::INVALID;
    }
}

static s32 ForwardRequest(IOS::Request* req)
{
    const s32 fd = req->fd - REAL_HANDLE_BASE;
    ASSERT(req->cmd == IOS::Cmd::OPEN || (fd >= 0 && fd < REAL_HANDLE_MAX));

    switch (req->cmd) {
    case IOS::Cmd::OPEN:
        // Should never reach here.
        ASSERT(!"Open in ForwardRequest");
        return IOS::IOSError::NOT_FOUND;

    case IOS::Cmd::CLOSE:
        return IOS_Close(fd);

    case IOS::Cmd::READ:
        return IOS_Read(fd, req->read.data, req->read.len);

    case IOS::Cmd::WRITE:
        return IOS_Write(fd, req->write.data, req->write.len);

    case IOS::Cmd::SEEK:
        return IOS_Seek(fd, req->seek.where, req->seek.whence);

    case IOS::Cmd::IOCTL:
        return IOS_Ioctl(
            fd, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len,
            req->ioctl.out, req->ioctl.out_len
        );

    case IOS::Cmd::IOCTLV:
        return IOS_Ioctlv(
            fd, req->ioctlv.cmd, req->ioctlv.in_count, req->ioctlv.out_count,
            req->ioctlv.vec
        );

    default:
        PRINT(
            IOS_EmuFS, ERROR, "Unknown command: %u", static_cast<u32>(req->cmd)
        );
        return ISFS::ISFSError::INVALID;
    }
}

static s32 OpenReplaced(IOS::Request* req)
{
    char path[64];
    strncpy(path, req->open.path, 64);
    path[0] = '/';

    PRINT(IOS_EmuFS, INFO, "IOS_Open(\"%s\", 0x%X)", path, req->open.mode);

    if (!strcmp(path, "/dev/fs")) {
        PRINT(IOS_EmuFS, INFO, "Open /dev/fs from PPC");

        // Find open handle.
        u32 i = 0;
        for (; i < MANAGER_HANDLE_MAX; i++) {
            if (s_managers[i].GetFd() < 0)
                break;
        }

        // There should always be an open handle.
        ASSERT(i != MANAGER_HANDLE_MAX);

        // Security note! Interrupts will be disabled at this point
        // (IOS_Open always does), and the IPC thread can't do anything else
        // while it's waiting for a response from us, so this should be safe
        // to do to the root process..?
        s32 pid = IOS_GetProcessId();
        ASSERT(pid >= 0);

        PRINT(
            IOS_EmuFS, INFO, "Set PID %d to UID %08X GID %04X", pid,
            req->open.uid, req->open.gid
        );

        s32 ret2 = IOS_SetUid(pid, req->open.uid);
        ASSERT(ret2 == IOS::IOSError::OK);
        ret2 = IOS_SetGid(pid, req->open.gid);
        ASSERT(ret2 == IOS::IOSError::OK);

        new (&s_managers[i]) IOS::ResourceCtrl<ISFS::ISFSIoctl>("/dev/fs");

        ret2 = IOS_SetUid(pid, 0);
        ASSERT(ret2 == IOS::IOSError::OK);
        ret2 = IOS_SetGid(pid, 0);
        ASSERT(ret2 == IOS::IOSError::OK);

        if (s_managers[i].GetFd() < 0) {
            PRINT(
                IOS_EmuFS, INFO, "/dev/fs open error: %d", s_managers[i].GetFd()
            );
            return s_managers[i].GetFd();
        }

        PRINT(IOS_EmuFS, INFO, "/dev/fs open success");
        return MANAGER_HANDLE_BASE + i;
    }

    if (!strncmp(path, "/dev", 4)) {
        // Fall through to the next resource.
        return IOS::IOSError::NOT_FOUND;
    }

    if (IsPathReplaced(path)) {
        return ReqProxyOpen(path, req->open.mode);
    }

    PRINT(IOS_EmuFS, INFO, "Forwarding open to real FS");
    return ForwardRequest(req);
}

static s32 Frontend_HandleRequest(IOS::Request* req)
{
    s32 ret = IOS::IOSError::INVALID;

    s32 fd = req->fd;
    if (req->cmd != IOS::Cmd::OPEN && GetHandleType(fd) == HandleType::REAL)
        return ForwardRequest(req);

    if (req->cmd != IOS::Cmd::OPEN && GetHandleType(fd) == HandleType::DIRECT &&
        (req->cmd == IOS::Cmd::READ || req->cmd == IOS::Cmd::WRITE ||
         req->cmd == IOS::Cmd::SEEK || req->cmd == IOS::Cmd::IOCTL)) {
        s32 realFd = s_directFileArray[GetHandleIndex(fd)].fd;

        // Switch to replaced file fd for future commands.
        if (!s_directFileArray[GetHandleIndex(fd)].inUse ||
            !Proxy_IsValidFile(realFd)) {
            PRINT(
                IOS_EmuFS, ERROR, "Attempting to use an unopened direct file"
            );
            return ISFS::ISFSError::INVALID;
        }

        fd = realFd;
    }

    switch (req->cmd) {
    case IOS::Cmd::OPEN: {
        if (req->open.path[0] == '$') {
            // Replaced ISFS path.
            ret = OpenReplaced(req);
            break;
        }

        if (strcmp(req->open.path, "/dev/saoirse/file") != 0) {
            ret = IOS::IOSError::NOT_FOUND;
            break;
        }

        // Direct file open.
        // Find available direct file index.
        int i = 0;
        for (; i < DIRECT_HANDLE_MAX; i++) {
            if (!s_directFileArray[i].inUse)
                break;
        }

        if (i == DIRECT_HANDLE_MAX) {
            ret = ISFS::ISFSError::MAX_HANDLES_OPEN;
            break;
        }

        s_directFileArray[i].inUse = true;
        s_directFileArray[i].fd = ISFS::ISFSError::NOT_FOUND;
        ret = DIRECT_HANDLE_BASE + i;
        break;
    }

    case IOS::Cmd::CLOSE:
        PRINT(IOS_EmuFS, INFO, "IOS_Close(%d)", fd);
        ret = ReqClose(fd);
        break;

    case IOS::Cmd::READ:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Read(%d, 0x%08X, 0x%X)", fd, req->read.data,
            req->read.len
        );
        ret = ReqRead(fd, req->read.data, req->read.len);
        break;

    case IOS::Cmd::WRITE:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Write(%d, 0x%08X, 0x%X)", fd, req->write.data,
            req->write.len
        );
        ret = ReqWrite(fd, req->write.data, req->write.len);
        break;

    case IOS::Cmd::SEEK:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Seek(%d, %d, %d)", fd, req->seek.where,
            req->seek.whence
        );
        ret = ReqSeek(fd, req->seek.where, req->seek.whence);
        break;

    case IOS::Cmd::IOCTL:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Ioctl(%d, %d, 0x%08X, 0x%X, 0x%08X, 0x%X)",
            fd, req->ioctl.cmd, req->ioctl.in, req->ioctl.in_len,
            req->ioctl.out, req->ioctl.out_len
        );
        ret = ReqIoctl(
            fd, static_cast<ISFS::ISFSIoctl>(req->ioctl.cmd), req->ioctl.in,
            req->ioctl.in_len, req->ioctl.out, req->ioctl.out_len
        );
        break;

    case IOS::Cmd::IOCTLV:
        PRINT(
            IOS_EmuFS, INFO, "IOS_Ioctlv(%d, %d, %d, %d, 0x%08X)", fd,
            req->ioctlv.cmd, req->ioctlv.in_count, req->ioctlv.out_count,
            req->ioctlv.vec
        );
        ret = ReqIoctlv(
            fd, static_cast<ISFS::ISFSIoctl>(req->ioctlv.cmd),
            req->ioctlv.in_count, req->ioctlv.out_count, req->ioctlv.vec
        );
        break;

    default:
        PRINT(
            IOS_EmuFS, ERROR, "Unknown command: %u", static_cast<u32>(req->cmd)
        );
        ret = ISFS::ISFSError::INVALID;
        break;
    }

    // Can't print on IOS_Open before game launches due to locked IPC thread
    if (req->cmd != IOS::Cmd::OPEN) {
        PRINT(IOS_EmuFS, INFO, "Reply: %d", ret);
    }

    return ret;
}

static Queue<IOS::Request*> s_ipcQueue;

static s32 ThreadEntry([[maybe_unused]] void* arg)
{
    PRINT(IOS_EmuFS, INFO, "Starting FS...");
    PRINT(IOS_EmuFS, INFO, "EmuFS thread ID: %d", IOS_GetThreadId());

    while (true) {
        IOS::Request* request = s_ipcQueue.Receive();
        request->Reply(Frontend_HandleRequest(request));
    }

    // Can never reach here
    return 0;
}

void DeviceEmuFS::Init()
{
    s32 ret = IOS_RegisterResourceManager("$", s_ipcQueue.GetID());
    if (ret != IOS::IOSError::OK) {
        PRINT(IOS_EmuFS, ERROR, "IOS_RegisterResourceManager failed: %d", ret);
        System::Abort();
    }

    ret = IOS_RegisterResourceManager("/dev/saoirse/file", s_ipcQueue.GetID());
    if (ret != IOS::IOSError::OK) {
        PRINT(IOS_EmuFS, ERROR, "IOS_RegisterResourceManager failed: %d", ret);
        System::Abort();
    }

    // Reset files
    for (int i = 0; i < HANDLE_PROXY_NUM; i++) {
        s_fileArray[HANDLE_PROXY_BASE + i].inUse = false;
        s_fileArray[HANDLE_PROXY_BASE + i].filOpened = false;
    }

    for (int i = 0; i < DIRECT_HANDLE_MAX; i++) {
        s_directFileArray[DIRECT_HANDLE_BASE + i].inUse = false;
        s_directFileArray[DIRECT_HANDLE_BASE + i].fd =
            ISFS::ISFSError::NOT_FOUND;
    }

    new Thread(ThreadEntry, nullptr, nullptr, 0x2000, 80);
}
