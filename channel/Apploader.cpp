#include "Apploader.hpp"
#include <AddressMap.h>
#include <CPUCache.hpp>
#include <DI.hpp>
#include <DOL.hpp>
#include <Import_RVL_OS.h>
#include <LoMem.hpp>
#include <Log.hpp>
#include <cstring>
#include <magic_enum.hpp>
#include <optional>

struct PartitionGroup {
    u32 count;
    u32 shiftedOffset;
};

static_assert(sizeof(PartitionGroup) == 0x8);

namespace PartitionType
{
enum {
    Data = 0,
};
} // namespace PartitionType

struct PartitionInfo {
    u32 shiftedOffset;
    u32 type;
};

static_assert(sizeof(PartitionInfo) == 0x8);

struct PartitionOffsets {
    u32 dolOffset;
    u32 fstOffset;
    u32 fstSize;
    u32 fstMaxSize;
    FILL(0x10, 0x20); // Alignment
};

extern "C" void RunDOL(DOL* dol);

static void ShutdownOS()
{
    __OSUnRegisterStateEvent();

    OSDisableScheduler();
    __OSShutdownDevices(6);
    OSEnableScheduler();

    OSDisableInterrupts();

    for (s32 i = 0; i < 32; i++) {
        IOS_Close(i);
    }
}

