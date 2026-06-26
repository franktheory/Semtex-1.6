#include "movefix.h"

#include "../sdk/usercmd.h"

#include <cmath>

namespace
{
    constexpr float k_pi = 3.14159265f;

    float s_forwardmove = 0.f;
    float s_sidemove    = 0.f;
    float s_upmove      = 0.f;
    float s_old_yaw     = 0.f;

    static float normalize_delta(float delta)
    {
        while (delta > 180.f)
            delta -= 360.f;
        while (delta < -180.f)
            delta += 360.f;
        return delta;
    }
}

void movefix::begin(usercmd_t* cmd)
{
    if (!cmd)
        return;

    s_forwardmove = cmd->forwardmove;
    s_sidemove    = cmd->sidemove;
    s_upmove      = cmd->upmove;
    s_old_yaw     = cmd->viewangles[1];
}

void movefix::end(usercmd_t* cmd)
{
    if (!cmd)
        return;

    const float delta = normalize_delta(cmd->viewangles[1] - s_old_yaw);
    const float rad   = delta * (k_pi / 180.f);
    const float cos_d = cosf(rad);
    const float sin_d = sinf(rad);

    cmd->forwardmove = cos_d * s_forwardmove - sin_d * s_sidemove;
    cmd->sidemove    = sin_d * s_forwardmove + cos_d * s_sidemove;
    cmd->upmove      = s_upmove;
}
