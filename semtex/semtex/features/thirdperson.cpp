#include "thirdperson.h"

#include "../game/memory.h"
#include "../menu/menu_vars.h"
#include "../menu/keybind.h"
#include "../sdk/ref_params.h"
#include "../sdk/usercmd.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using CL_IsThirdPerson_t = int(__cdecl*)();

static CL_IsThirdPerson_t s_orig_is_thirdperson = nullptr;
static uintptr_t          s_cl_funcs_table      = 0;
static bool                 s_hooked              = false;

namespace
{
    constexpr float k_chase_back = 150.f;
    constexpr float k_chase_up   = 16.f;

    static bool key_active()
    {
        return keybind::active(
            menu_vars::thirdperson_key,
            keybind::parse_mode(menu_vars::thirdperson_key_mode),
            menu_vars::thirdperson_key_state);
    }

    static bool should_run(const ref_params_t* params)
    {
        if (!params || !menu_vars::thirdperson_enabled || !key_active())
            return false;

        if (!game::in_match() || params->health <= 0)
            return false;

        if (params->intermission || params->spectator)
            return false;

        return true;
    }
}

bool thirdperson::active()
{
    return menu_vars::thirdperson_enabled && key_active() && game::in_match();
}

static int __cdecl detour_CL_IsThirdPerson()
{
    if (menu_vars::thirdperson_enabled && key_active() && game::in_match())
        return 1;

    if (s_orig_is_thirdperson)
        return s_orig_is_thirdperson();

    return 0;
}

void thirdperson::on_calc_ref_post(ref_params_t* params)
{
    if (!should_run(params))
        return;

    __try
    {
        const float back = -k_chase_back;
        const float up   = k_chase_up;

        params->vieworg[0] += params->right[0] * 0.f
                            + params->up[0] * up
                            + params->forward[0] * back;
        params->vieworg[1] += params->right[1] * 0.f
                            + params->up[1] * up
                            + params->forward[1] * back;
        params->vieworg[2] += params->right[2] * 0.f
                            + params->up[2] * up
                            + params->forward[2] * back;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool thirdperson::init_hooks()
{
    if (s_hooked)
        return true;

    const uintptr_t table = game::cl_dll_funcs();
    if (!table)
        return false;

    auto* slot = reinterpret_cast<CL_IsThirdPerson_t*>(table + cldll_off::HUD_CL_IsThirdPerson);
    s_orig_is_thirdperson = *slot;
    if (!s_orig_is_thirdperson)
        return false;

    *slot = detour_CL_IsThirdPerson;
    s_cl_funcs_table = table;
    s_hooked = true;
    return true;
}

void thirdperson::shutdown_hooks()
{
    if (!s_hooked || !s_cl_funcs_table)
        return;

    if (s_orig_is_thirdperson)
        *reinterpret_cast<CL_IsThirdPerson_t*>(s_cl_funcs_table + cldll_off::HUD_CL_IsThirdPerson) = s_orig_is_thirdperson;

    s_orig_is_thirdperson = nullptr;
    s_cl_funcs_table = 0;
    s_hooked = false;
}
