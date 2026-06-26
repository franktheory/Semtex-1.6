#include "ragebot.h"

#include "aimbot.h"
#include "movement.h"

#include "../game/entities.h"
#include "../game/memory.h"
#include "../game/trace.h"
#include "../game/weapons.h"
#include "../menu/keybind.h"
#include "../menu/menu_vars.h"
#include "../sdk/pm_types.h"
#include "../sdk/usercmd.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cmath>

namespace
{
    constexpr float k_pi              = 3.14159265f;
    constexpr float k_full_fov        = 180.f;
    constexpr float k_stop_fire_speed = 15.f;

    static bool key_active()
    {
        return keybind::active(
            menu_vars::ragebot_key,
            keybind::parse_mode(menu_vars::ragebot_key_mode),
            menu_vars::ragebot_key_state);
    }

    static float velocity_speed_2d(void* pm)
    {
        if (!pm)
            return 0.f;

        __try
        {
            const Vector vel = pm_velocity(pm);
            return sqrtf(vel.x * vel.x + vel.y * vel.y);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0.f;
        }
    }

    static void auto_stop(usercmd_t* cmd, void* pm)
    {
        if (!cmd || !pm)
            return;

        __try
        {
            Vector vel = pm_velocity(pm);
            vel.z = 0.f;

            const float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
            if (speed < 1.f)
                return;

            const float yaw_rad = cmd->viewangles[1] * (k_pi / 180.f);
            const float sy        = sinf(yaw_rad);
            const float cy        = cosf(yaw_rad);

            const Vector forward(cy, sy, 0.f);
            const Vector right(sy, -cy, 0.f);

            float forward_part = vel.x * forward.x + vel.y * forward.y;
            float side_part    = vel.x * right.x + vel.y * right.y;

            cmd->forwardmove = -forward_part;
            cmd->sidemove    = -side_part;

            const float move_mag = sqrtf(cmd->forwardmove * cmd->forwardmove + cmd->sidemove * cmd->sidemove);
            constexpr float k_max_move = 450.f;
            if (move_mag > k_max_move)
            {
                const float scale = k_max_move / move_mag;
                cmd->forwardmove *= scale;
                cmd->sidemove *= scale;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
}

void ragebot::run(usercmd_t* cmd)
{
    if (!cmd || !menu_vars::ragebot_enabled || !game::movement_allowed() || !game::in_match())
        return;

    if (!key_active())
        return;

    if (menu_vars::ragebot_hitboxes == 0)
        return;

    if (!weapons::can_rage_shoot())
    {
        cmd->buttons &= ~IN_ATTACK;
        return;
    }

    aimbot::clear_pending_angles();

    if (void* pm = movement::cached_pmove())
        trace::bind_pmove(pm);
    else if (void* pm = game::playermove_state())
        trace::bind_pmove(pm);

    entities::refresh_player_info();

    float view[3] = { cmd->viewangles[0], cmd->viewangles[1], 0.f };

    Vector eye{};
    if (!aimbot::get_view_eye(eye))
        return;

    Vector target{};
    const bool require_los = menu_vars::ragebot_autowall;
    if (!aimbot::find_target(view, eye, k_full_fov, menu_vars::ragebot_hitboxes, target, require_los))
        return;

    void* pm = movement::cached_pmove();
    if (!pm)
        pm = game::playermove_state();

    const float speed_before_stop = velocity_speed_2d(pm);

    if (menu_vars::ragebot_auto_stop)
        auto_stop(cmd, pm);

    if (!aimbot::apply_aim(cmd, view, eye, target, 1.f))
        return;

    if (menu_vars::ragebot_auto_fire)
    {
        if (!menu_vars::ragebot_auto_stop || speed_before_stop <= k_stop_fire_speed)
            cmd->buttons |= IN_ATTACK;
        else
            cmd->buttons &= ~IN_ATTACK;
    }
}
