#pragma once

#include <IPC.h>
#include <Import.h>
#include <Import_GX.h>
#include <Util.h>

#define IMPORT_RVL_OS 1

EXTERN_C_START

typedef s64 OSTime;

#define OS_BUS_CLOCK (*(u32*) 0x800000F8)
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)

#define OSSecondsToTicks(sec) ((sec) *OS_TIMER_CLOCK)
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))
#define OSMicrosecondsToTicks(usec) ((usec) * (OS_TIMER_CLOCK / 1000000))
#define OSNanosecondsToTicks(nsec) ((nsec) / (1000000000 / OS_TIMER_CLOCK))
#define OSTicksToSeconds(ticks) ((ticks) / OS_TIMER_CLOCK)
#define OSTicksToNanoseconds(ticks) ((ticks) * (1000000000 / OS_TIMER_CLOCK))
#define OSTicksToMilliseconds(ticks) ((ticks) / (OS_TIMER_CLOCK / 1000))

// Replaced
// IMPORT(0x8006C280)
void OSReport(const char* format, ...);

IMPORT(0x8006FE70)
void __OSShutdownDevices(int param_1);

IMPORT(0x80070100)
void OSReturnToMenu();

IMPORT(0x8006B098)
u32 OSGetMEM1ArenaHi();

IMPORT(0x8006B0A0)
u32 OSGetMEM2ArenaHi();

IMPORT(0x8006B0A8)
u32 OSGetArenaHi();

IMPORT(0x8006B0B0)
u32 OSGetMEM1ArenaLo();

IMPORT(0x8006B0B8)
u32 OSGetMEM2ArenaLo();

IMPORT(0x8006B0C0)
u32 OSGetArenaLo();

IMPORT(0x8006E7B0)
u32 OSDisableInterrupts();

IMPORT(0x8006E7D8)
void OSRestoreInterrupts(u32 level);

IMPORT(0x8006EB40)
u32 __OSMaskInterrupts(u32 mask);

IMPORT(0x8006EBC0)
u32 __OSUnmaskInterrupts(u32 mask);

typedef struct {
    FILL(0x0, 0x18);
} OSMutex;

static_assert(sizeof(OSMutex) == 0x18);

IMPORT(0x8006FA34)
void OSInitMutex(OSMutex* mutex);

IMPORT(0x8006FA6C)
void OSLockMutex(OSMutex* mutex);

IMPORT(0x8006FB48)
void OSUnlockMutex(OSMutex* mutex);

IMPORT(0x8006FC7C)
bool OSTryLockMutex(OSMutex* mutex);

#define OS_THREAD_STACK_TOP_MAGIC 0xDEADBABE

#define OS_THREAD_PRIORITY_LOWEST 0
#define OS_THREAD_PRIORITY_HIGHEST 31

typedef struct {
    FILL(0x0, 0x8);
} OSThreadQueue;

typedef struct {
    FILL(0x0, 0x2D8);
    void* val;
    FILL(0x2DC, 0x308);
    u32* stackTop;
    FILL(0x30C, 0x318);
} OSThread;

static_assert(sizeof(OSThread) == 0x318);

void OSInitThreadQueue(OSThreadQueue* queue);

IMPORT(0x8006BF54)
void OSDumpContext(OSThread* context);

IMPORT(0x80070F88)
OSThread* OSGetCurrentThread();

IMPORT(0x80070F94)
bool OSIsThreadTerminated(OSThread* thread);

IMPORT(0x80070FC0)
s32 OSDisableScheduler();

IMPORT(0x80070FFC)
s32 OSEnableScheduler();

IMPORT(0x8007155C)
bool OSCreateThread(
    OSThread* thread, void* (*proc)(void*), void* param, void* stack,
    u32 stackSize, s32 priority, u16 attr
);

IMPORT(0x800717C8)
void OSExitThread(void* exitCode);

IMPORT(0x800718AC)
void OSCancelThread(OSThread* thread);

IMPORT(0x80071A84)
bool OSJoinThread(OSThread* thread, void** val);

IMPORT(0x80071BC4)
s32 OSResumeThread(OSThread* thread);

IMPORT(0x80071FF0)
void OSSleepThread(OSThreadQueue* queue);

IMPORT(0x800720DC)
void OSWakeupThread(OSThreadQueue* queue);

IMPORT(0x800721F4)
void OSSleepTicks(OSTime ticks);

#define OSSleepMicroseconds(usec) OSSleepTicks(OSMicrosecondsToTicks(usec))

IMPORT(0x800722A8)
OSTime OSGetTime();

IMPORT(0x80072E7C)
s32 __OSUnRegisterStateEvent();

