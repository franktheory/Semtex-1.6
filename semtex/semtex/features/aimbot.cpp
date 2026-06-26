#include "aimbot.h"

#include "../inc.h"
#include "../features/movement.h"
#include "../features/norecoil.h"
#include "../features/thirdperson.h"
#include "../game/entities.h"
#include "../game/memory.h"
#include "../game/trace.h"
#include "../game/world.h"
#include "../features/visuals.h"
#include "../menu/menu_vars.h"
#include "../menu/keybind.h"
#include "../sdk/ref_params.h"
#include "../sdk/usercmd.h"
#include "../../shared/shared.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cmath>

extern semtex_shared_t* g_shm();

using HUD_CalcRef_t = void(__cdecl*)(ref_params_t*);

static HUD_CalcRef_t s_orig_calc_ref = nullptr;
static uintptr_t     s_cl_funcs_table = 0;
static bool          s_calc_ref_hooked = false;

namespace
{
    constexpr float k_pi = 3.14159265f;

    float       s_pending_angles[3]{};
    bool        s_has_pending_angles = false;
    int32_t     s_last_fail          = 0;

    static bool legit_key_active()
    {
        return keybind::active(
            menu_vars::aimbot_key,
            keybind::parse_mode(menu_vars::aimbot_key_mode),
            menu_vars::aimbot_key_state);
    }

    static bool rage_active()
    {
        return menu_vars::ragebot_enabled && keybind::active(
            menu_vars::ragebot_key,
            keybind::parse_mode(menu_vars::ragebot_key_mode),
            menu_vars::ragebot_key_state);
    }

    static void calc_angles(const Vector& eye, const Vector& target, float out[3])
    {
        const Vector delta(target.x - eye.x, target.y - eye.y, target.z - eye.z);
        const float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);

        out[0] = -atan2f(delta.z, hyp) * (180.f / k_pi);
        out[1] = atan2f(delta.y, delta.x) * (180.f / k_pi);
        out[2] = 0.f;

        if (out[0] > 89.f)  out[0] = 89.f;
        if (out[0] < -89.f) out[0] = -89.f;

