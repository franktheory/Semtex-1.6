#pragma once

#include "hl_types.h"

constexpr int FL_ONGROUND = (1 << 9);

namespace pm_off
{
    constexpr std::ptrdiff_t forward    = 20;
    constexpr std::ptrdiff_t right      = 32;
    constexpr std::ptrdiff_t velocity   = 92;
    constexpr std::ptrdiff_t punchangle = 160;
    constexpr std::ptrdiff_t flags      = 184;
    constexpr std::ptrdiff_t movetype   = 220;
    constexpr std::ptrdiff_t onground   = 224;
    constexpr std::ptrdiff_t waterlevel = 228;
}

inline Vector pm_velocity(void* pm)
{
    return *reinterpret_cast<Vector*>(static_cast<char*>(pm) + pm_off::velocity);
}

inline float* pm_punchangle(void* pm)
{
    return reinterpret_cast<float*>(static_cast<char*>(pm) + pm_off::punchangle);
}

inline int pm_flags(void* pm)
{
    return *reinterpret_cast<int*>(static_cast<char*>(pm) + pm_off::flags);
}

inline int pm_movetype(void* pm)
{
    return *reinterpret_cast<int*>(static_cast<char*>(pm) + pm_off::movetype);
}

inline int pm_onground(void* pm)
{
    return *reinterpret_cast<int*>(static_cast<char*>(pm) + pm_off::onground);
}

inline int pm_waterlevel(void* pm)
{
    return *reinterpret_cast<int*>(static_cast<char*>(pm) + pm_off::waterlevel);
}

inline float* pm_forward(void* pm)
{
    return reinterpret_cast<float*>(static_cast<char*>(pm) + pm_off::forward);
}

inline float* pm_right(void* pm)
{
    return reinterpret_cast<float*>(static_cast<char*>(pm) + pm_off::right);
}