/*
 * ARC Library
 */

typedef struct {
    FILL(0x0, 0x8);
    int fstSize;
    int fileStart;
    FILL(0x10, 0x20);
} ARCHeader;

static_assert(sizeof(ARCHeader) == 0x20);

typedef struct {
    FILL(0x0, 0x4);
    void* FSTStart;
    FILL(0x8, 0xC);
    u32 entryNum;
    FILL(0x10, 0x1C);
} ARCHandle;

static_assert(sizeof(ARCHandle) == 0x1C);

typedef struct {
    ARCHandle* handle;
    u32 startOffset;
    u32 length;
} ARCFileInfo;

static_assert(sizeof(ARCFileInfo) == 0xC);

typedef struct {
    ARCHandle* handle;
    u32 entryNum;
    u32 location;
    u32 next;
} ARCDir;

static_assert(sizeof(ARCDir) == 0x10);

typedef struct {
    FILL(0x0, 0x4);
    u32 entryNum;
    bool isDir;
    char* name;
} ARCDirEntry;

static_assert(sizeof(ARCDirEntry) == 0x10);

/*
 * IPC Library
 */

IMPORT(0x80098968)
s32 RVL_IOS_Open(const char* path, u32 mode);

IMPORT(0x80098B48)
s32 RVL_IOS_Close(s32 fd);

IMPORT(0x80098CF0)
s32 RVL_IOS_Read(s32 fd, void* data, u32 len);

IMPORT(0x80098EF8)
s32 RVL_IOS_Write(s32 fd, const void* data, u32 len);

IMPORT(0x800990E0)
s32 RVL_IOS_Seek(s32 fd, u32 where, u32 whence);

IMPORT(0x80099300)
s32 RVL_IOS_Ioctl(
    s32 fd, u32 ioctl, const void* in, u32 inLen, void* out, u32 outLen
);

IMPORT(0x8009956C)
s32 RVL_IOS_Ioctlv(s32 fd, u32 ioctl, u32 inCount, u32 outCount, IOVector* vec);

/*
 * ISFS library
 */

IMPORT(0x8009ACD0)
s32 ISFS_Delete(const char* path);

/*
 * MEM library
 */

struct MEMAllocatorFunc;

typedef struct {
    struct MEMAllocatorFunc* functions;
    FILL(0x4, 0x10);
} MEMAllocator;

typedef void* (*MEMFuncAllocatorAlloc)(MEMAllocator* allocator, u32 size);
typedef void (*MEMFuncAllocatorFree)(MEMAllocator* allocator, void* block);

typedef struct MEMAllocatorFunc {
    MEMFuncAllocatorAlloc alloc;
    MEMFuncAllocatorFree free;
} MEMAllocatorFunc;

typedef struct {
    void* prevObject;
    void* nextObject;
} MEMLink;

typedef struct {
    void* headObject;
    void* tailObject;
    u16 numObjects;
    u16 offset;
} MEMList;

typedef struct {
    u32 signature;
    MEMLink link;
    MEMList childList;
    FILL(0x18, 0x3C);
} MEMiHeapHead;

static_assert(sizeof(MEMiHeapHead) == 0x3C);

typedef MEMiHeapHead* MEMHeapHandle;

IMPORT(0x80090C84)
MEMHeapHandle MEMCreateExpHeapEx(void* begin, u32 size, u16 flags);

IMPORT(0x80090D64)
void* MEMAllocFromExpHeapEx(MEMHeapHandle handle, u32 size, u32 align);

IMPORT(0x80090E14)
void MEMFreeToExpHeap(MEMHeapHandle handle, void* block);

/*
 * AX library
 */

IMPORT(0x8008B770)
void AXInit();

/*
 * VI library
 */

IMPORT(0x800783BC)
void VIInit();

IMPORT(0x80078904)
void VIWaitForRetrace();

IMPORT(0x80078DA0)
void VIConfigure(GXRenderModeObj* rmode);

IMPORT(0x80079888)
void VIFlush();

IMPORT(0x8007999C)
void VISetNextFrameBuffer(void* fb);

IMPORT(0x80079A08)
void VISetBlack(bool value);

IMPORT(0x80079B18)
u32 VIGetTvFormat();

IMPORT(0x80079B78)
u32 VIGetScanMode();

IMPORT(0x80079BD8)
u32 VIGetDTVStatus();

/*
 * SC library
 */
IMPORT(0x80096118)
bool SCFindU8Item(void* out, u32 id);

IMPORT(0x80096964)
u8 SCGetAspectRatio();

// Missing functions we must reimplement

