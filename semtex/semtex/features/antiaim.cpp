#include "antiaim.h"

#include "../features/movefix.h"
#include "../game/memory.h"
#include "../menu/menu_vars.h"
#include "../sdk/usercmd.h"

#include <cmath>

namespace
{
    void normalize_yaw(float& yaw)
    {
        while (yaw > 180.f)
            yaw -= 360.f;
        while (yaw < -180.f)
            yaw += 360.f;
    }
}

void antiaim::run(usercmd_t* cmd)
{
    if (!cmd || !menu_vars::antiaim_enabled || !game::movement_allowed() || !game::in_match())
        return;

    if (cmd->buttons & IN_USE)
        return;

    if (cmd->buttons & IN_ATTACK)
        return;

    movefix::begin(cmd);

    const float base_yaw = cmd->viewangles[1];

    switch (menu_vars::antiaim_yaw_mode)
    {
    case 0:
    {
        float speed = menu_vars::antiaim_spin_speed;
        if (speed < 1.f)
            speed = 1.f;

        cmd->viewangles[1] = fmodf(game::client_time() * speed * 360.f, 360.f);
        normalize_yaw(cmd->viewangles[1]);
        break;
    }
    case 1:
        cmd->viewangles[1] = base_yaw + 180.f;
        normalize_yaw(cmd->viewangles[1]);
        break;
    default:
        break;
    }

    switch (menu_vars::antiaim_pitch_mode)
    {
    case 0:
        cmd->viewangles[0] = -89.f;
        break;
    case 1:
        cmd->viewangles[0] = 89.f;
        break;
    case 2:
        cmd->viewangles[0] = 0.f;
        break;
    default:
        break;
    }

    movefix::end(cmd);
}
