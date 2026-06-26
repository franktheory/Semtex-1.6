#pragma once
#include <Windows.h>
#include <cstdint>

#define SEMTEX_SHARED_NAME "semtex_debug_shm"
#define SEMTEX_SHM_MAGIC   0x58544553u
#define SEMTEX_SHM_VERSION 13u

#pragma pack(push, 1)
struct semtex_shared_t
{
    uint32_t magic;
    uint32_t version;
    int32_t  hook_fired;
    int32_t  swap_call_count;
    int32_t  renderer_ok;
    int32_t  menu_ok;
    int32_t  input_ok;
    int32_t  oshgui_init;
    int32_t  menu_visible;
    int32_t  hwnd;
    int32_t  init_step;
    int32_t  game_ok;
    int32_t  visuals_ok;
    int32_t  pat_cl;
    int32_t  pat_studio_api;
    int32_t  pat_client_studio;
    int32_t  pat_pstudiohdr;
    int32_t  pat_extra_info;
    int32_t  pat_studio_draw;
    int32_t  pat_studio_draw_player;

    int32_t  local_slot;
    int32_t  local_team;
    int32_t  local_extra_team;
    int32_t  show_teammates;
    int32_t  esp_cached;
    int32_t  esp_draw_enemies;
    int32_t  esp_draw_teammates;
    int32_t  weapon_cached;

    int32_t  movement_hook_ok;
    int32_t  createmove_count;
    uint32_t cl_funcs_addr;
    uint32_t createmove_fn_addr;
    uint32_t pmove_addr;
    int32_t  bhop_pm_flags;
    int32_t  bhop_ground;
    int32_t  bhop_active_hits;

    int32_t  aimbot_hook_ok;
    int32_t  aimbot_enabled_flag;
    int32_t  aimbot_key_down;
    int32_t  aimbot_bound_key;
    int32_t  aimbot_last_fail;
    int32_t  aimbot_target_found;
    int32_t  aimbot_calc_ref_calls;
    int32_t  aimbot_angles_applied;
    int32_t  aimbot_scan_players;
    int32_t  aimbot_head_ok;

    int32_t  recoil_punch_valid;
    int32_t  recoil_apply_count;

    int32_t  postruncmd_hook_ok;
    int32_t  recoil_cmd_match;
    int32_t  recoil_punch_pitch;
    int32_t  recoil_punch_yaw;
    int32_t  postruncmd_vtable_index;
    char     team_snapshot[192];
    char     last_error[256];
};
#pragma pack(pop)

static_assert(sizeof(semtex_shared_t) == 4 + 4 + 51 * 4 + 192 + 256, "unexpected semtex_shared_t size");
