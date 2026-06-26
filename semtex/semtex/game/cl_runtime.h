#pragma once

#include "../sdk/usercmd.h"

namespace cl_runtime
{

    constexpr std::ptrdiff_t k_client_viewangles_rva = 0x1277D4;


    constexpr std::ptrdiff_t k_playermove_ptr_rva = 0x12841C;


    constexpr std::ptrdiff_t k_cl_yawspeed_cvar_rva = 0x12769C;

    extern std::ptrdiff_t viewangles;
    extern std::ptrdiff_t cmd;
    extern std::ptrdiff_t punchangle;

    void reset();
    bool ready();
    void calibrate(client_state_t* cl, const usercmd_t* ucmd);
    bool read_punch_global(float out[3]);
}
