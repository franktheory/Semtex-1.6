#include "pattern.h"

static uint8_t* to_ptr(uintptr_t value)
{
    return reinterpret_cast<uint8_t*>(value);
}

uint8_t* pattern::scan_region(uint8_t* start, size_t size, const char* signature)
{
    if (!start || !signature || !size)
        return nullptr;

    uint8_t* scan = start;
    const uint8_t* end = start + size;

    for (auto current = scan; current < end; ++current)
    {
        const char* pat = signature;
        uint8_t* candidate = current;
        bool matched = true;

        while (*pat)
        {
            if (candidate >= end)
            {
                matched = false;
                break;
            }

            if (*pat == ' ')
            {
                ++pat;
                continue;
            }

            if (*pat == '?')
            {
                ++pat;
                if (*pat == '?')
                    ++pat;
                ++candidate;
                continue;
            }

            const auto nibble = [](char c) -> int
            {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                return -1;
            };

            const int high = nibble(pat[0]);
            const int low  = nibble(pat[1]);
            if (high < 0 || low < 0)
            {
                matched = false;
                break;
            }

            if (*candidate != static_cast<uint8_t>((high << 4) | low))
            {
                matched = false;
                break;
            }

            pat += 2;
            ++candidate;
        }

        if (matched)
            return current;
    }

    return nullptr;
}

uint8_t* pattern::scan_module(const char* module_name, const char* signature, int offset, int deref_count)
{
    const HMODULE mod = GetModuleHandleA(module_name);
    if (!mod)
        return nullptr;

    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
    const auto nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
    auto* start = reinterpret_cast<uint8_t*>(mod);
    const size_t size = nt->OptionalHeader.SizeOfImage;

    uint8_t* found = scan_region(start, size, signature);
    if (!found)
        return nullptr;

    uintptr_t address = reinterpret_cast<uintptr_t>(found) + offset;
    for (int i = 0; i < deref_count; ++i)
        address = *reinterpret_cast<uintptr_t*>(address);

    return to_ptr(address);
}
