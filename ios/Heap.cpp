#include "Syscalls.h"
#include "System.hpp"
#include <cassert>

void* operator new(std::size_t size)
{
    void* block = IOS_Alloc(System::GetHeap(), size);
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size)
{
    void* block = IOS_Alloc(System::GetHeap(), size);
    assert(block != nullptr);
    return block;
}

void* operator new(std::size_t size, std::align_val_t align)
{
    void* block =
        IOS_AllocAligned(System::GetHeap(), size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
}

void* operator new[](std::size_t size, std::align_val_t align)
{
    void* block =
        IOS_AllocAligned(System::GetHeap(), size, static_cast<u32>(align));
    assert(block != nullptr);
    return block;
}

void operator delete(void* ptr)
{
    IOS_Free(System::GetHeap(), ptr);
}

void operator delete[](void* ptr)
{
    IOS_Free(System::GetHeap(), ptr);
}

void operator delete(void* ptr, std::size_t size)
{
    (void) size;

    IOS_Free(System::GetHeap(), ptr);
}

void operator delete[](void* ptr, std::size_t size)
{
    (void) size;

    IOS_Free(System::GetHeap(), ptr);
}

void operator delete(void* ptr, std::size_t size, std::align_val_t align)
{
    (void) size;
    (void) align;

    IOS_Free(System::GetHeap(), ptr);
}

void operator delete[](void* ptr, std::size_t size, std::align_val_t align)
{
    (void) size;
    (void) align;

    IOS_Free(System::GetHeap(), ptr);
}
