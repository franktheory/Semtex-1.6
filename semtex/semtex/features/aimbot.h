#pragma once

#include "../sdk/sdk.h"
#include "../../oshgui/Drawing/Graphics.hpp"

struct usercmd_t;
struct ref_params_t;

namespace aimbot
{
    constexpr int k_bone_head = 7;


    constexpr int k_hitbox_count = 5;

    bool init_hooks();
    void shutdown_hooks();

    void run(usercmd_t* cmd);
    void apply_ref_view(ref_params_t* params);
    bool has_pending_angles();
    void clear_pending_angles();
    void update_debug_status();

    void draw_fov_circle(OSHGui::Drawing::Graphics& gfx);
    float fov_radius_px();

    bool get_view_eye(Vector& eye);
    bool find_target(const float view[3], const Vector& eye, float fov_deg, uint32_t hitbox_mask, Vector& out_pos, bool require_line_of_sight = false);
    void compute_aim_angles(const Vector& eye, const Vector& target, float out[3]);
    bool apply_aim(usercmd_t* cmd, const float view[3], const Vector& eye, const Vector& target, float smooth);
}
