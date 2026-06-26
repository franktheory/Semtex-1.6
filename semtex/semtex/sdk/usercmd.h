#pragma once

#include "hl_types.h"
#include <cstddef>

#define IN_ATTACK     (1 << 0)
#define IN_JUMP       (1 << 1)
#define IN_DUCK       (1 << 2)
#define IN_FORWARD    (1 << 3)
#define IN_BACK       (1 << 4)
#define IN_USE        (1 << 5)
#define IN_MOVELEFT   (1 << 9)
#define IN_MOVERIGHT  (1 << 10)

struct usercmd_t
{
    short          lerp_msec;
    byte           msec;
    byte           pad0;
    float          viewangles[3];
    float          forwardmove;
    float          sidemove;
    float          upmove;
    byte           lightlevel;
    unsigned short buttons;
    byte           impulse;
    byte           weaponselect;
    int            impact_index;
    float          impact_position[3];
};

static_assert(offsetof(usercmd_t, viewangles) == 4, "usercmd_t::viewangles offset mismatch");
static_assert(offsetof(usercmd_t, buttons) == 30, "usercmd_t::buttons offset mismatch");

namespace cldll_off
{

    constexpr std::ptrdiff_t HUD_ClientMove       = 6 * sizeof(void*);
    constexpr std::ptrdiff_t HUD_ClientMoveInit   = 7 * sizeof(void*);
    constexpr std::ptrdiff_t HUD_CL_CreateMove       = 14 * sizeof(void*);
    constexpr std::ptrdiff_t HUD_CL_IsThirdPerson    = 15 * sizeof(void*);
    constexpr std::ptrdiff_t HUD_CalcRef             = 19 * sizeof(void*);
    constexpr std::ptrdiff_t HUD_PostRunCmd      = 25 * sizeof(void*);
    constexpr std::ptrdiff_t HUD_Studio_Interface = 39 * sizeof(void*);

    constexpr std::ptrdiff_t base_from_studio_iface = -HUD_Studio_Interface;
}
