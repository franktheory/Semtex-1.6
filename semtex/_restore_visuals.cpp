#include "visuals.h"

#include "../game/entities.h"
#include "../game/memory.h"
#include "../game/world.h"
#include "../game/pattern_defs.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../../detours/detours.h"
#include <GL/gl.h>

using CL_ProcessEntityUpdate_t = void(__cdecl*)(cl_entity_t*);
using R_GLStudioDrawPoints_t   = void(__cdecl*)();

static CL_ProcessEntityUpdate_t s_orig_entity_update = nullptr;
static R_GLStudioDrawPoints_t   s_orig_studio_draw   = nullptr;
static uint8_t*                 s_entity_update_addr = nullptr;
static uint8_t*                 s_studio_draw_addr   = nullptr;
static bool                     s_hooks_attached     = false;

bool visuals::hooks_attached() { return s_hooks_attached; }
bool visuals::has_studio_hook()  { return s_studio_draw_addr != nullptr; }
bool visuals::has_entity_hook()  { return s_entity_update_addr != nullptr; }

static const player_info_t* local_player_info()
{
    if (!game::ready())
        return nullptr;

    const int local_index = game::cl_playernum(game::cl());
    if (local_index < 0)
        return nullptr;

    if (auto* api = game::studio_api())
    {
        if (api->PlayerInfo)
            return api->PlayerInfo(local_index);
    }

    if (auto* cl = game::cl())
        return &game::cl_player(cl, local_index);

    return nullptr;
}

static int player_team(const player_cache_t& player)
{
    if (player.extra && player.extra->teamnumber > 0)
        return player.extra->teamnumber;

    if (player.info && player.info->topcolor > 0)
    {
        if (player.info->topcolor == 5 || player.info->bottomcolor == 5)
            return TEAM_TERRORIST;
        if (player.info->topcolor == 3 || player.info->bottomcolor == 3)
            return TEAM_CT;
    }

    if (player.ent && player.ent->curstate.team > 0)
        return player.ent->curstate.team;
    if (player.state && player.state->team > 0)
        return player.state->team;

    return 0;
}

static bool same_model_colors(const player_info_t* a, const player_info_t* b)
{
    if (!a || !b)
        return false;

    return a->topcolor > 0
        && a->bottomcolor > 0
        && a->topcolor == b->topcolor
        && a->bottomcolor == b->bottomcolor;
}

static bool is_teammate(const player_cache_t& player)
{
    if (!player.info)
        return false;

    const int local_team = local_team_number();
    const int team = player_team(player);
    if (local_team > 0 && team > 0)
        return local_team == team;

    if (const auto* local = local_player_info())
        return same_model_colors(local, player.info);

    return false;
}

static int local_team_number()
{
    if (!game::ready())
        return 0;

    const int local_index = game::cl_playernum(game::cl());
    if (local_index < 0)
        return 0;

    if (auto* extra = game_extra_info())
        return extra[local_index + 1].teamnumber;

    const auto it = entities::players().find(local_index + 1);
    if (it != entities::players().end())
        return player_team(it->second);

    if (auto* api = game::studio_api())
    {
        if (api->GetPlayerState)
        {
            if (entity_state_t* st = api->GetPlayerState(local_index))
                if (st->team > 0)
                    return st->team;
        }
    }

    return 0;
}

static bool is_teammate_entity(cl_entity_t* ent)
{
    if (!ent || !game::ready())
        return false;

    const auto it = entities::players().find(ent->index);
    if (it != entities::players().end())
        return is_teammate(it->second);

    const int local_team = local_team_number();
    const int team = ent->curstate.team;
    return local_team > 0 && team > 0 && local_team == team;
}

static bool get_player_origin_bounds(const player_cache_t& player, Vector& origin, Vector& mins, Vector& maxs)
{
    origin = (player.ent && !player.ent->origin.IsZero())
        ? player.ent->origin
        : (player.state ? player.state->origin : Vector{});
    if (origin.IsZero())
        return false;

    mins = player.ent ? player.ent->curstate.mins : (player.state ? player.state->mins : Vector{});
    maxs = player.ent ? player.ent->curstate.maxs : (player.state ? player.state->maxs : Vector{});
    if (mins.IsZero()) mins.z = -36.0f;
    if (maxs.IsZero()) maxs.z = 36.0f;
    return true;
}

