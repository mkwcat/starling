#include "Heap.hpp"
#include <Import/RVL_OS.h>

static MEMHeapHandle s_mem1Heap;
static MEMHeapHandle s_mem2Heap;

/**
 * Initialize the memory system.
 */
void Heap::Init()
{
    u32 heapStart = AlignUp(OSGetMEM1ArenaLo(), 32);
    u32 heapEnd = AlignDown(OSGetMEM1ArenaHi(), 32);
    s_mem1Heap = MEMCreateExpHeapEx((void*) heapStart, heapEnd - heapStart, 6);

    heapStart = AlignUp(OSGetMEM2ArenaLo(), 32);
    heapEnd = AlignDown(OSGetMEM2ArenaHi(), 32);
    s_mem2Heap = MEMCreateExpHeapEx((void*) heapStart, heapEnd - heapStart, 6);
}

/**
 * Allocate from the MEM1 heap.
 */
void* Heap::AllocMEM1(u32 size, u32 align)
{
    return MEMAllocFromExpHeapEx(s_mem1Heap, size, align);
}

/**
 * Free to the MEM1 heap.
 */
void Heap::FreeMEM1(void* block)
{
    return MEMFreeToExpHeap(s_mem1Heap, block);
}

/**
 * Allocate from the MEM2 heap.
 */
void* Heap::AllocMEM2(u32 size, u32 align)
{
    return MEMAllocFromExpHeapEx(s_mem2Heap, size, align);
}

/**
 * Free to the MEM2 heap.
 */
void Heap::FreeMEM2(void* block)
{
    return MEMFreeToExpHeap(s_mem2Heap, block);
}

TwmReplace(0x80009034) //
  void*
  operator new(size_t size)
{
    return MEMAllocFromExpHeapEx(s_mem1Heap, size, 4);
}

void* operator new[](size_t size)
{
    return MEMAllocFromExpHeapEx(s_mem1Heap, size, 4);
}

void* operator new(size_t size, std::align_val_t align)
{
    return MEMAllocFromExpHeapEx(s_mem1Heap, size, u32(align));
}

void* operator new[](size_t size, std::align_val_t align)
{
    return MEMAllocFromExpHeapEx(s_mem1Heap, size, u32(align));
}

TwmReplace(0x8000905C) //
  void
  operator delete(void* block)
{
    return MEMFreeToExpHeap(s_mem1Heap, block);
}

void operator delete[](void* block)
{
    return MEMFreeToExpHeap(s_mem1Heap, block);
}

void operator delete(void* block, size_t size)
{
    (void) size;

    return MEMFreeToExpHeap(s_mem1Heap, block);
}

void operator delete[](void* block, size_t size)
{
    (void) size;

    return MEMFreeToExpHeap(s_mem1Heap, block);
}

void operator delete(void* block, size_t size, std::align_val_t align)
{
    (void) size;
    (void) align;

    return MEMFreeToExpHeap(s_mem1Heap, block);
}