void Apploader::Load()
{
    DI di("/dev/di");

    DI::DiskID* diskID = reinterpret_cast<DI::DiskID*>(0x80000000);

    auto retDi = di.ReadDiskID(diskID);
    if (retDi == DI::DIError::Drive) {
        // The drive probably hasn't been spun up yet
        retDi = di.Reset(true);
        if (retDi == DI::DIError::OK) {
            retDi = di.ReadDiskID(diskID);
        }
    }

    if (retDi != DI::DIError::OK) {
        PRINT(
            BS2, ERROR, "Failed to init DI: 0x%X (%s)", retDi,
            magic_enum::enum_name(retDi).data()
        );
        return;
    }

    // Find the game partition
    PartitionGroup groups[4] alignas(0x20);
    retDi = di.UnencryptedRead(groups, sizeof(groups), 0x40000 >> 2);
    if (retDi != DI::DIError::OK) {
        PRINT(
            BS2, ERROR, "Failed to read groups: 0x%X (%s)", retDi,
            magic_enum::enum_name(retDi).data()
        );
        return;
    }

    u32 partOffset = 0;

    for (u32 i = 0; i < 4 && partOffset == 0; i++) {
        u32 partitionCount = groups[i].count;
        u32 offset = groups[i].shiftedOffset;

        if (partitionCount == 0 || partitionCount > 4 || offset == 0) {
            continue;
        }

        PartitionInfo partitions[4] alignas(0x20);
        retDi = di.UnencryptedRead(partitions, sizeof(partitions), offset);
        if (retDi != DI::DIError::OK) {
            PRINT(
                BS2, ERROR, "Failed to read partition info: 0x%X (%s)", retDi,
                magic_enum::enum_name(retDi).data()
            );
            return;
        }

        for (u32 j = 0; j < partitionCount; j++) {
            if (partitions[j].type == PartitionType::Data) {
                partOffset = partitions[j].shiftedOffset;
                break;
            }
        }
    }

    if (partOffset == 0) {
        PRINT(BS2, ERROR, "Failed to find game partition");
        return;
    }

    ES::TMDFixed<512> tmd alignas(32) = {};

    retDi = di.OpenPartition(partOffset, &tmd);
    if (retDi != DI::DIError::OK) {
        PRINT(
            BS2, ERROR, "Failed to open partition at offset %08X: 0x%X (%s)",
            partOffset, retDi, magic_enum::enum_name(retDi).data()
        );
        return;
    }

    PRINT(
        BS2, INFO, "Successfully opened partition at offset %08X", partOffset
    );

    PartitionOffsets hdrOffsets alignas(32) = {};
    retDi = di.Read(&hdrOffsets, sizeof(hdrOffsets), 0x420 >> 2);
    if (retDi != DI::DIError::OK) {
        PRINT(
            BS2, ERROR, "Failed to read from %08X: 0x%X (%s)", partOffset,
            retDi, magic_enum::enum_name(retDi).data()
        );
        return;
    }

    // Read the DOL
    DOL* dol = reinterpret_cast<DOL*>(LOAD_DOL_ADDRESS);

    retDi = di.Read(dol, sizeof(DOL), hdrOffsets.dolOffset);
    if (retDi != DI::DIError::OK) {
        PRINT(
            BS2, ERROR, "Failed to read DOL header: 0x%X (%s)", retDi,
            magic_enum::enum_name(retDi).data()
        );
        return;
    }

    for (u32 i = 0; i < DOL::SECTION_COUNT; i++) {
        if (dol->sectionSize[i] == 0) {
            continue;
        }

        PRINT(
            BS2, INFO, "DOL section (%02u): %08X, %08X, %08X", i,
            dol->section[i], dol->sectionAddr[i], dol->sectionSize[i]
        );

        dol->sectionSize[i] = AlignUp(dol->sectionSize[i], 32);

        if (!IsAligned(dol->section[i], 32) ||
            !IsAligned(dol->sectionAddr[i], 32) ||
            !IsAligned(dol->sectionSize[i], 4)) {
            PRINT(BS2, ERROR, "DOL section (%02u) has bad alignment", i);
            return;
        }

        if (!CheckBounds(
                sizeof(DOL), LOAD_DOL_MAXLEN - sizeof(DOL), dol->section[i],
                dol->sectionSize[i]
            ) ||
            !CheckBounds(
                0x80001800, 0x80900000 - 0x80001800, dol->sectionAddr[i],
                dol->sectionSize[i]
            )) {
            PRINT(BS2, ERROR, "DOL section (%02u) out of bounds", i);
            return;
        }

        retDi = di.Read(
            reinterpret_cast<void*>(LOAD_DOL_ADDRESS + dol->section[i]),
            dol->sectionSize[i], hdrOffsets.dolOffset + (dol->section[i] >> 2)
        );
        if (retDi != DI::DIError::OK) {
            PRINT(
                BS2, ERROR, "Failed to read DOL section (%u): 0x%X (%s)", i,
                retDi, magic_enum::enum_name(retDi).data()
            );
            return;
        }
    }

    // Read the FST
    u32 fstSize = hdrOffsets.fstSize << 2;
    u32 fstDest = AlignDown(0x81800000 - fstSize, 32);
    if (fstDest < 0x81700000) {
        PRINT(BS2, ERROR, "FST size is too large");
        return;
    }

    gLoMem.systemInfo.fstStart = reinterpret_cast<void*>(fstDest);
    retDi = di.Read(
        gLoMem.systemInfo.fstStart, AlignUp(fstSize, 32), hdrOffsets.fstOffset
    );
    if (retDi != DI::DIError::OK) {
        PRINT(
            BS2, ERROR, "Failed to read FST: 0x%X (%s)", retDi,
            magic_enum::enum_name(retDi).data()
        );
        return;
    }

    // Read BI2
    gLoMem.threadInfo.bi2 =
        reinterpret_cast<LoMem::ThreadInfo::BI2*>(fstDest - 0x2000);
    retDi = di.Read(gLoMem.threadInfo.bi2, 0x2000, 0x440);
    if (retDi != DI::DIError::OK) {
        PRINT(
            BS2, ERROR, "Failed to read BI2: 0x%X (%s)", retDi,
            magic_enum::enum_name(retDi).data()
        );
        return;
    }

    if (gLoMem.threadInfo.bi2->dualLayerValue == 0x7ED40000) {
        gLoMem.osGlobals.dualLayerValue =
            LoMem::OSGlobals::DualLayerValue::DualLayer;
    } else {
        gLoMem.osGlobals.dualLayerValue =
            LoMem::OSGlobals::DualLayerValue::SingleLayer;
    }

    gLoMem.osGlobals.mem1ArenaEnd = fstDest;
    gLoMem.osGlobals.mem2UsableStart = 0x90000800;

    ShutdownOS();

    gLoMem.threadInfo.debugMonitorAddress = 0x81800000;
    gLoMem.threadInfo.simulatedMemorySize = 0x01800000;
    gLoMem.threadInfo.busSpeed = 0x0E7BE2C0;
    gLoMem.threadInfo.cpuSpeed = 0x2B73A840;

    gLoMem.osGlobals.iosVersion = gLoMem.osGlobals.iosMinimumVersion;
    std::memcpy(gLoMem.osGlobals.gameCode, gLoMem.diskId.gameCode, 4);

    gLoMem.systemInfo.arenaHigh = 0;

    RunDOL(dol);

    while (true) {
    }
}