struct box_metrics_t
{
    Vector2D top{};
    Vector2D bot{};
    float    wide_half = 0.f;
};

static bool get_player_box_metrics(const player_cache_t& player, box_metrics_t& box)
{
    Vector origin{}, mins{}, maxs{};
    if (!get_player_origin_bounds(player, origin, mins, maxs))
        return false;

    const Vector top3d = origin + Vector(0.0f, 0.0f, maxs.z);
    const Vector bot3d = origin + Vector(0.0f, 0.0f, mins.z);
    if (!world::to_screen(top3d, box.top) || !world::to_screen(bot3d, box.bot))
        return false;

    const float tall = box.bot.y - box.top.y;
    box.wide_half = tall * 0.35f;
    return true;
}

static OSHGui::Drawing::Color team_color(const player_cache_t& player)
{
    const int team = player_team(player);
    if (team == TEAM_TERRORIST) return OSHGui::Drawing::Color::FromRGB(230, 70, 70);
    if (team == TEAM_CT)        return OSHGui::Drawing::Color::FromRGB(70, 120, 230);
    return OSHGui::Drawing::Color::FromRGB(200, 200, 200);
}

static bool should_draw_player(const player_cache_t& player)
{
    if (!player.valid() || !player.alive() || player.is_local())
        return false;

    const Vector origin = player.ent ? player.ent->origin
                                     : (player.state ? player.state->origin : Vector{});
    if (origin.IsZero())
        return false;

    if (player.ent)
    {
        const float age = game::cl_time(game::cl()) - player.ent->curstate.msg_time;
        if (player.ent->curstate.msg_time == 0.0f || age > 1.0f)
            return false;
    }

    if (!menu_vars::teammates && is_teammate(player))
        return false;

    return true;
}

static bool should_draw_entity(cl_entity_t* ent)
{
    if (!ent || !ent->player || !game::ready())
        return false;

    if (ent->index == game::cl_playernum(game::cl()) + 1)
        return false;

    const auto it = entities::players().find(ent->index);
    if (it != entities::players().end())
        return should_draw_player(it->second);

    if (!menu_vars::teammates && is_teammate_entity(ent))
        return false;

    return true;
}

static void draw_skeleton_gl(cl_entity_t* ent)
{
    auto* api = game::studio_api();
    auto** phdr_ptr = game::pstudiohdr();
    if (!api || !phdr_ptr || !*phdr_ptr || !ent)
        return;

    auto* hdr = *phdr_ptr;
    auto* bones = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(hdr) + hdr->boneindex);
    auto* mats = reinterpret_cast<float(*)[MAXSTUDIOBONES][3][4]>(api->StudioGetBoneTransform());
    if (!bones || !mats || hdr->numbones <= 0)
        return;

    const auto it = entities::players().find(ent->index);
    const auto color = (it != entities::players().end())
        ? team_color(it->second)
        : OSHGui::Drawing::Color::FromRGB(180, 180, 180);

    const GLfloat r = color.GetRed()   / 255.f;
    const GLfloat g = color.GetGreen() / 255.f;
    const GLfloat b = color.GetBlue()  / 255.f;

    glDisable(GL_TEXTURE_2D);
    glLineWidth(1.5f);

    for (int i = 0; i < hdr->numbones; ++i)
    {
        if (bones[i].parent == -1)
            continue;

        const int parent = bones[i].parent;

        glColor3f(r, g, b);
        glBegin(GL_LINES);
        glVertex3f((*mats)[i][0][3], (*mats)[i][1][3], (*mats)[i][2][3]);
        glVertex3f((*mats)[parent][0][3], (*mats)[parent][1][3], (*mats)[parent][2][3]);
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
}

void __cdecl detour_CL_ProcessEntityUpdate(cl_entity_t* ent)
{
    if (s_orig_entity_update)
        s_orig_entity_update(ent);
    visuals::on_entity_update(ent);
}

void __cdecl detour_R_GLStudioDrawPoints()
{
    if (s_orig_studio_draw)
        s_orig_studio_draw();

    if (!menu_vars::visuals_enabled || !menu_vars::skeleton_esp || !game::ready())
        return;

    auto* api = game::studio_api();
    if (!api || !api->GetCurrentEntity)
        return;

    if (cl_entity_t* ent = api->GetCurrentEntity())
    {
        if (ent->player)
            entities::on_entity_update(ent);
    }

    if (cl_entity_t* ent = api->GetCurrentEntity())
    {
        if (should_draw_entity(ent))
            draw_skeleton_gl(ent);
    }
}

