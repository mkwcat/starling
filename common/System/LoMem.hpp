#pragma once

#include <System/Types.h>
#include <System/Util.h>

typedef struct {
    char gamename[4];
    char company[2];
    u8 disknum;
    u8 gamever;
    u8 streaming;
    u8 streambufsize;
    u8 pad[14];
    u32 wii_magic;
    u32 gc_magic;
} os_disc_id_t;

typedef enum {
    OS_BOOT_NORMAL = 0x0d15ea5e
} os_boot_type_t;

typedef struct {
    u32 boot_type;
    u32 version;
    u32 mem1_size;
    u32 console_type;
    u32 arena_low;
    u32 arena_high;
    void* fst;
    u32 fst_size;
} os_system_info_t;

typedef struct {
    u32 enabled;
    u32 exception_mask;
    void* destination;
    u8 temp[0x14];
    u8 hook[0x24];
    u8 padding[0x3c];
} os_debugger_t;

typedef enum {
    OS_TV_MODE_NTSC,
    OS_TV_MODE_PAL,
    OS_TV_MODE_MPAL,
    OS_TV_MODE_DEBUG,
    OS_TV_MODE_DEBUG_PAL,
    OS_TV_MODE_PAL60,
} os_tv_mode_t;

struct BI2 {
    FILL(0x0, 0x30);
    u32 dualLayerValue;
    FILL(0x30, 0x2000);
};

typedef struct {
    void* current_context_phy;
    u32 previous_interrupt_mask;
    u32 current_interrupt_mask;
    u32 tv_mode;
    u32 aram_size;
    void* current_context;
    void* default_thread;
    void* thread_queue_head;
    void* thread_queue_tail;
    void* current_thread;
    u32 debug_monitor_size;
    void* debug_monitor_location;
    u32 simulated_memory_size;
    BI2* bi2;
    u32 bus_speed;
    u32 cpu_speed;
} os_thread_info_t;

typedef struct {
    os_disc_id_t disc; /* 0x0 */
    os_system_info_t info; /* 0x20 */
    os_debugger_t debugger; /* 0x40 */
    os_thread_info_t threads; /* 0xc0 */
} os_early_globals_t;

static os_early_globals_t* const os0 = (os_early_globals_t*) 0x80000000;

typedef struct {
    void* exception_handlers[0x10]; /* 0x0 */
    void* irq_handlers[0x20]; /* 0x40 */
    u8 paddingc0[0xe3 - 0xc0]; /* 0xc0 */
    u8 pad_recal;
    u8 paddinge4[0x100 - 0xe4]; /* 0xe4 */
    u32 mem1_size; /* 0x100 */
    u32 mem1_simulated_size; /* 0x104 */
    void* mem1_end; /* 0x108 */
    u8 padding10c[0x110 - 0x10c]; /* 0x10c */
    void* fst; /* 0x110 */
    u8 padding114[0x118 - 0x114]; /* 0x114 */
    u32 mem2_size; /* 0x118 */
    u32 mem2_simulated_size; /* 0x11c */
    u32 mem2_end; /* 0x120 */
    u32 usable_mem2_start; /* 0x124 */
    u32 usable_mem2_end; /* 0x128 */
    u8 padding12c[0x130 - 0x12c]; /* 0x12c */
    u32 ios_heap_start; /* 0x130 */
    u32 ios_heap_end; /* 0x134 */
    u32 hollywood_version; /* 0x138 */
    u8 padding13c[0x140 - 0x13c]; /* 0x13c */
    u16 ios_number; /* 0x140 */
    u16 ios_revision; /* 0x142 */
    u32 ios_build_date; /* 0x144 */
    u8 padding148[0x158 - 0x148]; /* 0x148 */
    u32 gddr_vendor_id; /* 0x158 */
    u32 legacy_di; /* 0x15c */
    u32 init_semaphore; /* 0x160 */
    u32 mios_flag; /* 0x164 */
    u8 padding168[0x180 - 0x168]; /* 0x168 */
    char application_name[4]; /* 0x180 */
    os_disc_id_t* id; /* 0x184 */
    u16 expected_ios_number; /* 0x188 */
    u16 expected_ios_revision; /* 0x18a */
    u32 launch_code; /* 0x18c */
    u32 return_code; /* 0x190 */
    u8 padding194[0x19c - 0x194]; /* 0x194 */
    u8 dual_layer_value /* 0x19c */;
} os_late_globals_t;

static os_late_globals_t* const os1 = (os_late_globals_t*) 0x80003000;