        while (out[1] > 180.f)  out[1] -= 360.f;
        while (out[1] < -180.f) out[1] += 360.f;
    }

    static void smooth_angles(const float current[3], const float target[3], float smooth, float out[3])
    {
        float dp = target[0] - current[0];
        float dy = target[1] - current[1];
        while (dy > 180.f)  dy -= 360.f;
        while (dy < -180.f) dy += 360.f;

        const float factor = 1.f / (smooth < 1.f ? 1.f : smooth);
        out[0] = current[0] + dp * factor;
        out[1] = current[1] + dy * factor;
        out[2] = 0.f;

        if (out[0] > 89.f)  out[0] = 89.f;
        if (out[0] < -89.f) out[0] = -89.f;

        while (out[1] > 180.f)  out[1] -= 360.f;
        while (out[1] < -180.f) out[1] += 360.f;
    }

    static float angular_fov_to(const float view[3], const Vector& eye, const Vector& target)
    {
        float aim[3]{};
        calc_angles(eye, target, aim);

        float dp = aim[0] - view[0];
        float dy = aim[1] - view[1];
        while (dy > 180.f)  dy -= 360.f;
        while (dy < -180.f) dy += 360.f;
        return sqrtf(dp * dp + dy * dy);
    }

    static constexpr int k_hitbox_bones[aimbot::k_hitbox_count] = { 7, 6, 5, 3, 1 };
    static constexpr float k_hitbox_height_frac[aimbot::k_hitbox_count] = { 0.92f, 0.82f, 0.65f, 0.45f, 0.25f };

    static bool get_aim_point(int entity_index, int hitbox_bit, Vector& out)
    {
        if (hitbox_bit < 0 || hitbox_bit >= aimbot::k_hitbox_count)
            return false;

        const int bone = k_hitbox_bones[hitbox_bit];
        if (visuals::get_player_bone_pos(entity_index, bone, out))
            return true;

        if (hitbox_bit == 0 && visuals::get_player_head_pos(entity_index, out))
            return true;

        cl_entity_t* ent = game::cl_entity(entity_index);
        if (!ent || !ent->player || ent->origin.IsZero())
            return false;

        float height = 72.f;
        float base_z = 0.f;
        if (ent->curstate.maxs.z > ent->curstate.mins.z)
        {
            height = ent->curstate.maxs.z - ent->curstate.mins.z;
            base_z   = ent->curstate.mins.z;
        }

        out = ent->origin;
        out.z += base_z + height * k_hitbox_height_frac[hitbox_bit];
        return true;
    }

    static bool get_best_aim_point(int entity_index, uint32_t hitbox_mask, const float view[3], const Vector& eye,
        float aim_fov_deg, Vector& out_pos, float& out_fov)
    {
        float best_fov = aim_fov_deg;
        bool  found    = false;

        for (int bit = 0; bit < aimbot::k_hitbox_count; ++bit)
        {
            if ((hitbox_mask & (1u << bit)) == 0)
                continue;

            Vector point{};
            if (!get_aim_point(entity_index, bit, point))
                continue;

            const float fov = angular_fov_to(view, eye, point);
            if (fov > aim_fov_deg)
                continue;

            if (!found || fov < best_fov)
            {
                best_fov = fov;
                out_pos  = point;
                found    = true;
            }
        }

        out_fov = best_fov;
        return found;
    }

    static void set_view_angles(usercmd_t* cmd, const float angles[3])
    {
        if (!cmd)
            return;

        cmd->viewangles[0] = angles[0];
        cmd->viewangles[1] = angles[1];
        cmd->viewangles[2] = 0.f;
    }

    static void apply_client_view(ref_params_t* params, const float angles[3])
    {
        if (!params)
            return;

        params->cl_viewangles[0] = angles[0];
        params->cl_viewangles[1] = angles[1];
        params->cl_viewangles[2] = 0.f;


        params->viewangles[0] = angles[0] + params->punchangle[0];
        params->viewangles[1] = angles[1] + params->punchangle[1];
        params->viewangles[2] = angles[2] + params->punchangle[2];
    }

    static int team_for_slot(int slot)
    {
        if (slot < 0)
            return TEAM_UNASSIGNED;

        if (auto* extra = game_extra_info())
        {
            const int team = static_cast<int>(extra[slot + 1].teamnumber);
            if (team == TEAM_TERRORIST || team == TEAM_CT)
                return team;
        }

        if (const auto it = entities::players().find(slot + 1); it != entities::players().end())
        {
            if (it->second.state)
            {
                const int team = static_cast<int>(it->second.state->team);
                if (team == TEAM_TERRORIST || team == TEAM_CT)
                    return team;
            }
        }

        return TEAM_UNASSIGNED;
    }

    static int local_team()
    {
        const int slot = game::local_slot();
        if (slot < 0)
            return TEAM_UNASSIGNED;
        return team_for_slot(slot);
    }

    static bool is_enemy_slot(int slot)
    {
        const int lt = local_team();
        const int pt = team_for_slot(slot);
        if (lt == TEAM_UNASSIGNED || pt == TEAM_UNASSIGNED)
            return false;
        return lt != pt;
    }

    static bool is_alive_entity(int entity_index)
    {
        if (const auto it = entities::players().find(entity_index); it != entities::players().end())
        {
            if (it->second.valid() && !it->second.alive())
                return false;
        }

        if (auto* extra = game_extra_info())
        {
            const auto& info = extra[entity_index];
            if (info.dead)
                return false;
        }

        return true;
    }

    static bool find_best_target(const float view[3], const Vector& eye, float aim_fov_deg, uint32_t hitbox_mask, Vector& out_pos, bool require_line_of_sight)
    {
        float best_fov = aim_fov_deg;
        bool  found    = false;

        const int local = game::local_slot() + 1;
        int       scan  = 0;
        int       heads = 0;

        for (int entity_index = 1; entity_index <= MAX_CLIENTS; ++entity_index)
        {
            if (entity_index == local)
                continue;

            const int slot = entity_index - 1;
            if (!is_enemy_slot(slot))
                continue;

            if (!is_alive_entity(entity_index))
                continue;

            ++scan;

            Vector aim_point{};
            float  point_fov = 0.f;
            if (!get_best_aim_point(entity_index, hitbox_mask, view, eye, aim_fov_deg, aim_point, point_fov))
                continue;

            if (require_line_of_sight && !trace::line_clear(eye, aim_point, entity_index))
                continue;

            if (!found || point_fov < best_fov)
            {
                best_fov = point_fov;
                out_pos  = aim_point;
                found    = true;
            }
        }

        if (auto* shm = g_shm())
        {
            shm->aimbot_scan_players = scan;
            shm->aimbot_head_ok      = heads;
        }

        return found;
    }

    static bool apply_aim_internal(usercmd_t* cmd, const float view[3], const Vector& eye, const Vector& target, float smooth)
    {
        if (!cmd)
            return false;

        float aim_angles[3]{};
        calc_angles(eye, target, aim_angles);

        float angles[3]{};
        smooth_angles(view, aim_angles, smooth, angles);

        s_pending_angles[0] = angles[0];
        s_pending_angles[1] = angles[1];
        s_pending_angles[2] = 0.f;
        s_has_pending_angles = true;
        s_last_fail = 0;

        set_view_angles(cmd, angles);

        if (auto* shm = g_shm())
            shm->aimbot_target_found++;

        return true;
    }

    static bool acquire_and_aim(const float view[3], const Vector& eye, usercmd_t* cmd, float smooth)
    {
        if (menu_vars::aimbot_hitboxes == 0)
        {
            s_last_fail = 6;
            return false;
        }

        const float aim_fov = static_cast<float>(menu_vars::aimbot_fov);
        if (aim_fov <= 0.f)
        {
            s_last_fail = 4;
            return false;
        }

        Vector target{};
        if (!find_best_target(view, eye, aim_fov, menu_vars::aimbot_hitboxes, target, false))
        {
            s_last_fail = 5;
            return false;
        }

        return apply_aim_internal(cmd, view, eye, target, smooth);
    }
}

