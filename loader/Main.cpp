// Main.cpp - Starling Entry Point
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "7zLzmaDec.h"
#include <AddressMap.h>
#include <Archive.hpp>
#include <Arguments.hpp>
#include <CPUCache.hpp>
#include <Console.hpp>
#include <DOL.hpp>
#include <ES.hpp>
#include <IOS.hpp>
#include <ImportInfo.hpp>
#include <Log.hpp>
#include <StarlingIOS.hpp>
#include <array>
#include <cstring>
#include <iterator>
#include <stdio.h>
#include <stdlib.h>

static inline void ClearWords(u32* data, u32 count)
{
    count /= 8;
    while (count--) {
        asm volatile("dcbz    0, %0\n"
                     //"sync\n"
                     "dcbf    0, %0\n" ::"r"(data));
        data += 8;
    }
}

static inline void CopyWords(u32* dest, u32* src, u32 count)
{
    u32 value = 0;
    count /= 8;
    while (count--) {
        asm volatile(
            "dcbz    0, %1\n"
            //"sync\n"
            "lwz     %0, 0(%2)\n"
            "stw     %0, 0(%1)\n"
            "lwz     %0, 4(%2)\n"
            "stw     %0, 4(%1)\n"
            "lwz     %0, 8(%2)\n"
            "stw     %0, 8(%1)\n"
            "lwz     %0, 12(%2)\n"
            "stw     %0, 12(%1)\n"
            "lwz     %0, 16(%2)\n"
            "stw     %0, 16(%1)\n"
            "lwz     %0, 20(%2)\n"
            "stw     %0, 20(%1)\n"
            "lwz     %0, 24(%2)\n"
            "stw     %0, 24(%1)\n"
            "lwz     %0, 28(%2)\n"
            "stw     %0, 28(%1)\n"
            "dcbf    0, %1\n" ::"r"(value),
            "r"(dest), "r"(src)
        );
        dest += 8;
        src += 8;
    }
}

void WaitMilliseconds(u32 milliseconds)
{
    u32 duration = milliseconds * 60750;
    u32 start;
    asm volatile("mftbl %0" : "=r"(start));
    for (u32 current = start; current - start < duration;) {
        asm volatile("mftbl %0" : "=r"(current));
    }
}

ES::TMDFixed<32> s_shopTmd alignas(32);
u64 s_shopId = 0x0001000848414c45;

const char s_contentPath[] = "/title/00000000/00000000/content/00000000.app";

struct ContentPath {
    char str[sizeof(s_contentPath)];
};

template <class T>
void ToHexString(T v, char* out)
{
    for (u32 i = 1; i < sizeof(T) * 2 + 1; i++) {
        u8 j = 0xF & (v >> (sizeof(T) * 8 - i * 4));
        if (j < 0xA)
            out[i - 1] = j + '0';
        else
            out[i - 1] = j - 0xA + 'a';
    }
}

void GetContentPath(u64 titleID, u32 cid, ContentPath* out)
{
    memcpy(out->str, s_contentPath, sizeof(s_contentPath));

    ToHexString<u32>(titleID >> 32, out->str + sizeof("/title/") - 1);
    ToHexString<u32>(titleID, out->str + sizeof("/title/00000000/") - 1);
    ToHexString<u32>(
        cid, out->str + sizeof("/title/00000000/00000000/content/") - 1
    );
}

static const char* s_tmdPaths[] = {
    "/title/00010008/48414c45/content/title.tmd", // HALE
    "/title/00010008/48414c50/content/title.tmd", // HALP
    "/title/00010008/48414c4a/content/title.tmd", // HALJ
    "/title/00010008/48414c4b/content/title.tmd", // HALK
    "/title/00010008/48414c43/content/title.tmd", // HALC
};

bool ReadWiiShopTMD()
{
    IOS::File file_tmd;

    for (u32 i = 0; i < std::size(s_tmdPaths); i++) {
        new (&file_tmd) IOS::File(s_tmdPaths[i], IOS::Mode::READ);
        if (file_tmd.IsValid()) {
            break;
        }
    }

    if (!file_tmd.IsValid()) {
        Console::Print("\nERROR: Failed to open rgnsel TMD\n");
        return false;
    }

    u32 size = file_tmd.GetSize();

    if (size > sizeof(ES::TMDFixed<32>)) {
        Console::Print("\nERROR : rgnsel TMD is too large.\n");
        return false;
    }

    if (file_tmd.Read(&s_shopTmd, size) != s32(size)) {
        Console::Print("\nERROR : Failed to read rgnsel TMD.\n");
        return false;
    }

    if (s_shopTmd.numContents < 1) {
        Console::Print("\nERROR : Invalid rgnsel TMD contents.\n");
        return false;
    }

    s_shopId = s_shopTmd.titleId;

    return true;
}

