#pragma once

#include <Types.h>

class Heap
{
public:
    /**
     * Initialize the memory system.
     */
    static void Init();

    /**
     * Allocate from the MEM1 heap.
     */
    static void* AllocMEM1(u32 size, u32 align = 4);

    /**
     * Free to the MEM1 heap.
     */
    static void FreeMEM1(void* block);

    /**
     * Allocate from the MEM2 heap.
     */
    static void* AllocMEM2(u32 size, u32 align = 4);

    /**
     * Free to the MEM2 heap.
     */
    static void FreeMEM2(void* block);
};
