// main.S - LZMA loader init
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0

#include <ASM.s>

#define SRR0 26
#define SRR1 27
#define PVR 287
#define GQR0 912
#define GQR1 913
#define GQR2 914
#define GQR3 915
#define GQR4 916
#define GQR5 917
#define GQR6 918
#define GQR7 919
#define HID2 920
#define HID5 944
#define BCR 949
#define L2CR 1017
#define HID0 1008
#define HID4 1011
#define IBAT0U 0x210
#define IBAT0L 0x211
#define IBAT1U 0x212
#define IBAT1L 0x213
#define IBAT2U 0x214
#define IBAT2L 0x215
#define IBAT3U 0x216
#define IBAT3L 0x217
#define DBAT0U 0x218
#define DBAT0L 0x219
#define DBAT1U 0x21A
#define DBAT1L 0x21B
#define DBAT2U 0x21C
#define DBAT2L 0x21D
#define DBAT3U 0x21E
#define DBAT3L 0x21F
#define IBAT4U 0x230
#define IBAT4L 0x231
#define IBAT5U 0x232
#define IBAT5L 0x233
#define IBAT6U 0x234
#define IBAT6L 0x235
#define IBAT7U 0x236
#define IBAT7L 0x237
#define DBAT4U 0x238
#define DBAT4L 0x239
#define DBAT5U 0x23A
#define DBAT5L 0x23B
#define DBAT6U 0x23C
#define DBAT6L 0x23D
#define DBAT7U 0x23E
#define DBAT7L 0x23F

/**
 * Temporary stack data.
 */
ASM_SYMBOL_START(_Stack, .bss)
	.space  0x1000
	.global _StackEnd
_StackEnd:
ASM_SYMBOL_END(_Stack)

/**
 * Entry point used by the Homebrew Channel and Wii U.
 */
ASM_FUNCTION_START(_start, .text)
	b       PPCBootEntry

	.ascii  "_arg"
	.global HBCArgvData
HBCArgvData:
	.space  0x18
ASM_FUNCTION_END(_start)

/**
 * Low level PPC boot entry point.
 */
ASM_SYMBOL_START(PPCBootEntry, .stub)
	// Check if we're in Wii U mode
	mfspr   r4, PVR
	rlwinm  r4, r4, 16, 16, 31
	cmplwi  r4, 0x7001
	bne-    L_CallRVLStartup

	// If Wii U, then enter Wii compatibility mode
	// Init L2 cache
	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, ~0x0
	mtspr   L2CR, r3
	sync

	mfspr   r3, L2CR
	oris    r3, r3, 0x20
	mtspr   L2CR, r3

L_L2InitCafe_1:
	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 0x1
	cmpwi   r3, 0
	bne+    L_L2InitCafe_1

	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 11, 9
	mtspr   L2CR, r3

L_L2InitCafe_2:
	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 0x1
	cmpwi   r3, 0
	bne+    L_L2InitCafe_2

	// Disable Wii U specific registers
	b       L_CafeAligned
	.balign 32
L_CafeAligned:
	mfspr   r3, HID5
	rlwinm  r3, r3, 0, ~0x80000000
	mtspr   HID5, r3

	nop
	sync
	nop
	nop
	nop

	mfspr   r3, BCR
	oris    r3, r3, 0x1000
	mtspr   BCR, r3
	li      r4, 255

L_CafeSync:
	subi    r4, r4, 1
	cmpwi   r4, 0
	bne+    L_CafeSync
	nop

L_CallRVLStartup:
	lis     r3, L_RVLStartup@h
	andis.  r3, r3, 0x7FFF
	ori     r3, r3, L_RVLStartup@l
	mtspr   SRR0, r3
	li      r4, 0
	mtspr   SRR1, r4
	rfi

	.balign 0x100

L_RVLStartup:
#define REG_HID0_DEFAULT 0x00110C64
	lis     r4, REG_HID0_DEFAULT@h
	ori     r4, r4, REG_HID0_DEFAULT@l
	mtspr   HID0, r4

#define MACHINE_STATE_DEFAULT 0x00002000
	lis     r4, MACHINE_STATE_DEFAULT@h
	ori     r4, r4, MACHINE_STATE_DEFAULT@l
	mtmsr   r4