static inline bool SCGetEuRgb60Mode()
{
    u8 value;

    if (SCFindU8Item(&value, 6) && value == 1) {
        return 1;
    }

    return 0;
}

static inline bool SCGetProgressiveMode()
{
    u8 value;

    if (SCFindU8Item(&value, 14) && value == 1) {
        return 1;
    }

    return 0;
}

/*
 * MTX library
 */

IMPORT(0x8007C030)
void MTXIdentity(float mtx[3][4]);

IMPORT(0x8007C3F4)
void MTXMultVec(float mtx[3][4], float vec1[3], float vec2[3]);

IMPORT(0x8007C448)
void MTXOrtho(
    float mtx[4][4], float top, float bottom, float left, float right,
    float near, float far
);

/*
 * WPAD library
 */

/**
 * Wii Remote buttons.
 */
enum {
    // D-pad left
    WPAD_BUTTON_LEFT = 0x0001,
    // D-pad right
    WPAD_BUTTON_RIGHT = 0x0002,
    // D-pad down
    WPAD_BUTTON_DOWN = 0x0004,
    // D-pad up
    WPAD_BUTTON_UP = 0x0008,
    // + button
    WPAD_BUTTON_PLUS = 0x0010,
    // 2 button
    WPAD_BUTTON_2 = 0x0100,
    // 1 button
    WPAD_BUTTON_1 = 0x0200,
    // B button
    WPAD_BUTTON_B = 0x0400,
    // A button
    WPAD_BUTTON_A = 0x0800,
    // - button
    WPAD_BUTTON_MINUS = 0x1000,
    // Nunchuk Z button
    WPAD_BUTTON_Z = 0x2000,
    // Nunchuk C button
    WPAD_BUTTON_C = 0x4000,
    // HOME button
    WPAD_BUTTON_HOME = 0x8000,
};

/**
 * Classic Controller buttons.
 */
enum {
    // D-pad up
    WPAD_CLASSIC_BUTTON_UP = 0x0001,
    // D-pad left
    WPAD_CLASSIC_BUTTON_LEFT = 0x0002,
    // ZR trigger
    WPAD_CLASSIC_BUTTON_ZR = 0x0004,
    // X button
    WPAD_CLASSIC_BUTTON_X = 0x0008,
    // A button
    WPAD_CLASSIC_BUTTON_A = 0x0010,
    // Y button
    WPAD_CLASSIC_BUTTON_Y = 0x0020,
    // B button
    WPAD_CLASSIC_BUTTON_B = 0x0040,
    // ZL trigger
    WPAD_CLASSIC_BUTTON_ZL = 0x0080,
    // R trigger
    WPAD_CLASSIC_BUTTON_R = 0x0200,
    // + button
    WPAD_CLASSIC_BUTTON_PLUS = 0x0400,
    // HOME button
    WPAD_CLASSIC_BUTTON_HOME = 0x0800,
    // - button
    WPAD_CLASSIC_BUTTON_MINUS = 0x1000,
    // L trigger
    WPAD_CLASSIC_BUTTON_L = 0x2000,
    // D-pad down
    WPAD_CLASSIC_BUTTON_DOWN = 0x4000,
    // D-pad right
    WPAD_CLASSIC_BUTTON_RIGHT = 0x8000,
};

typedef void* (*WPADAlloc)(u32 size);
typedef s32 (*WPADFree)(void* block);

IMPORT(0x8009D804)
void WPADRegisterAllocator(WPADAlloc alloc, WPADFree free);

/*
 * KPAD library
 */

typedef struct {
    float x;
    float y;
} KPADVec2D;

typedef struct {
    float x;
    float y;
    float z;
} KPADVec3D;

typedef struct {
    u32 hold;
    u32 trigger;
    u32 release;

    KPADVec3D acc;
    float accMagnitude;
    float accVariation;

    KPADVec2D pos;
    FILL(0x28, 0x34);

    KPADVec2D angle;
    FILL(0x3C, 0x5C);

    u8 extensionType;
    s8 error;
    s8 posValid;
    u8 format;

    FILL(0x60, 0x84);
} KPADStatus;

static_assert(sizeof(KPADStatus) == 0x84);

IMPORT(0x800AB788)
void KPADInit();

IMPORT(0x800A8E38)
void KPADSetPosParam(u32 chan, float x, float y);

IMPORT(0x800AB05C)
s32 KPADRead(u32 chan, KPADStatus* status, u32 count);

IMPORT(0x800A9028)
void KPADGetProjectionPos(
    KPADVec2D* out, KPADVec2D* in, float rect[4], float ratio
);

EXTERN_C_END
