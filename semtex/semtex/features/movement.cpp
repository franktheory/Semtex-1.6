#include "movement.h"

#include "../features/aimbot.h"
#include "../features/antiaim.h"
#include "../features/norecoil.h"
#include "../features/ragebot.h"
#include "../game/cl_cmd.h"
#include "../game/cl_runtime.h"
#include "../game/memory.h"
#include "../game/trace.h"
#include "../menu/menu_vars.h"
#include "../sdk/pm_types.h"
#include "../sdk/usercmd.h"
#include "../../shared/shared.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

extern semtex_shared_t* g_shm();

using CL_CreateMove_t        = void(__cdecl*)(float, usercmd_t*, int);
using HUD_ClientMove_t       = void(__cdecl*)(void*, int);
using HUD_ClientMoveInit_t   = void(__cdecl*)(void*);
using HUD_PostRunCmd_t       = void(__cdecl*)(void*, void*, usercmd_t*, int, double, unsigned int);

static CL_CreateMove_t        s_orig_createmove     = nullptr;
static HUD_ClientMove_t       s_orig_clientmove       = nullptr;
static HUD_ClientMoveInit_t   s_orig_clientmoveinit = nullptr;
static HUD_PostRunCmd_t       s_orig_postruncmd      = nullptr;
static int                    s_postruncmd_index    = 25;
static uintptr_t              s_cl_funcs_table      = 0;
static void*                  s_live_pmove          = nullptr;
static bool                   s_hooked              = false;

namespace movetype
{
    constexpr int fly    = 5;
    constexpr int noclip = 8;
}

static void* active_pmove()
{
    if (s_live_pmove)
        return s_live_pmove;

    if (!game::movement_allowed())
        return nullptr;

    void* pm = game::playermove_state();
    if (pm)
        s_live_pmove = pm;

    return s_live_pmove;
}