#define REG_HID4_DEFAULT 0x83900000
	lis     r4, REG_HID4_DEFAULT@h
	ori     r4, r4, REG_HID4_DEFAULT@l
	mtspr   HID4, r4

	mfspr   r3, HID0
	ori     r4, r3, 0xC000
	mtspr   HID0, r4
	isync

	#define BAT_DEFAULT 0
	lis     r4, BAT_DEFAULT@h
	ori     r4, r4, BAT_DEFAULT@l

	.irp    i, DBAT0U, DBAT1U, DBAT2U, DBAT3U, DBAT4U, DBAT5U, DBAT6U, DBAT7U
	mtspr   \i, r4
	.endr
	.irp    i, DBAT0L, DBAT1L, DBAT2L, DBAT3L, DBAT4L, DBAT5L, DBAT6L, DBAT7L
	mtspr   \i, r4
	.endr

	.irp    i, IBAT0U, IBAT1U, IBAT2U, IBAT3U, IBAT4U, IBAT5U, IBAT6U, IBAT7U
	mtspr   \i, r4
	.endr
	.irp    i, IBAT0L, IBAT1L, IBAT2L, IBAT3L, IBAT4L, IBAT5L, IBAT6L, IBAT7L
	mtspr   \i, r4
	.endr

#define SEGMENT_DEFAULT 0x80000000
	lis     r4, SEGMENT_DEFAULT@ha
	addi    r4, r4, SEGMENT_DEFAULT@l
	.irp    i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	mtsr    \i, r4
	.endr

#define X -1

#define LOAD_VALUE(_REGISTER, _VALUE)                                          \
	lis     _REGISTER, (_VALUE)@h;                                             \
	ori     _REGISTER, _REGISTER, (_VALUE)@l

#define BITS(_VALUE, _OFFSET, _SIZE)                                           \
    (((_VALUE) & ((1 << (_SIZE)) - 1)) << (_OFFSET))

#define CONFIG_BAT(_DBAT, _IBAT, _BEPI, _BL, _VS, _VP, _BRPN, _WIMG, _PP)      \
    LOAD_VALUE(r4,                                                             \
	    BITS(_BRPN >> 1, 17, 15) |                                             \
        BITS(_WIMG, 3, 4) |                                                    \
		BITS(_PP, 0, 2)                                                        \
	);                                                                         \
    LOAD_VALUE(r3,                                                             \
		BITS(_BEPI >> 1, 17, 15) |                                             \
		BITS(_BL >> 1, 2, 11) |                                                \
		BITS(_VS, 1, 1) |                                                      \
		BITS(_VP, 0, 1)                                                        \
    );                                                                         \
    .if     _DBAT != X;                                                        \
    mtspr   DBAT##_DBAT##L, r4;                                                \
    mtspr   DBAT##_DBAT##U, r3;                                                \
    isync;                                                                     \
    .endif;                                                                    \
    .if     _IBAT != X;                                                        \
    mtspr   IBAT##_IBAT##L, r4;                                                \
    mtspr   IBAT##_IBAT##U, r3;                                                \
    isync;                                                                     \
    .endif

    CONFIG_BAT(0, X, 0x8000, 0x0FF, 1, 1, 0x0000, 0x0, 0x2)
    CONFIG_BAT(1, X, 0x8100, 0x07F, 1, 1, 0x0100, 0x0, 0x2)
    CONFIG_BAT(2, X, 0x9000, 0x1FF, 1, 1, 0x1000, 0x0, 0x2)
    CONFIG_BAT(3, X, 0x9200, 0x0FF, 1, 1, 0x1200, 0x0, 0x2)
    CONFIG_BAT(4, X, 0x9300, 0x03F, 1, 1, 0x1300, 0x0, 0x2)
    CONFIG_BAT(5, X, 0x9340, 0x01F, 1, 1, 0x1340, 0x0, 0x2)
	CONFIG_BAT(6, X, 0xC000, 0xFFF, 1, 1, 0x0000, 0x5, 0x2)
	CONFIG_BAT(7, X, 0xD000, 0x3FF, 1, 1, 0x1000, 0x5, 0x2)

	CONFIG_BAT(X, 0, 0x8000, 0x03F, 1, 1, 0x0000, 0x0, 0x2)

	lis     r3, 0x0
	li      r4, 0x0
	stw     r4, 0xF4(r3)

	lis     r3, PPCAppStartup@ha
	ori     r3, r3, PPCAppStartup@l
	mtspr   SRR0, r3

	mfmsr   r4
	ori     r4, r4, 0x30
	mtspr   SRR1, r4
	rfi