DOL s_shopDol alignas(32);
s32 s_rgnselVer = -1;

static std::array<u8, 20> s_rgnselVersions[] = {
    // 0: Pv2 Ev2 Jv2
    {0xEE, 0x8E, 0x78, 0xAA, 0x48, 0xAC, 0xDE, 0x8B, 0x9D, 0x10,
     0xA1, 0xA5, 0xBB, 0xCA, 0x81, 0x14, 0xD3, 0x32, 0x47, 0x2B},
    // 1: Kv2
    {0x3B, 0xEE, 0x47, 0x4D, 0x62, 0xB1, 0x96, 0xF2, 0x93, 0xCD,
     0xFF, 0xCC, 0xA4, 0x91, 0x64, 0xEA, 0x2E, 0xA3, 0x55, 0x44},
    // 2: Cv2
    {0x09, 0xF0, 0x70, 0xA7, 0x4B, 0x64, 0xB2, 0xD5, 0xF1, 0x20,
     0x25, 0xA6, 0x10, 0xD3, 0x96, 0x36, 0xEB, 0x1C, 0xF3, 0xED},
};

bool LoadWiiShopDOL()
{
    // Read index 1 as that should be the DOL as loaded by the NAND loader.
    if (s_shopTmd.numContents < 2) {
        Console::Print("\nERROR : rgnsel does not have a DOL.\n");
        return false;
    }

    ContentPath path alignas(32) = {};
    GetContentPath(s_shopId, s_shopTmd.contents[1].cid, &path);

    for (u32 i = 0; i < std::size(s_rgnselVersions); i++) {
        if (!std::memcmp(
                s_rgnselVersions[i].data(), s_shopTmd.contents[1].hash,
                s_rgnselVersions[i].size()
            )) {
            s_rgnselVer = i;
            break;
        }
    }

    if (s_rgnselVer == -1) {
        Console::Print("\nERROR : Unsupported system version. Please update\n"
                       "        your Wii using the Wii System Update.\n");
        return false;
    }

    if (s_rgnselVer == 1) {
        Console::Print("\nERROR : Korean Wiis are currently not supported.\n");
        return false;
    }

    if (s_rgnselVer == 2) {
        Console::Print("\nERROR : Chinese Wiis are currently not supported.\n");
        return false;
    }

    IOS::File file_dol(path.str, IOS::Mode::READ);
    if (!file_dol.IsValid()) {
        Console::Print("\nERROR: Failed to open rgnsel DOL.\n");
        return false;
    }

    if (file_dol.Read(&s_shopDol, sizeof(s_shopDol)) != sizeof(s_shopDol)) {
        Console::Print("\nERROR : Failed to read rgnsel DOL header.\n");
        return false;
    }

    ClearWords(
        reinterpret_cast<u32*>(s_shopDol.bssAddr),
        s_shopDol.bssSize / sizeof(u32)
    );

    for (u32 i = 0; i < 7 + 11; i++) {
        if (s_shopDol.sectionSize[i] == 0)
            continue;

        if (file_dol.Seek(s_shopDol.section[i], SEEK_SET) !=
            s32(s_shopDol.section[i])) {
            Console::Print("\nERROR : Failed to seek rgnsel DOL.\n");
            return false;
        }

        if (file_dol.Read(
                reinterpret_cast<void*>(s_shopDol.sectionAddr[i]),
                s_shopDol.sectionSize[i]
            ) != s32(s_shopDol.sectionSize[i])) {
            Console::Print("\nERROR : Failed to read rgnsel DOL.\n");
            return false;
        }

        CPUCache::ICInvalidate(
            reinterpret_cast<void*>(s_shopDol.sectionAddr[i]),
            s_shopDol.sectionSize[i]
        );
    }

    return true;
}

extern u8 LoaderArchive[];
extern u32 LoaderArchiveSize;

