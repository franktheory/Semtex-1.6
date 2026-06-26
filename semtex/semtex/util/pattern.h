#pragma once

#include <Windows.h>
#include <cstdint>

namespace pattern
{
    uint8_t* scan_module(const char* module_name, const char* signature, int offset = 0, int deref_count = 0);
    uint8_t* scan_region(uint8_t* start, size_t size, const char* signature);
}