bool aimbot::get_view_eye(Vector& eye)
{
    auto* api = game::studio_api();
    if (!api || !api->GetViewInfo)
        return false;

    float org[3]{}, up[3]{}, right[3]{}, vpn[3]{};
    __try
    {
        api->GetViewInfo(org, up, right, vpn);
        eye = Vector(org[0], org[1], org[2]);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool aimbot::find_target(const float view[3], const Vector& eye, float fov_deg, uint32_t hitbox_mask, Vector& out_pos, bool require_line_of_sight)
{
    return find_best_target(view, eye, fov_deg, hitbox_mask, out_pos, require_line_of_sight);
}

void aimbot::compute_aim_angles(const Vector& eye, const Vector& target, float out[3])
{
    calc_angles(eye, target, out);
}

bool aimbot::apply_aim(usercmd_t* cmd, const float view[3], const Vector& eye, const Vector& target, float smooth)
{
    return apply_aim_internal(cmd, view, eye, target, smooth);
}

static void __cdecl detour_HUD_CalcRef(ref_params_t* params)
{
    norecoil::on_calc_ref_pre(params);

    if (s_orig_calc_ref)
        s_orig_calc_ref(params);

    aimbot::apply_ref_view(params);
    norecoil::on_calc_ref_post(params);
    thirdperson::on_calc_ref_post(params);
}

bool aimbot::init_hooks()
{
    if (s_calc_ref_hooked)
        return true;

    if (!movement::hooks_attached())
        return false;

    const uintptr_t table = game::cl_dll_funcs();
    if (!table)
        return false;

    auto* slot = reinterpret_cast<HUD_CalcRef_t*>(table + cldll_off::HUD_CalcRef);
    s_orig_calc_ref = *slot;
    if (!s_orig_calc_ref)
        return false;

    *slot = detour_HUD_CalcRef;
    s_cl_funcs_table = table;
    s_calc_ref_hooked = true;
    thirdperson::init_hooks();
    return true;
}

void aimbot::shutdown_hooks()
{
    if (!s_calc_ref_hooked || !s_cl_funcs_table)
        return;

    if (s_orig_calc_ref)
        *reinterpret_cast<HUD_CalcRef_t*>(s_cl_funcs_table + cldll_off::HUD_CalcRef) = s_orig_calc_ref;

    thirdperson::shutdown_hooks();

    s_orig_calc_ref = nullptr;
    s_cl_funcs_table = 0;
    s_calc_ref_hooked = false;
}

float aimbot::fov_radius_px()
{
    const Vector2D sz = world::screen_size();
    const float half_h = sz.y * 0.5f;
    const float game_fov = game::view_fov();
    const float aim_fov = static_cast<float>(menu_vars::aimbot_fov);

    if (half_h <= 0.f || game_fov <= 0.f || aim_fov <= 0.f)
        return 0.f;

    return half_h * tanf(aim_fov * k_pi / 360.f) / tanf(game_fov * k_pi / 360.f);
}

void aimbot::run(usercmd_t* cmd)
{
    s_last_fail = 0;

    if (rage_active())
    {
        s_last_fail = 7;
        return;
    }

    s_has_pending_angles = false;

    if (!cmd || !menu_vars::aimbot_enabled || !game::movement_allowed() || !game::in_match())
    {
        s_last_fail = 1;
        return;
    }

    if (rage_active())
    {
        s_last_fail = 7;
        return;
    }

    if (!legit_key_active())
    {
        s_last_fail = 2;
        return;
    }

    entities::refresh_player_info();

    float view[3] = { cmd->viewangles[0], cmd->viewangles[1], 0.f };

    Vector eye{};
    if (!get_view_eye(eye))
    {
        s_last_fail = 3;
        return;
    }

    acquire_and_aim(view, eye, cmd, menu_vars::aimbot_smooth);
}

bool aimbot::has_pending_angles()
{
    return s_has_pending_angles;
}

void aimbot::clear_pending_angles()
{
    s_has_pending_angles = false;
}

void aimbot::apply_ref_view(ref_params_t* params)
{
    if (auto* shm = g_shm())
        shm->aimbot_calc_ref_calls++;

    if (!params || !game::in_match())
        return;

    const bool legit_active = menu_vars::aimbot_enabled && legit_key_active();
    const bool rage_on      = rage_active();
    if (!legit_active && !rage_on)
        return;

    if (!s_has_pending_angles)
        return;

    set_view_angles(params->cmd, s_pending_angles);
    apply_client_view(params, s_pending_angles);

    if (auto* shm = g_shm())
        shm->aimbot_angles_applied++;
}

void aimbot::update_debug_status()
{
    auto* shm = g_shm();
    if (!shm)
        return;

    shm->aimbot_hook_ok = s_calc_ref_hooked ? 1 : 0;
    shm->aimbot_key_down = (legit_key_active() || rage_active()) ? 1 : 0;
    shm->aimbot_enabled_flag = menu_vars::aimbot_enabled ? 1 : 0;
    shm->aimbot_bound_key = menu_vars::aimbot_key & static_cast<int>(OSHGui::Key::KeyCode);
    shm->aimbot_last_fail = s_last_fail;
}

void aimbot::draw_fov_circle(OSHGui::Drawing::Graphics& gfx)
{
    if (!menu_vars::aimbot_enabled || !menu_vars::aimbot_fov_circle)
        return;

    const float radius = fov_radius_px();
    if (radius <= 1.f)
        return;

    const Vector2D sz = world::screen_size();
    const float cx = sz.x * 0.5f;
    const float cy = sz.y * 0.5f;

    const auto color = OSHGui::Drawing::Color::FromARGB(180, 210, 45, 45);
    constexpr int segments = 64;

    OSHGui::Drawing::PointF prev(cx + radius, cy);

    for (int i = 1; i <= segments; ++i)
    {
        const float t = (static_cast<float>(i) / static_cast<float>(segments)) * (2.f * k_pi);
        const OSHGui::Drawing::PointF cur(
            cx + cosf(t) * radius,
            cy + sinf(t) * radius);

        gfx.DrawLine(color, prev, cur);
        prev = cur;
    }
}
