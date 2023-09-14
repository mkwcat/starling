#pragma once

#include <System/Types.h>

#include <cstring>

namespace Boot_IOS
{

void SafeFlush(const void* start, u32 size);
bool BootstrapEntry();

} // namespace Boot_IOS
