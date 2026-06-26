#include "norecoil.h"

#include "../game/memory.h"
#include "../menu/menu_vars.h"
#include "../sdk/ref_params.h"
#include "../../shared/shared.h"

#include <cmath>
#include <cstdint>

extern semtex_shared_t* g_shm();

namespace
{
    constexpr uint32_t k_removal_visual_recoil = 1u << 0;


    float s_norecoil_angle[3] = {};
}

namespace norecoil
{
    void on_calc_ref_pre(ref_params_t* params)
    {
        if (!params || !game::in_match())
            return;

        const bool visual = (menu_vars::visual_removals & k_removal_visual_recoil) != 0;
        const bool anti   = menu_vars::norecoil_enabled;

        if (!visual && !anti)
            return;

        __try
        {
            if (anti)
            {
                s_norecoil_angle[0] = params->punchangle[0] * 2.f;
                s_norecoil_angle[1] = params->punchangle[1] * 2.f;
                s_norecoil_angle[2] = 0.f;

                params->punchangle[0] = 0.f;
                params->punchangle[1] = 0.f;
                params->punchangle[2] = 0.f;
            }

            if (visual)
            {
                params->punchangle[0] = 0.f;
                params->punchangle[1] = 0.f;
                params->punchangle[2] = 0.f;
                params->crosshairangle[0] = 0.f;
                params->crosshairangle[1] = 0.f;
                params->crosshairangle[2] = 0.f;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    void on_calc_ref_post(ref_params_t* params)
    {
        (void)params;
    }

    void update_debug_status()
    {
        auto* shm = g_shm();
        if (!shm)
            return;

        shm->recoil_punch_valid = menu_vars::norecoil_enabled ? 1 : 0;
        shm->recoil_punch_pitch = static_cast<int32_t>(s_norecoil_angle[0] * 50.f);
        shm->recoil_punch_yaw   = static_cast<int32_t>(s_norecoil_angle[1] * 50.f);
    }

    void on_createmove(usercmd_t* cmd)
    {
        if (!menu_vars::norecoil_enabled || !cmd || !game::movement_allowed())
            return;

        if (s_norecoil_angle[0] == 0.f && s_norecoil_angle[1] == 0.f && s_norecoil_angle[2] == 0.f)
            return;

        cmd->viewangles[0] -= s_norecoil_angle[0];
        cmd->viewangles[1] -= s_norecoil_angle[1];
        cmd->viewangles[2] -= s_norecoil_angle[2];

        auto* shm = g_shm();
        if (shm)
        {
            shm->recoil_apply_count++;
            shm->recoil_cmd_match = 1;
        }
    }
}