bool visuals::init_engine_hooks()
{
    if (!game::init())
        return false;

    if (s_hooks_attached)
        return true;

    s_entity_update_addr = pattern_scan_any(k_pat_entity_update);
    s_studio_draw_addr   = pattern_scan_any(k_pat_studio_draw);

    if (!s_entity_update_addr && !s_studio_draw_addr)
    {
        s_hooks_attached = true;
        return true;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (s_studio_draw_addr)
    {
        s_orig_studio_draw = reinterpret_cast<R_GLStudioDrawPoints_t>(s_studio_draw_addr);
        DetourAttach(reinterpret_cast<void**>(&s_orig_studio_draw), detour_R_GLStudioDrawPoints);
    }

    if (s_entity_update_addr)
    {
        s_orig_entity_update = reinterpret_cast<CL_ProcessEntityUpdate_t>(s_entity_update_addr);
        DetourAttach(reinterpret_cast<void**>(&s_orig_entity_update), detour_CL_ProcessEntityUpdate);
    }

    if (DetourTransactionCommit() != NO_ERROR)
        return false;

    s_hooks_attached = true;
    return true;
}

void visuals::shutdown_engine_hooks()
{
    if (s_orig_entity_update || s_orig_studio_draw)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        if (s_orig_entity_update)
            DetourDetach(reinterpret_cast<void**>(&s_orig_entity_update), detour_CL_ProcessEntityUpdate);
        if (s_orig_studio_draw)
            DetourDetach(reinterpret_cast<void**>(&s_orig_studio_draw), detour_R_GLStudioDrawPoints);
        DetourTransactionCommit();
        s_orig_entity_update = nullptr;
        s_orig_studio_draw = nullptr;
    }

    s_entity_update_addr = nullptr;
    s_studio_draw_addr = nullptr;
    s_hooks_attached = false;

    entities::clear();
    game::shutdown();
}

void visuals::on_entity_update(cl_entity_t* ent)
{
    entities::on_entity_update(ent);
}

void visuals::refresh_players()
{
    entities::refresh_player_info();
}

void visuals::cache_skeleton_bones() {}

void visuals::draw_skeleton_esp(OSHGui::Drawing::Graphics& gfx)
{
    (void)gfx;
}

void visuals::draw_box_esp(OSHGui::Drawing::Graphics& gfx)
{
    if (!menu_vars::box_esp || !game::ready())
        return;

    refresh_players();

    for (const auto& [index, player] : entities::players())
    {
        (void)index;
        if (!should_draw_player(player))
            continue;

        box_metrics_t box{};
        if (!get_player_box_metrics(player, box))
            continue;

        const auto color = team_color(player);
        const float left   = box.top.x - box.wide_half;
        const float right  = box.top.x + box.wide_half;
        const float top    = box.top.y;
        const float bottom = box.bot.y;

        gfx.DrawLine(color, OSHGui::Drawing::PointF(left, top),    OSHGui::Drawing::PointF(right, top));
        gfx.DrawLine(color, OSHGui::Drawing::PointF(right, top),   OSHGui::Drawing::PointF(right, bottom));
        gfx.DrawLine(color, OSHGui::Drawing::PointF(right, bottom), OSHGui::Drawing::PointF(left, bottom));
        gfx.DrawLine(color, OSHGui::Drawing::PointF(left, bottom), OSHGui::Drawing::PointF(left, top));
    }
}

void visuals::draw_name_esp(OSHGui::Drawing::Graphics& gfx, const OSHGui::Drawing::FontPtr& font)
{
    if (!menu_vars::name_esp || !game::ready())
        return;

    refresh_players();

    for (const auto& [index, player] : entities::players())
    {
        (void)index;
        if (!should_draw_player(player))
            continue;

        box_metrics_t box{};
        if (!get_player_box_metrics(player, box))
            continue;

        const char* name = player.info->name;
        const float text_w = font->GetTextExtent(name ? name : "?");
        const float label_x = box.bot.x - text_w * 0.5f;
        const float label_y = box.bot.y + 2.0f;
        const auto color = team_color(player);
        gfx.DrawString(name ? name : "?", font, color, label_x, label_y);
    }
}
