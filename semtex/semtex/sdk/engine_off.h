#pragma once

#include <cstddef>

namespace engine_off
{

    constexpr std::ptrdiff_t GetMaxClients = 36 * sizeof(void*);
    constexpr std::ptrdiff_t PM_TraceLine  = 60 * sizeof(void*);
    constexpr std::ptrdiff_t pEventAPI     = 85 * sizeof(void*);
}
