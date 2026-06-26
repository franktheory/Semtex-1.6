#pragma once

#include "keybind.h"
#include <cstdint>

namespace menu_vars
{
    inline bool visuals_enabled = true;
    inline bool name_esp = true;
    inline bool weapon_esp = true;
    inline bool skeleton_esp = true;
    inline bool box_esp = true;
    inline bool teammates = false;

    inline bool chams     = false;
    inline bool chams_xqz = true;

    inline float chams_vis_color[4] = { 255.f, 255.f, 200.f, 0.f };
    inline float chams_xqz_color[4] = { 255.f, 0.f, 100.f, 255.f };

    inline uint32_t visual_removals = 0;

    inline bool thirdperson_enabled = false;
    inline int  thirdperson_key     = 0;
    inline int  thirdperson_key_mode = static_cast<int>(keybind::mode::hold);
    inline keybind::state_t thirdperson_key_state{};

    inline bool norecoil_enabled = false;

    inline bool bhop_enabled = false;

    inline bool     aimbot_enabled           = false;
    inline int      aimbot_key               = 0;
    inline int      aimbot_key_mode          = static_cast<int>(keybind::mode::hold);
    inline keybind::state_t aimbot_key_state{};
    inline int      aimbot_fov               = 5;
    inline bool     aimbot_fov_circle        = true;
    inline float    aimbot_smooth            = 1.f;
    inline uint32_t aimbot_hitboxes          = 1u;

    inline bool  antiaim_enabled   = false;
    inline int   antiaim_yaw_mode  = 0;
    inline int   antiaim_pitch_mode = 0;
    inline float antiaim_spin_speed = 30.f;

    inline bool     ragebot_enabled    = false;
    inline int      ragebot_key        = 0;
    inline int      ragebot_key_mode   = static_cast<int>(keybind::mode::always);
    inline keybind::state_t ragebot_key_state{};
    inline bool     ragebot_auto_fire  = true;
    inline bool     ragebot_auto_stop  = true;
    inline bool     ragebot_autowall   = true;
    inline uint32_t ragebot_hitboxes   = 1u;
}
