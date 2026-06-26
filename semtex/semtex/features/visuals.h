#pragma once

#include "../sdk/sdk.h"
#include "../../oshgui/Drawing/Graphics.hpp"
#include "../menu/menu_vars.h"

struct semtex_shared_t;

namespace visuals
{
    bool init_engine_hooks();
    void shutdown_engine_hooks();

    bool hooks_attached();
    bool has_studio_hook();
    bool has_studio_draw_player_hook();
    bool has_studio_renderer_hook();
    bool has_studio_remap_hook();
    bool has_entity_hook();

    void ensure_studio_hooks();

    bool studio_hooks_pending();

    void on_entity_update(cl_entity_t* ent);
    void refresh_players();

    void cache_skeleton_bones();
    void draw_world_esp_gl();
    void finish_frame_esp();
    void draw_skeleton_esp(OSHGui::Drawing::Graphics& gfx);
    void draw_box_esp(OSHGui::Drawing::Graphics& gfx);
    void draw_name_esp(OSHGui::Drawing::Graphics& gfx, const OSHGui::Drawing::FontPtr& font);
    void draw_weapon_esp(OSHGui::Drawing::Graphics& gfx, const OSHGui::Drawing::FontPtr& font);
    void update_debug_status(semtex_shared_t* shm);

    bool get_player_bone_pos(int entity_index, int bone_index, Vector& out);
    bool get_player_head_pos(int entity_index, Vector& out);
}