static bool local_on_ground(void* pm)
{
    __try
    {
        const int flags = pm_flags(pm);
        if (flags & FL_ONGROUND)
            return true;

        if (pm_onground(pm) > 0)
            return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return false;
}

static bool bhop_blocked_movetype(void* pm)
{
    __try
    {
        const int mt = pm_movetype(pm);
        if (mt == movetype::noclip || mt == movetype::fly)
            return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return true;
    }

    return false;
}

static void bhop(usercmd_t* cmd)
{
    auto* shm = g_shm();

    if (!menu_vars::bhop_enabled || !cmd)
        return;


    if (!(cmd->buttons & IN_JUMP))
        return;

    void* pm = active_pmove();
    if (!pm)
        return;

    if (bhop_blocked_movetype(pm))
        return;

    __try
    {
        if (shm)
        {
            shm->pmove_addr = reinterpret_cast<uint32_t>(pm);
            shm->bhop_pm_flags = pm_flags(pm);
        }

        cmd->buttons &= ~IN_JUMP;

        if (pm_waterlevel(pm) >= 2 || local_on_ground(pm))
        {
            cmd->buttons |= IN_JUMP;
            if (shm)
                shm->bhop_ground = 1;
        }
        else if (shm)
            shm->bhop_ground = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }
}

static void __cdecl detour_HUD_ClientMove(void* ppmove, int server)
{
    if (s_orig_clientmove)
        s_orig_clientmove(ppmove, server);

    if (game::movement_allowed() && ppmove)
    {
        s_live_pmove = ppmove;
        trace::bind_pmove(ppmove);
    }
    else if (!game::movement_allowed())
        s_live_pmove = nullptr;
}

static void __cdecl detour_HUD_ClientMoveInit(void* ppmove)
{
    if (s_orig_clientmoveinit)
        s_orig_clientmoveinit(ppmove);

    if (game::movement_allowed() && ppmove)
    {
        s_live_pmove = ppmove;
        trace::bind_pmove(ppmove);
    }
}

static void __cdecl detour_CL_CreateMove(float frametime, usercmd_t* cmd, int active)
{
    if (s_orig_createmove)
        s_orig_createmove(frametime, cmd, active);

    if (!game::movement_allowed() || !active || !cmd)
        return;

    if (auto* cl = game::cl())
        cl_runtime::calibrate(cl, cmd);

    auto* shm = g_shm();
    if (shm)
    {
        shm->createmove_count++;
        shm->bhop_active_hits++;
    }

    bhop(cmd);
    ragebot::run(cmd);
    aimbot::run(cmd);
    antiaim::run(cmd);
    norecoil::on_createmove(cmd);
}

static void __cdecl detour_HUD_PostRunCmd(void* from, void* to, usercmd_t* cmd, int runfuncs, double time, unsigned int random_seed)
{
    if (s_orig_postruncmd)
        s_orig_postruncmd(from, to, cmd, runfuncs, time, random_seed);
}

bool movement::init_hooks()
{
    if (s_hooked)
        return true;

    if (!GetModuleHandleA("client.dll"))
        return false;

    const uintptr_t table = game::cl_dll_funcs();
    if (!table)
        return false;

    auto* createmove_slot = reinterpret_cast<CL_CreateMove_t*>(table + cldll_off::HUD_CL_CreateMove);
    auto* clientmove_slot = reinterpret_cast<HUD_ClientMove_t*>(table + cldll_off::HUD_ClientMove);
    auto* clientmoveinit_slot = reinterpret_cast<HUD_ClientMoveInit_t*>(table + cldll_off::HUD_ClientMoveInit);

    s_postruncmd_index = game::resolve_postruncmd_vtable_index(table);
    auto* postruncmd_slot = reinterpret_cast<HUD_PostRunCmd_t*>(table + s_postruncmd_index * sizeof(void*));

    s_orig_createmove = *createmove_slot;
    s_orig_clientmove = *clientmove_slot;
    s_orig_clientmoveinit = *clientmoveinit_slot;
    s_orig_postruncmd = *postruncmd_slot;

    if (!s_orig_createmove || !s_orig_clientmove || !s_orig_clientmoveinit)
        return false;

    *createmove_slot = detour_CL_CreateMove;
    *clientmove_slot = detour_HUD_ClientMove;
    *clientmoveinit_slot = detour_HUD_ClientMoveInit;

    const bool postruncmd_ok = s_orig_postruncmd && game::fn_in_client_dll(reinterpret_cast<void*>(s_orig_postruncmd));
    if (postruncmd_ok)
        *postruncmd_slot = detour_HUD_PostRunCmd;

    s_cl_funcs_table = table;
    s_hooked = true;
    aimbot::init_hooks();

    auto* shm = g_shm();
    if (shm)
    {
        shm->movement_hook_ok = 1;
        shm->cl_funcs_addr = static_cast<uint32_t>(table);
        shm->createmove_fn_addr = reinterpret_cast<uint32_t>(s_orig_createmove);
        if (shm->version >= 13u)
        {
            shm->postruncmd_hook_ok = postruncmd_ok ? 1 : 0;
            shm->postruncmd_vtable_index = s_postruncmd_index;
        }
    }

    return true;
}

void movement::shutdown_hooks()
{
    aimbot::shutdown_hooks();

    if (!s_hooked || !s_cl_funcs_table)
        return;

    if (s_orig_createmove)
        *reinterpret_cast<CL_CreateMove_t*>(s_cl_funcs_table + cldll_off::HUD_CL_CreateMove) = s_orig_createmove;
    if (s_orig_clientmove)
        *reinterpret_cast<HUD_ClientMove_t*>(s_cl_funcs_table + cldll_off::HUD_ClientMove) = s_orig_clientmove;
    if (s_orig_clientmoveinit)
        *reinterpret_cast<HUD_ClientMoveInit_t*>(s_cl_funcs_table + cldll_off::HUD_ClientMoveInit) = s_orig_clientmoveinit;
    if (s_orig_postruncmd)
        *reinterpret_cast<HUD_PostRunCmd_t*>(s_cl_funcs_table + s_postruncmd_index * sizeof(void*)) = s_orig_postruncmd;

    s_orig_createmove = nullptr;
    s_orig_clientmove = nullptr;
    s_orig_clientmoveinit = nullptr;
    s_orig_postruncmd = nullptr;
    s_live_pmove = nullptr;
    s_cl_funcs_table = 0;
    s_hooked = false;

    auto* shm = g_shm();
    if (shm)
        shm->movement_hook_ok = 0;
}

bool movement::hooks_attached()
{
    return s_hooked;
}

void* movement::cached_pmove()
{
    return s_live_pmove;
}

void movement::update_debug_status(semtex_shared_t* shm)
{
    if (!shm)
        return;

    shm->movement_hook_ok = s_hooked ? 1 : 0;
    if (s_cl_funcs_table)
        shm->cl_funcs_addr = static_cast<uint32_t>(s_cl_funcs_table);
    if (s_orig_createmove)
        shm->createmove_fn_addr = reinterpret_cast<uint32_t>(s_orig_createmove);

    if (!s_hooked || !game::in_match())
        return;

    void* pm = active_pmove();
    if (!pm)
        return;

    shm->pmove_addr = reinterpret_cast<uint32_t>(pm);
    __try
    {
        shm->bhop_pm_flags = pm_flags(pm);
        shm->bhop_ground = (pm_flags(pm) & FL_ONGROUND) ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