static bool s_bootArcDecomp = false;
static u8* s_bootArcData = reinterpret_cast<u8*>(BOOT_ARC_ADDRESS);
static u32 s_bootArcSize = BOOT_ARC_MAXLEN;

Archive GetLoaderArchive()
{
    if (!s_bootArcDecomp) {
        ELzmaStatus status;
        u32 inLen = LoaderArchiveSize - 5;
        auto ret = LzmaDecode(
            s_bootArcData, &s_bootArcSize, LoaderArchive + 0xD, &inLen,
            LoaderArchive, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, 0
        );

        assert(ret == SZ_OK);

        s_bootArcDecomp = true;
    }

    return Archive(s_bootArcData, s_bootArcSize);
}

struct HBCArgv {
    static constexpr u32 MAGIC = 0x5F617267;

    u32 magic; // 0x5F617267
    char* cmdLine;
    u32 length;
    u32 argc;
    char** argv;
    char** argvEnd;
};

extern HBCArgv HBCArgvData;

extern TwmImportEntry TwmTable;
extern TwmImportEntry TwmTableEnd;

ImportInfo s_initInfo = {};

extern u32 aicr;

void Launch()
{
    // Reset the DSP; libogc apps like the HBC cannot initialize it properly,
    // but the SDK can.
    aicr = 0;

    Console::Init();
    Console::Print("\n\nStarling Launcher\n\n\n");

    Archive archive = GetLoaderArchive();

    u32 argc = 0;
    const char* argv[128] = {};

    // Process command line arguments
    if (HBCArgvData.magic == HBCArgv::MAGIC && HBCArgvData.cmdLine != nullptr &&
        HBCArgvData.length > 0) {
        char lastChar = '\0';
        for (u32 i = 0; i < HBCArgvData.length && argc < std::size(argv); i++) {
            char thisChar = HBCArgvData.cmdLine[i];

            if (lastChar == '\0' && thisChar != '\0') {
                argv[argc++] = HBCArgvData.cmdLine + i;
            }

            lastChar = thisChar;
        }
    }

    Console::Print("I[Loader] Starting IOS loader... ");
    auto entry = archive.get("./ios_loader.bin");
    Archive::File* file = std::get_if<Archive::File>(&entry);
    if (!file) {
        Console::Print("\nERROR : Failed to get the IOS boot payload.\n");
        return;
    }

    memcpy(
        reinterpret_cast<void*>(IOS_BOOT_ADDRESS), s_bootArcData + file->offset,
        file->size
    );
    StarlingIOS::SafeFlush(
        reinterpret_cast<void*>(IOS_BOOT_ADDRESS), file->size
    );

    entry = archive.get("./ios_module.elf");
    file = std::get_if<Archive::File>(&entry);
    if (!file) {
        Console::Print("\nERROR : Failed to get the IOS module.\n");
        return;
    }

    WriteU32(IOS_FILE_INFO_ADDRESS, s_bootArcData + file->offset);
    WriteU32(IOS_FILE_INFO_ADDRESS + 4, file->size);
    StarlingIOS::SafeFlush(
        reinterpret_cast<void*>(IOS_FILE_INFO_ADDRESS), IOS_FILE_INFO_MAXLEN
    );
    StarlingIOS::SafeFlush(s_bootArcData + file->offset, file->size);

    if (!StarlingIOS::BootstrapEntry()) {
        Console::Print("\nERROR : Failed to launch the IOS boot payload.\n");
        return;
    }
    Console::Print("OK\n");

    argc = 9;
    argv[1] = "--patch-id";
    argv[2] = "nsmbw-pipe-randomizer";
    argv[3] = "--patch-id=";
    argv[4] = "--riivo-xml=xmlpath";
    argv[5] = "--riivo-xml";
    argv[6] = "the_other_path";
    argv[7] = "--riivo-xml=";
    argv[8] = "--riivo-xml";

    PRINT(System, INFO, "Start the command line\n");

    auto arguments = Arguments::ParseCommandLine(argc, argv);

    PRINT(System, INFO, "Done with command line\n");

    if (arguments.IsStartReady()) {
        PRINT(System, INFO, "Start the game");
        arguments.Launch();
    }

    // Not enough arguments to start a game directly
    Console::Print("I[Loader] > Booting into Channel\n");

    Console::Print("I[Loader] Reading Rgnsel TMD... ");
    if (!ReadWiiShopTMD()) {
        return;
    }
    Console::Print("OK\n");

    Console::Print("I[Loader] Mounting Rgnsel... ");
    if (!LoadWiiShopDOL()) {
        return;
    }
    Console::Print("OK\n");

    s32 importCount = std::distance(&TwmTable, &TwmTableEnd);
    TwmImportEntry* importTable = &TwmTable;

    for (s32 i = 0; i < importCount; i++) {
        u32 address = importTable[i].address;

        switch (importTable[i].type) {
        case TWM_FUNCTION_IMPORT:
            // Patch the stub function to a branch
            WriteU32(
                importTable[i].stub | 0xC0000000,
                0x48000000 | ((address - u32(importTable[i].stub)) & 0x03FFFFFC)
            );
            break;

        case TWM_FUNCTION_REPLACE:
            // Patch the replaced function to a branch
            WriteU32(
                address | 0xC0000000,
                0x48000000 | ((u32(importTable[i].stub) - address) & 0x03FFFFFC)
            );
            break;

        case TWM_DATA_IMPORT:
            // Write the offset to the stub address
            WriteU32(importTable[i].stub, address);
            break;
        }
    }

    std::memcpy(&s_initInfo.s_tmd, &s_shopTmd, sizeof(s_initInfo.s_tmd));
    s_initInfo.s_importDolId = s_rgnselVer;

    u32* mem1 = reinterpret_cast<u32*>(0x80000000);
    std::memset(mem1, 0, 0x100);
    mem1[0x20 / 4] = 0x0D15EA5E; // "disease"
    mem1[0x24 / 4] = 0x00000001;
    mem1[0x28 / 4] = 0x01800000;
    mem1[0x2C / 4] = 1 + (ReadU32(0xCC00302C) >> 28);
    mem1[0x34 / 4] = 0x817FEC60;
    mem1[0xF0 / 4] = 0x01800000;
    mem1[0xF8 / 4] = 0x0E7BE2C0;
    mem1[0xFC / 4] = 0x2B73A840;
    CPUCache::DCFlush(mem1, 0x100);

    u32 time0 = ReadU32(0x800030D8);
    u32 time1 = ReadU32(0x800030DC);

    std::memset(mem1 + 0x3000 / 4, 0, 0x400);
    mem1[0x30D8 / 4] = time0;
    mem1[0x30DC / 4] = time1;
    mem1[0x30E4 / 4] = 0x00008201;
    mem1[0x3100 / 4] = 0x01800000;
    mem1[0x3104 / 4] = 0x01800000;
    mem1[0x3108 / 4] = 0x81800000;
    mem1[0x310C / 4] = 0x80400000;
    mem1[0x3110 / 4] = 0x81600000;
    mem1[0x3114 / 4] = 0xDEADBEEF;
    mem1[0x3118 / 4] = 0x04000000;
    mem1[0x311C / 4] = 0x04000000;
    mem1[0x3120 / 4] = 0x93600000;
    mem1[0x3124 / 4] = CHANNEL_HEAP_ADDRESS;
    mem1[0x3128 / 4] = 0x935E0000;
    mem1[0x312C / 4] = 0xDEADBEEF;
    mem1[0x3130 / 4] = 0x935E0000;
    mem1[0x3134 / 4] = 0x93600000;
    mem1[0x3138 / 4] = 0x00000011;
    mem1[0x313C / 4] = 0xDEADBEEF;
    mem1[0x3140 / 4] = 0xFFFF | ((s_shopTmd.iosTitleId & 0xFFFF) << 16);
    mem1[0x3144 / 4] = 0x00030310;
    mem1[0x3148 / 4] = 0x93600000;
    mem1[0x314C / 4] = 0x93620000;
    mem1[0x3150 / 4] = 0xDEADBEEF;
    mem1[0x3154 / 4] = 0xDEADBEEF;
    mem1[0x3158 / 4] = 0x0000FF01;
    mem1[0x315C / 4] = 0x80AD0113;
    mem1[0x3188 / 4] = 0xFFFF | ((s_shopTmd.iosTitleId & 0xFFFF) << 16);
    CPUCache::DCFlush(mem1 + 0x3000 / 4, 0x400);

    ((void (*)(void)) s_shopDol.entryPoint)();
}

extern "C" void load()
{
    extern u32 _bss_start;
    extern u32 _bss_end;

    ClearWords(&_bss_start, &_bss_end - &_bss_start);

    Launch();
    while (true) {
    }
}
