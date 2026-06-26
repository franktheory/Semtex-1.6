#pragma once

#include <cstddef>


namespace local_state_off
{
    constexpr std::ptrdiff_t client_punchangle = 0x1B4;
}

inline float* ls_client_punch(void* local_state)
{
    return reinterpret_cast<float*>(static_cast<char*>(local_state) + local_state_off::client_punchangle);
}
