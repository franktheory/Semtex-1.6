#pragma once

#include "hl_types.h"
#include "../game/cl_runtime.h"

namespace game
{
    inline double cl_time(client_state_t* cl)
    {
        return *reinterpret_cast<double*>(reinterpret_cast<char*>(cl) + cl_off::time);
    }

    inline int& cl_playernum(client_state_t* cl)
    {
        return *reinterpret_cast<int*>(reinterpret_cast<char*>(cl) + cl_off::playernum);
    }

    inline int& cl_maxclients(client_state_t* cl)
    {
        return *reinterpret_cast<int*>(reinterpret_cast<char*>(cl) + cl_off::maxclients);
    }

    inline player_info_t* cl_players(client_state_t* cl)
    {
        return reinterpret_cast<player_info_t*>(reinterpret_cast<char*>(cl) + cl_off::players);
    }

    inline player_info_t& cl_player(client_state_t* cl, int index)
    {
        return cl_players(cl)[index];
    }

    inline char* cl_levelname(client_state_t* cl)
    {
        return reinterpret_cast<char*>(reinterpret_cast<char*>(cl) + cl_off::levelname);
    }

    inline float* cl_viewangles(client_state_t* cl)
    {
        const std::ptrdiff_t off = cl_runtime::ready() ? cl_runtime::viewangles : cl_off::viewangles;
        return reinterpret_cast<float*>(reinterpret_cast<char*>(cl) + off);
    }

    inline float* cl_punchangle(client_state_t* cl)
    {
        const std::ptrdiff_t off = cl_runtime::ready() ? cl_runtime::punchangle : cl_off::punchangle;
        return reinterpret_cast<float*>(reinterpret_cast<char*>(cl) + off);
    }
}