ASM_FUNCTION_END(PPCBootEntry)

ASM_FUNCTION_START(PPCAppStartup, .text)
	// Initialize registers
	.irp    i, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14
	li      \i, 0
	.endr
	.irp    i, r15, r16, r17, r18, r19, r20, r21, r22, r23, r24, r25, r26, r27
	li      \i, 0
	.endr
	.irp    i, r28, r29, r30, r31
	li      \i, 0
	.endr

	lis     r1, _StackEnd@h
	ori     r1, r1, _StackEnd@l

	// Initialize hardware
	// Enable floating point
	mfmsr   r0
	ori     r0, r0, 0x2000
	mtmsr   r0

	// PS Init
	mfspr   r3, HID2
	oris    r3, r3, 0xA000
	mtspr   HID2, r3

	// ICFlashInvalidate
	mfspr   r3, HID0
	ori     r3, r3, 0x800
	mtspr   HID0, r3
	sync

	// Set GQRs
	li      r3, 0
	.irp    i, GQR0, GQR1, GQR2, GQR3, GQR4, GQR5, GQR6, GQR7
	mtspr   \i, r3
	.endr

	// Init FPRs
	mfspr   r3, HID2
	rlwinm. r3, r3, 3, 1
	beq-    L_SkipPairedSingles

	lis     r3, _PSZero@ha
	addi    r3, r3, _PSZero@l
	psq_l   f0, 0(r3), 0, 0

	.irp    i, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15
	ps_mr   \i, f0
	.endr
	.irp    i, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28
	ps_mr   \i, f0
	.endr
	.irp    i, f29, f30, f31
	ps_mr   \i, f0
	.endr

L_SkipPairedSingles:
	lis     r3, _FPRZero@ha
	lfd     f0, _FPRZero@l(r3)

	.irp    i, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15
	fmr     \i, f0
	.endr
	.irp    i, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28
	fmr     \i, f0
	.endr
	.irp    i, f29, f30, f31
	fmr     \i, f0
	.endr

	mtfsf   255, f0

	// Initialize cache
	mfspr   r3, HID0
	rlwinm. r0, r3, 0, 0x200000
	bne-    L_SkipICEnable

	// Enable instruction cache
	isync
	mfspr   r3, HID0
	ori     r3, r3, 0x8000
	mtspr   HID0, r3

L_SkipICEnable:
	mfspr   r3, HID0
	rlwinm. r0, r3, 0, 0x400000
	bne-    L_SkipDCEnable

	// Enable data cache
	sync
	mfspr   r3, HID0
	ori     r3, r3, 0x4000
	mtspr   HID0, r3

L_SkipDCEnable:
	mfspr   r3, L2CR
	rlwinm. r0, r3, 0, 0x1
	bne-    L_SkipL2Setup

	// Backup MSR
	mfmsr   r30
	sync
	li      r3, 0x30
	mtmsr   r3
	sync
	sync

	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, ~0x80000000
	mtspr   L2CR, r3
	sync

	mfspr   r3, L2CR
	oris    r3, r3, 0x20
	mtspr   L2CR, r3

L_L2Init_1:
	mfspr   r3, L2CR
	rlwinm. r0, r3, 0, 0x1
	bne+    L_L2Init_1

	mfspr   r3, L2CR
	rlwinm  r3, r3, 0, 11, 9
	mtspr   L2CR, r3

L_L2Init_2:
	mfspr   r3, L2CR
	rlwinm. r0, r3, 0, 0x1
	bne+    L_L2Init_2

	// Restore MSR backup
	mtmsr   r30

	mfspr   r3, L2CR
	oris    r0, r3, 0x8000
	rlwinm  r3, r0, 0, 11, 9
	mtspr   L2CR, r3

L_SkipL2Setup:

	// Hardware initialized
	// Jump to loader
	bl      load
L_InfiniteLoop:
	b       L_InfiniteLoop
ASM_FUNCTION_END(PPCAppStartup)

ASM_SYMBOL_START(_PSZero, .rodata)
	.double 0
ASM_SYMBOL_END(_PSZero)

ASM_SYMBOL_START(_FPRZero, .rodata)
	.double 0
ASM_SYMBOL_END(_FPRZero)


