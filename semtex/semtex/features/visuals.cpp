#include "visuals.h"

#include "../game/entities.h"
#include "../game/memory.h"
#include "../game/world.h"
#include "../game/pattern_defs.h"
#include "../../shared/shared.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../../detours/detours.h"
#include <GL/gl.h>
#include <unordered_set>
#include <cstring>

struct player_bones_t
{
    Vector  positions[MAXSTUDIOBONES]{};
    int     num_bones = 0;
    bool    has_data  = false;
};

struct player_esp_cache_t
{
    Vector origin{};
    Vector mins{};
    Vector maxs{};
    char   name[MAX_SCOREBOARDNAME]{};
    char   weapon[32]{};
    byte   color_r = 200;
    byte   color_g = 200;
    byte   color_b = 200;
    bool   has_data = false;
};

struct model_name_view_t
{
    char name[64];
};

static player_bones_t     s_player_bones[MAX_CLIENTS + 1];
static player_esp_cache_t s_player_esp[MAX_CLIENTS + 1];

using CL_ProcessEntityUpdate_t = void(__cdecl*)(cl_entity_t*);
using R_GLStudioDrawPoints_t   = void(__cdecl*)();
using R_StudioDrawPlayer_t     = int(__cdecl*)(int, entity_state_t*);
using StudioSetRemapColors_t   = void(__cdecl*)(int, int);
using StudioDrawPlayerIface_t  = int(__cdecl*)(int, entity_state_t*);

static CL_ProcessEntityUpdate_t s_orig_entity_update       = nullptr;
static R_GLStudioDrawPoints_t   s_orig_studio_draw         = nullptr;
static R_StudioDrawPlayer_t     s_orig_studio_draw_player  = nullptr;
static StudioSetRemapColors_t   s_orig_studio_remap        = nullptr;
static StudioDrawPlayerIface_t  s_orig_iface_draw_player   = nullptr;
static uint8_t*                 s_entity_update_addr       = nullptr;
static uint8_t*                 s_studio_draw_addr         = nullptr;
static uint8_t*                 s_studio_draw_player_addr   = nullptr;
static r_studio_interface_t*    s_pstudio_iface            = nullptr;
static bool                     s_remap_hooked             = false;
static bool                     s_iface_hooked               = false;
static bool                     s_renderer_hooked            = false;
static bool                     s_hooks_attached           = false;
static bool                     s_hw_gl_lookup_done        = false;
static bool                     s_pstudio_iface_lookup_done = false;

enum class chams_pass_t { none, xqz, visible };
static chams_pass_t             s_chams_pass               = chams_pass_t::none;

using StudioDrawPoints_t     = void(__cdecl*)();
using StudioSetupLighting_t  = void(__cdecl*)(alight_t*);
using StudioRenderModelFn    = void(__fastcall*)(void* self, void* edx);
using StudioRenderFinalFn    = void(__fastcall*)(void* self, void* edx);

using glColor4f_t      = void(APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat);
using glColor3f_t      = void(APIENTRY*)(GLfloat, GLfloat, GLfloat);
using glColor4ub_t     = void(APIENTRY*)(GLubyte, GLubyte, GLubyte, GLubyte);
using glBindTexture_t  = void(APIENTRY*)(GLenum, GLuint);
using glDrawElements_t = void(APIENTRY*)(GLenum, GLsizei, GLenum, const void*);
using glDrawArrays_t   = void(APIENTRY*)(GLenum, GLint, GLsizei);

static StudioDrawPoints_t       s_orig_api_draw_points     = nullptr;
static StudioSetupLighting_t    s_orig_studio_lighting     = nullptr;
static StudioRenderModelFn      s_orig_studio_render_model = nullptr;
static StudioRenderFinalFn      s_orig_studio_render_final = nullptr;
static studio_model_renderer_t* s_renderer_vtable          = nullptr;
static glColor4f_t              s_orig_glColor4f           = nullptr;
static glColor3f_t              s_orig_glColor3f           = nullptr;
static glColor4ub_t             s_orig_glColor4ub          = nullptr;
static glBindTexture_t          s_orig_glBindTexture       = nullptr;
static bool                     s_glcolor_hooked           = false;
static glDrawElements_t         s_orig_glDrawElements      = nullptr;
static glDrawArrays_t           s_orig_glDrawArrays        = nullptr;

static bool                     s_chams_active             = false;
static float                    s_chams_r                  = 1.f;
static float                    s_chams_g                  = 1.f;
static float                    s_chams_b                  = 1.f;


static glColor4f_t     s_gl_color4f = nullptr;
static glBindTexture_t s_gl_bindtex = nullptr;

static bool                     s_hw_gl_hooked             = false;
static void**                   s_hw_glcolor4f_slot        = nullptr;
static void**                   s_hw_glcolor3f_slot        = nullptr;
static glColor4f_t              s_orig_hw_glColor4f        = nullptr;
static glColor3f_t              s_orig_hw_glColor3f        = nullptr;

static void ensure_direct_gl_ptrs()
{
    if (s_gl_color4f) return;
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) return;
    s_gl_color4f = reinterpret_cast<glColor4f_t>(GetProcAddress(gl, "glColor4f"));
    s_gl_bindtex = reinterpret_cast<glBindTexture_t>(GetProcAddress(gl, "glBindTexture"));
}

void APIENTRY detour_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (s_orig_glDrawElements)
        s_orig_glDrawElements(mode, count, type, indices);
}

void APIENTRY detour_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (s_orig_glDrawArrays)
        s_orig_glDrawArrays(mode, first, count);
}

static void apply_chams_glcolor(GLfloat& r, GLfloat& g, GLfloat& b);
static bool replace_fn_ptr(void** slot, void* replacement, void** saved);

void APIENTRY detour_hw_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    apply_chams_glcolor(red, green, blue);
    if (s_orig_hw_glColor4f)
        s_orig_hw_glColor4f(red, green, blue, alpha);
}

void APIENTRY detour_hw_glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
    apply_chams_glcolor(red, green, blue);
    if (s_orig_hw_glColor3f)
        s_orig_hw_glColor3f(red, green, blue);
}

static bool is_executable_ptr(const void* ptr)
{
    if (!ptr)
        return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
        return false;

    const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ
        | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.State == MEM_COMMIT) && ((mbi.Protect & exec) != 0);
}

static bool hook_hw_gl_slot(void** slot, void* hook, void** saved)
{
    if (!slot)
        return false;

    __try
    {
        void* proc = *slot;
        if (!proc || !is_executable_ptr(proc))
            return false;

        if (!*saved)
            *saved = proc;

        return replace_fn_ptr(slot, hook, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool hook_hw_gl_dispatch()
{
    if (s_hw_gl_hooked)
        return true;

    if (s_hw_gl_lookup_done)
        return false;

    s_hw_gl_lookup_done = true;

    uint8_t* site4f = pattern_scan_any(k_pat_hw_glcolor4f);
    if (!site4f)
        return false;

    s_hw_glcolor4f_slot = reinterpret_cast<void**>(*reinterpret_cast<uint32_t*>(site4f));
    if (!hook_hw_gl_slot(s_hw_glcolor4f_slot,
            reinterpret_cast<void*>(detour_hw_glColor4f),
            reinterpret_cast<void**>(&s_orig_hw_glColor4f)))
        return false;

    s_hw_gl_hooked = true;
    return true;
}

static void unhook_hw_gl_dispatch()
{
    if (!s_hw_gl_hooked)
        return;

    if (s_hw_glcolor4f_slot && s_orig_hw_glColor4f)
    {
        replace_fn_ptr(s_hw_glcolor4f_slot,
            reinterpret_cast<void*>(s_orig_hw_glColor4f), nullptr);
    }
    if (s_hw_glcolor3f_slot && s_orig_hw_glColor3f)
    {
        replace_fn_ptr(s_hw_glcolor3f_slot,
            reinterpret_cast<void*>(s_orig_hw_glColor3f), nullptr);
    }

    s_hw_glcolor4f_slot = nullptr;
    s_hw_glcolor3f_slot = nullptr;
    s_orig_hw_glColor4f = nullptr;
    s_orig_hw_glColor3f = nullptr;
    s_hw_gl_hooked      = false;
    s_hw_gl_lookup_done = false;
}

static engine_studio_api_t* active_studio_api();
static bool should_cache_bones()
{
    return menu_vars::aimbot_enabled
        || (menu_vars::visuals_enabled && menu_vars::skeleton_esp);
}

static bool passes_team_filter_for_slot(int slot, const player_info_t* pinfo);

static void chams_set_color(bool xqz)
{
    const float* col = xqz ? menu_vars::chams_xqz_color : menu_vars::chams_vis_color;
    s_chams_r = col[1] / 255.f;
    s_chams_g = col[2] / 255.f;
    s_chams_b = col[3] / 255.f;
}

static void apply_chams_glcolor(GLfloat& r, GLfloat& g, GLfloat& b)
{
    if (s_chams_active && s_chams_pass != chams_pass_t::none)
    {
        r = s_chams_r;
        g = s_chams_g;
        b = s_chams_b;
    }
}

void APIENTRY detour_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    apply_chams_glcolor(red, green, blue);
    if (s_orig_glColor4f)
        s_orig_glColor4f(red, green, blue, alpha);
}

void APIENTRY detour_glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
    apply_chams_glcolor(red, green, blue);
    if (s_orig_glColor3f)
        s_orig_glColor3f(red, green, blue);
}

void APIENTRY detour_glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    if (s_chams_active)
    {
        red   = static_cast<GLubyte>(s_chams_r * 255.f);
        green = static_cast<GLubyte>(s_chams_g * 255.f);
        blue  = static_cast<GLubyte>(s_chams_b * 255.f);
    }
    if (s_orig_glColor4ub)
        s_orig_glColor4ub(red, green, blue, alpha);
}

void APIENTRY detour_glBindTexture(GLenum target, GLuint texture)
{
    if (s_chams_active && target == GL_TEXTURE_2D)
        texture = 0;

    if (s_orig_glBindTexture)
        s_orig_glBindTexture(target, texture);
}

static bool hook_gl_color()
{
    if (s_glcolor_hooked)
        return true;

    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl)
        return false;

    s_orig_glColor4f     = reinterpret_cast<glColor4f_t>(GetProcAddress(gl, "glColor4f"));
    s_orig_glColor3f     = reinterpret_cast<glColor3f_t>(GetProcAddress(gl, "glColor3f"));
    s_orig_glColor4ub    = reinterpret_cast<glColor4ub_t>(GetProcAddress(gl, "glColor4ub"));
    s_orig_glBindTexture = reinterpret_cast<glBindTexture_t>(GetProcAddress(gl, "glBindTexture"));
    if (!s_orig_glColor4f)
        return false;


    s_gl_color4f = s_orig_glColor4f;
    s_gl_bindtex = s_orig_glBindTexture;

    s_orig_glDrawElements = reinterpret_cast<glDrawElements_t>(GetProcAddress(gl, "glDrawElements"));
    s_orig_glDrawArrays   = reinterpret_cast<glDrawArrays_t>(GetProcAddress(gl, "glDrawArrays"));

    if (DetourAttach(reinterpret_cast<void**>(&s_orig_glColor4f), detour_glColor4f) != NO_ERROR)
        return false;
    if (s_orig_glColor3f)
        DetourAttach(reinterpret_cast<void**>(&s_orig_glColor3f), detour_glColor3f);
    if (s_orig_glColor4ub)
        DetourAttach(reinterpret_cast<void**>(&s_orig_glColor4ub), detour_glColor4ub);
    if (s_orig_glBindTexture)
        DetourAttach(reinterpret_cast<void**>(&s_orig_glBindTexture), detour_glBindTexture);
    if (s_orig_glDrawElements)
        DetourAttach(reinterpret_cast<void**>(&s_orig_glDrawElements), detour_glDrawElements);
    if (s_orig_glDrawArrays)
        DetourAttach(reinterpret_cast<void**>(&s_orig_glDrawArrays), detour_glDrawArrays);

    s_glcolor_hooked = true;
    return true;
}

static void unhook_gl_color()
{
    if (!s_glcolor_hooked)
        return;

    if (s_orig_glColor4f)
        DetourDetach(reinterpret_cast<void**>(&s_orig_glColor4f), detour_glColor4f);
    if (s_orig_glColor3f)
        DetourDetach(reinterpret_cast<void**>(&s_orig_glColor3f), detour_glColor3f);
    if (s_orig_glColor4ub)
        DetourDetach(reinterpret_cast<void**>(&s_orig_glColor4ub), detour_glColor4ub);
    if (s_orig_glBindTexture)
        DetourDetach(reinterpret_cast<void**>(&s_orig_glBindTexture), detour_glBindTexture);
    if (s_orig_glDrawElements)
        DetourDetach(reinterpret_cast<void**>(&s_orig_glDrawElements), detour_glDrawElements);
    if (s_orig_glDrawArrays)
        DetourDetach(reinterpret_cast<void**>(&s_orig_glDrawArrays), detour_glDrawArrays);

    s_orig_glColor4f      = nullptr;
    s_orig_glColor3f      = nullptr;
    s_orig_glColor4ub     = nullptr;
    s_orig_glBindTexture  = nullptr;
    s_orig_glDrawElements = nullptr;
    s_orig_glDrawArrays   = nullptr;
    s_glcolor_hooked      = false;
}

static bool entity_wants_chams(cl_entity_t* ent)
{
    if (!ent || !ent->player || !game::in_match())
        return false;

    const int idx = ent->index;
    if (idx <= 0 || idx > MAX_CLIENTS)
        return false;

    if (idx == game::local_slot() + 1)
        return false;

    player_info_t* pinfo = nullptr;
    if (auto* api = active_studio_api(); api && api->PlayerInfo)
    {
        __try { pinfo = api->PlayerInfo(idx - 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { pinfo = nullptr; }
    }

    return passes_team_filter_for_slot(idx - 1, pinfo);
}

static bool player_wants_chams(entity_state_t* pplayer)
{
    if (!pplayer || !game::in_match())
        return false;

    const int idx = pplayer->number;
    if (idx <= 0 || idx > MAX_CLIENTS)
        return false;

    if (idx == game::local_slot() + 1)
        return false;

    player_info_t* pinfo = nullptr;
    if (auto* api = active_studio_api(); api && api->PlayerInfo)
    {
        __try { pinfo = api->PlayerInfo(idx - 1); }
        __except (EXCEPTION_EXECUTE_HANDLER) { pinfo = nullptr; }
    }

    return passes_team_filter_for_slot(idx - 1, pinfo);
}

static void studio_draw_points_chams_begin()
{
    if (s_chams_pass == chams_pass_t::none)
        return;

    chams_set_color(s_chams_pass == chams_pass_t::xqz);
    s_chams_active = true;

    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glShadeModel(GL_FLAT);
}

static void studio_draw_points_chams_end()
{
    if (s_chams_pass == chams_pass_t::none)
        return;

    s_chams_active = false;
    glShadeModel(GL_SMOOTH);
    glEnable(GL_TEXTURE_2D);
}

static int draw_player_with_chams(int flags, entity_state_t* pplayer,
    int (*draw_fn)(int, entity_state_t*))
{
    if (!draw_fn)
        return 0;

    if (!menu_vars::visuals_enabled || !menu_vars::chams || !player_wants_chams(pplayer))
        return draw_fn(flags, pplayer);


    if (s_renderer_hooked)
        return draw_fn(flags, pplayer);

    if (menu_vars::chams_xqz)
    {
        glDepthFunc(GL_GREATER);
        glDisable(GL_DEPTH_TEST);
        s_chams_pass = chams_pass_t::xqz;
        draw_fn(flags, pplayer);
        s_chams_pass = chams_pass_t::none;
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    s_chams_pass = chams_pass_t::visible;
    const int ret = draw_fn(flags, pplayer);
    s_chams_pass = chams_pass_t::none;
    return ret;
}

bool visuals::hooks_attached() { return s_hooks_attached; }
bool visuals::has_studio_hook()  { return s_studio_draw_addr != nullptr; }
bool visuals::has_studio_draw_player_hook() { return s_studio_draw_player_addr != nullptr || s_iface_hooked; }
bool visuals::has_studio_renderer_hook() { return s_renderer_hooked; }
bool visuals::has_studio_remap_hook() { return s_remap_hooked; }

bool visuals::studio_hooks_pending()
{
    if (!GetModuleHandleA("client.dll"))
        return false;

    if (!s_remap_hooked && game::client_studio_api())
        return true;

    if (!s_iface_hooked && game::client_studio_iface())
        return true;

    if (s_hooks_attached && !s_hw_gl_hooked && !s_hw_gl_lookup_done)
        return true;

    return false;
}
bool visuals::has_entity_hook()  { return s_entity_update_addr != nullptr; }

static bool replace_fn_ptr(void** slot, void* replacement, void** saved)
{
    if (!slot || !replacement)
        return false;

    DWORD old_protect = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect))
        return false;

    if (saved)
        *saved = *slot;
    *slot = replacement;

    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), old_protect, &ignored);
    return true;
}

static int get_player_team(const player_cache_t& player)
{
    if (player.extra && player.extra->teamnumber > 0)
        return player.extra->teamnumber;
    if (player.ent && player.ent->curstate.team > 0)
        return player.ent->curstate.team;
    if (player.state && player.state->team > 0)
        return player.state->team;
    return TEAM_UNASSIGNED;
}

static int team_for_slot(int slot)
{
    if (slot < 0)
        return TEAM_UNASSIGNED;

    if (const auto it = entities::players().find(slot + 1); it != entities::players().end())
    {
        const int team = get_player_team(it->second);
        if (team == TEAM_TERRORIST || team == TEAM_CT)
            return team;
    }

    if (auto* extra = game_extra_info())
    {
        const int team = static_cast<int>(extra[slot + 1].teamnumber);
        if (team == TEAM_TERRORIST || team == TEAM_CT)
            return team;
    }

    if (cl_entity_t* ent = game::cl_entity(slot + 1))
    {
        const int team = ent->curstate.team;
        if (team == TEAM_TERRORIST || team == TEAM_CT)
            return team;
    }

    return TEAM_UNASSIGNED;
}

static int local_team_number()
{
    if (!game::in_match())
        return TEAM_UNASSIGNED;

    const int idx = game::local_slot();
    if (idx < 0)
        return TEAM_UNASSIGNED;

    const int from_tab = team_for_slot(idx);
    if (from_tab != TEAM_UNASSIGNED)
        return from_tab;

    if (const auto it = entities::players().find(idx + 1); it != entities::players().end())
        return get_player_team(it->second);

    return TEAM_UNASSIGNED;
}

static bool is_teammate_player(const player_cache_t& player)
{
    const int lt = local_team_number();
    const int pt = get_player_team(player);
    if (lt == TEAM_UNASSIGNED || pt == TEAM_UNASSIGNED)
        return false;
    return lt == pt;
}

static bool is_teammate_for_slot(int slot)
{
    const int lt = local_team_number();
    const int pt = team_for_slot(slot);
    if (lt == TEAM_UNASSIGNED || pt == TEAM_UNASSIGNED)
        return false;
    return lt == pt;
}

static void team_color_bytes(int team, byte& r, byte& g, byte& b)
{
    if (team == TEAM_TERRORIST)      { r = 230; g = 70;  b = 70;  }
    else if (team == TEAM_CT)        { r = 70;  g = 120; b = 230; }
    else                             { r = 200; g = 200; b = 200; }
}

static bool passes_team_filter(const player_cache_t& player)
{
    if (menu_vars::teammates)
        return true;
    return !is_teammate_player(player);
}

static bool passes_team_filter_for_slot(int slot, const player_info_t*)
{
    if (menu_vars::teammates)
        return true;
    return !is_teammate_for_slot(slot);
}

static bool get_player_origin_bounds(const player_cache_t& player, Vector& origin, Vector& mins, Vector& maxs)
{
    if (player.info_index < 0)
        return false;

    const int entity_num = player.info_index + 1;

    if (player.entity_matches_slot() && !player.ent->origin.IsZero())
    {
        origin = player.ent->origin;
        mins   = player.ent->curstate.mins;
        maxs   = player.ent->curstate.maxs;
    }
    else if (player.state && player.state->number == entity_num && !player.state->origin.IsZero())
    {
        origin = player.state->origin;
        mins   = player.state->mins;
        maxs   = player.state->maxs;
    }
    else
    {
        return false;
    }

    if (mins.IsZero()) mins.z = -36.0f;
    if (maxs.IsZero()) maxs.z = 36.0f;
    return true;
}

static int origin_label_key(const Vector& origin)
{
    return (static_cast<int>(origin.x * 4.0f) << 20)
         ^ (static_cast<int>(origin.y * 4.0f) << 10)
         ^  static_cast<int>(origin.z * 4.0f);
}

static const char* player_display_name(const player_cache_t& player)
{
    if (player.info_index < 0)
        return nullptr;

    if (auto* api = game::studio_api(); api && api->PlayerInfo)
    {
        __try
        {
            if (player_info_t* p = api->PlayerInfo(player.info_index))
            {
                if (p->name[0])
                    return p->name;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (player.info && player.info->name[0])
        return player.info->name;

    return nullptr;
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
    const int team = get_player_team(player);
    if (team == TEAM_TERRORIST) return OSHGui::Drawing::Color::FromRGB(230, 70, 70);
    if (team == TEAM_CT)        return OSHGui::Drawing::Color::FromRGB(70, 120, 230);
    return OSHGui::Drawing::Color::FromRGB(200, 200, 200);
}

static bool should_draw_player(const player_cache_t& player)
{
    if (!player.valid() || !player.alive() || player.is_local())
        return false;

    Vector origin{}, mins{}, maxs{};
    if (!get_player_origin_bounds(player, origin, mins, maxs))
        return false;

    if (visuals::has_entity_hook() && player.ent && player.entity_matches_slot())
    {
        const float age = static_cast<float>(game::cl_time(game::cl())) - player.ent->curstate.msg_time;
        if (player.ent->curstate.msg_time == 0.0f || age > 1.0f)
            return false;
    }

    if (!passes_team_filter(player))
        return false;

    return true;
}

static studiohdr_t* get_studio_header(cl_entity_t* ent, engine_studio_api_t* api)
{
    if (auto** phdr = game::pstudiohdr(); phdr && *phdr)
        return *phdr;

    if (!ent || !api || !api->SetupPlayerModel || !api->Mod_Extradata)
        return nullptr;

    __try
    {
        model_t* mod = api->SetupPlayerModel(ent->index);
        if (!mod)
            return nullptr;

        auto* hdr = reinterpret_cast<studiohdr_t*>(api->Mod_Extradata(mod));
        if (hdr && hdr->numbones > 0 && hdr->numbones <= MAXSTUDIOBONES)
            return hdr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return nullptr;
}

static void cache_player_bones(cl_entity_t* ent, engine_studio_api_t* api, studiohdr_t* hdr)
{
    if (!ent || !api || !hdr || !api->StudioGetBoneTransform)
        return;

    if (ent->index <= 0 || ent->index > MAX_CLIENTS)
        return;

    auto* mats = reinterpret_cast<float(*)[MAXSTUDIOBONES][3][4]>(api->StudioGetBoneTransform());
    if (!mats)
        return;

    const int count = hdr->numbones < MAXSTUDIOBONES ? hdr->numbones : MAXSTUDIOBONES;
    if (count <= 0)
        return;

    auto& cache = s_player_bones[ent->index];
    for (int i = 0; i < count; ++i)
    {
        cache.positions[i] = Vector(
            (*mats)[i][0][3],
            (*mats)[i][1][3],
            (*mats)[i][2][3]);
    }
    cache.num_bones  = count;
    cache.has_data   = true;
}

static engine_studio_api_t* active_studio_api()
{
    if (auto* api = game::client_studio_api())
        return api;
    return game::studio_api();
}

static int weaponmodel_for_entity(cl_entity_t* ent, engine_studio_api_t* api, int slot)
{
    if (!ent)
        return 0;

    int weaponmodel = ent->curstate.weaponmodel;
    if (weaponmodel)
        return weaponmodel;

    if (ent->prevstate.weaponmodel)
        return ent->prevstate.weaponmodel;

    if (api && api->GetPlayerState && slot >= 0)
    {
        __try
        {
            if (entity_state_t* st = api->GetPlayerState(slot))
            {
                if (st->weaponmodel)
                    return st->weaponmodel;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    return 0;
}

static void resolve_weapon_name(int weaponmodel, engine_studio_api_t* api, char* out, size_t out_size)
{
    if (!out || out_size == 0)
        return;

    out[0] = '\0';
    if (!weaponmodel || !api || !api->GetModelByIndex)
        return;

    model_t* mdl = nullptr;
    __try
    {
        mdl = api->GetModelByIndex(weaponmodel);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }

    if (!mdl)
        return;

    const char* model_name = reinterpret_cast<model_name_view_t*>(mdl)->name;
    if (!model_name[0])
        return;

    const char* weapon = strstr(model_name, "/p_");
    if (!weapon)
        weapon = strstr(model_name, "\\p_");
    if (!weapon)
        return;

    weapon += 3;
    strncpy_s(out, out_size, weapon, _TRUNCATE);

    char* ext = strstr(out, ".mdl");
    if (ext)
        *ext = '\0';
}

static bool project_esp_box(const player_esp_cache_t& esp, box_metrics_t& box)
{
    if (!esp.has_data)
        return false;

    const Vector top3d = esp.origin + Vector(0.0f, 0.0f, esp.maxs.z);
    const Vector bot3d = esp.origin + Vector(0.0f, 0.0f, esp.mins.z);
    if (!world::to_screen(top3d, box.top) || !world::to_screen(bot3d, box.bot))
        return false;

    const float tall = box.bot.y - box.top.y;
    box.wide_half = tall * 0.35f;
    return box.wide_half > 0.5f;
}

static void cache_player_visuals_for_entity(cl_entity_t* ent)
{
    if (!ent || !ent->player)
        return;

    if (ent->index <= 0 || ent->index > MAX_CLIENTS)
        return;

    if (ent->index == game::local_slot() + 1)
        return;

    if (ent->curstate.iuser1 != 0)
        return;

    auto* api = active_studio_api();
    if (!api)
        return;

    const int slot = ent->index - 1;
    player_info_t* pinfo = nullptr;
    if (api->PlayerInfo)
    {
        __try
        {
            pinfo = api->PlayerInfo(slot);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            pinfo = nullptr;
        }
    }

    if (!passes_team_filter_for_slot(slot, pinfo))
        return;

    auto& esp = s_player_esp[ent->index];
    esp.origin = ent->origin;
    esp.mins   = ent->curstate.mins;
    esp.maxs   = ent->curstate.maxs;
    if (esp.mins.IsZero()) esp.mins.z = -36.0f;
    if (esp.maxs.IsZero()) esp.maxs.z = 36.0f;

    if (pinfo && pinfo->name[0])
        strncpy_s(esp.name, pinfo->name, _TRUNCATE);
    else
        esp.name[0] = '\0';

    resolve_weapon_name(weaponmodel_for_entity(ent, api, slot), api, esp.weapon, sizeof(esp.weapon));

    const int team = team_for_slot(slot);
    team_color_bytes(team, esp.color_r, esp.color_g, esp.color_b);
    esp.has_data = true;

    if (should_cache_bones())
    {
        if (auto* hdr = get_studio_header(ent, api))
            cache_player_bones(ent, api, hdr);
    }
}

static bool should_cache_entity_skeleton(cl_entity_t* ent)
{
    if (!ent || !ent->player || !game::in_match())
        return false;

    if (ent->index <= 0 || ent->index > MAX_CLIENTS)
        return false;

    if (ent->index == game::local_slot() + 1)
        return false;

    if (ent->curstate.iuser1 != 0)
        return false;

    return true;
}

static void cache_player_bones_for_entity(cl_entity_t* ent)
{
    if (!should_cache_entity_skeleton(ent))
        return;

    auto* api = active_studio_api();
    if (!api)
        return;

    const int slot = ent->index - 1;
    player_info_t* pinfo = nullptr;
    if (api->PlayerInfo)
    {
        __try
        {
            pinfo = api->PlayerInfo(slot);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            pinfo = nullptr;
        }
    }

    if (!passes_team_filter_for_slot(slot, pinfo))
        return;

    if (auto* hdr = get_studio_header(ent, api))
        cache_player_bones(ent, api, hdr);
}

static void draw_gl_line(int x1, int y1, int x2, int y2, byte r, byte g, byte b)
{
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(r, g, b, 255);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
}

static void draw_player_box_gl(int player_index)
{
    const auto& esp = s_player_esp[player_index];
    if (!esp.has_data)
        return;

    box_metrics_t box{};
    if (!project_esp_box(esp, box))
        return;

    const int left   = static_cast<int>(box.top.x - box.wide_half);
    const int right  = static_cast<int>(box.top.x + box.wide_half);
    const int top    = static_cast<int>(box.top.y);
    const int bottom = static_cast<int>(box.bot.y);

    draw_gl_line(left,  top,    right, top,    esp.color_r, esp.color_g, esp.color_b);
    draw_gl_line(right, top,    right, bottom, esp.color_r, esp.color_g, esp.color_b);
    draw_gl_line(right, bottom, left,  bottom, esp.color_r, esp.color_g, esp.color_b);
    draw_gl_line(left,  bottom, left,  top,    esp.color_r, esp.color_g, esp.color_b);
}

static void draw_bone_pair_gl(int player_index, int bone1, int bone2, byte r, byte g, byte b)
{
    if (player_index <= 0 || player_index > MAX_CLIENTS)
        return;

    const auto& bones = s_player_bones[player_index];
    if (!bones.has_data)
        return;

    if (bone1 < 0 || bone2 < 0 || bone1 >= MAXSTUDIOBONES || bone2 >= MAXSTUDIOBONES)
        return;


    const Vector& p1 = bones.positions[bone1];
    const Vector& p2 = bones.positions[bone2];
    if (p1.x == 0.f && p1.y == 0.f && p1.z == 0.f)
        return;
    if (p2.x == 0.f && p2.y == 0.f && p2.z == 0.f)
        return;


    const float dx = p1.x - p2.x;
    const float dy = p1.y - p2.y;
    const float dz = p1.z - p2.z;
    if (dx*dx + dy*dy + dz*dz > 80.f * 80.f)
        return;

    Vector2D screen1{}, screen2{};
    if (!world::to_screen(p1, screen1))
        return;
    if (!world::to_screen(p2, screen2))
        return;

    draw_gl_line(
        static_cast<int>(screen1.x), static_cast<int>(screen1.y),
        static_cast<int>(screen2.x), static_cast<int>(screen2.y),
        r, g, b);
}

static void draw_player_skeleton_gl(int player_index, byte r, byte g, byte b)
{
    const auto& bones = s_player_bones[player_index];
    if (!bones.has_data)
        return;


    const bool wide = (bones.num_bones >= 55);

    const int neck      = 6;
    const int head      = 7;
    const int l_clav    = 9;
    const int l_upper   = 10;
    const int l_fore    = 11;
    const int l_hand    = 12;
    const int r_clav    = wide ? 24 : 23;
    const int r_upper   = wide ? 25 : 24;
    const int r_fore    = wide ? 26 : 25;
    const int r_hand    = wide ? 27 : 26;
    const int spine_bot = 1;
    const int spine_mid = 3;
    const int spine_top = 5;
    const int l_thigh   = wide ? 42 : 40;
    const int l_calf    = wide ? 43 : 41;
    const int l_foot    = wide ? 44 : 42;
    const int r_thigh   = wide ? 48 : 46;
    const int r_calf    = wide ? 49 : 47;
    const int r_foot    = wide ? 50 : 48;


    draw_bone_pair_gl(player_index, spine_bot, spine_mid, r, g, b);
    draw_bone_pair_gl(player_index, spine_mid, spine_top, r, g, b);
    draw_bone_pair_gl(player_index, spine_top, neck,      r, g, b);
    draw_bone_pair_gl(player_index, neck,      head,      r, g, b);

    draw_bone_pair_gl(player_index, neck,    l_clav,  r, g, b);
    draw_bone_pair_gl(player_index, l_clav,  l_upper, r, g, b);
    draw_bone_pair_gl(player_index, l_upper, l_fore,  r, g, b);
    draw_bone_pair_gl(player_index, l_fore,  l_hand,  r, g, b);

    draw_bone_pair_gl(player_index, neck,    r_clav,  r, g, b);
    draw_bone_pair_gl(player_index, r_clav,  r_upper, r, g, b);
    draw_bone_pair_gl(player_index, r_upper, r_fore,  r, g, b);
    draw_bone_pair_gl(player_index, r_fore,  r_hand,  r, g, b);

    draw_bone_pair_gl(player_index, spine_bot, l_thigh, r, g, b);
    draw_bone_pair_gl(player_index, l_thigh,   l_calf,  r, g, b);
    draw_bone_pair_gl(player_index, l_calf,    l_foot,  r, g, b);

    draw_bone_pair_gl(player_index, spine_bot, r_thigh, r, g, b);
    draw_bone_pair_gl(player_index, r_thigh,   r_calf,  r, g, b);
    draw_bone_pair_gl(player_index, r_calf,    r_foot,  r, g, b);
}

void __cdecl detour_CL_ProcessEntityUpdate(cl_entity_t* ent)
{
    if (s_orig_entity_update)
        s_orig_entity_update(ent);

    if (!game::in_match())
        return;

    visuals::on_entity_update(ent);
}

void __cdecl detour_StudioSetRemapColors(int top, int bottom)
{
    if (game::in_match() && menu_vars::visuals_enabled
        && (menu_vars::skeleton_esp || menu_vars::box_esp
            || menu_vars::name_esp || menu_vars::weapon_esp))
    {
        if (auto* api = active_studio_api(); api && api->GetCurrentEntity)
        {
            if (cl_entity_t* ent = api->GetCurrentEntity())
            {
                if (ent->index != game::local_slot() + 1)
                    cache_player_visuals_for_entity(ent);
            }
        }
    }

    if (s_orig_studio_remap)
        s_orig_studio_remap(top, bottom);
}

void __cdecl detour_StudioSetupLighting(alight_t* plighting)
{
    if (s_chams_active && plighting)
    {
        plighting->shadelight   = 255;
        plighting->ambientlight = 128;
    }

    if (s_orig_studio_lighting)
        s_orig_studio_lighting(plighting);
}

void __cdecl detour_R_GLStudioDrawPoints()
{
    if (!s_renderer_hooked)
        studio_draw_points_chams_begin();

    if (s_orig_studio_draw)
        s_orig_studio_draw();

    if (!s_renderer_hooked)
        studio_draw_points_chams_end();

    if (game::in_match() && should_cache_bones())
    {
        if (auto* api = active_studio_api(); api && api->GetCurrentEntity)
        {
            if (cl_entity_t* ent = api->GetCurrentEntity())
                cache_player_bones_for_entity(ent);
        }
    }
}

void __cdecl detour_api_studio_draw_points()
{
    if (!s_renderer_hooked)
        studio_draw_points_chams_begin();

    if (s_orig_api_draw_points)
        s_orig_api_draw_points();

    if (!s_renderer_hooked)
        studio_draw_points_chams_end();
}

int __cdecl detour_iface_StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
    if (menu_vars::visuals_enabled && menu_vars::skeleton_esp && !menu_vars::weapon_esp && pplayer)
        pplayer->weaponmodel = 0;

    if (!s_orig_iface_draw_player)
        return 0;

    return draw_player_with_chams(flags, pplayer,
        reinterpret_cast<int(*)(int, entity_state_t*)>(s_orig_iface_draw_player));
}

int __cdecl detour_R_StudioDrawPlayer(int flags, entity_state_t* pplayer)
{
    if (menu_vars::visuals_enabled && menu_vars::skeleton_esp && !menu_vars::weapon_esp && pplayer)
        pplayer->weaponmodel = 0;

    if (!s_orig_studio_draw_player)
        return 0;

    return draw_player_with_chams(flags, pplayer, s_orig_studio_draw_player);
}

void __fastcall detour_StudioRenderModel(void* self, void*)
{
    if (!s_orig_studio_render_model || !s_orig_studio_render_final)
    {
        if (s_orig_studio_render_model)
            s_orig_studio_render_model(self, nullptr);
        return;
    }

    if (!menu_vars::visuals_enabled || !menu_vars::chams)
    {
        s_orig_studio_render_model(self, nullptr);
        return;
    }

    cl_entity_t* ent = nullptr;
    if (auto* api = active_studio_api(); api && api->GetCurrentEntity)
    {
        __try { ent = api->GetCurrentEntity(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { ent = nullptr; }
    }

    if (!ent || !entity_wants_chams(ent))
    {
        s_orig_studio_render_model(self, nullptr);
        return;
    }

    if (auto* api = active_studio_api(); api && api->SetForceFaceFlags)
        api->SetForceFaceFlags(0);

    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (menu_vars::chams_xqz)
    {
        chams_set_color(true);
        s_chams_pass = chams_pass_t::xqz;
        s_chams_active = true;
        glDepthFunc(GL_GREATER);
        glDisable(GL_DEPTH_TEST);
        s_orig_studio_render_final(self, nullptr);
        s_chams_active = false;
        s_chams_pass = chams_pass_t::none;
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    }

    chams_set_color(false);
    s_chams_pass = chams_pass_t::visible;
    s_chams_active = true;
    s_orig_studio_render_final(self, nullptr);
    s_chams_active = false;
    s_chams_pass = chams_pass_t::none;

    glEnable(GL_TEXTURE_2D);
}

static bool hook_studio_renderer()
{


    return false;
}

static void unhook_studio_renderer()
{
    if (!s_renderer_hooked || !s_renderer_vtable)
        return;

    if (s_orig_studio_render_model)
    {
        replace_fn_ptr(
            reinterpret_cast<void**>(&s_renderer_vtable->StudioRenderModel),
            reinterpret_cast<void*>(s_orig_studio_render_model),
            nullptr);
    }

    s_orig_studio_render_model = nullptr;
    s_orig_studio_render_final = nullptr;
    s_renderer_vtable          = nullptr;
    s_renderer_hooked          = false;
}

static bool hook_studio_remap()
{
    if (s_remap_hooked)
        return true;

    auto* api = game::client_studio_api();
    if (!api || !api->StudioSetRemapColors)
        return false;

    if (!replace_fn_ptr(
            reinterpret_cast<void**>(&api->StudioSetRemapColors),
            reinterpret_cast<void*>(detour_StudioSetRemapColors),
            reinterpret_cast<void**>(&s_orig_studio_remap)))
        return false;

    if (api->StudioDrawPoints)
    {
        replace_fn_ptr(
            reinterpret_cast<void**>(&api->StudioDrawPoints),
            reinterpret_cast<void*>(detour_api_studio_draw_points),
            reinterpret_cast<void**>(&s_orig_api_draw_points));
    }

    if (api->StudioSetupLighting)
    {
        replace_fn_ptr(
            reinterpret_cast<void**>(&api->StudioSetupLighting),
            reinterpret_cast<void*>(detour_StudioSetupLighting),
            reinterpret_cast<void**>(&s_orig_studio_lighting));
    }

    s_remap_hooked = true;
    return true;
}

static bool hook_pstudio_iface()
{
    if (s_iface_hooked)
        return true;

    r_studio_interface_t* iface = game::client_studio_iface();
    if (!iface && !s_pstudio_iface_lookup_done)
    {
        s_pstudio_iface_lookup_done = true;
        if (uint8_t* match = pattern_scan_any(k_pat_pstudio_api))
            iface = reinterpret_cast<r_studio_interface_t*>(match);
    }

    if (!iface)
        return false;

    __try
    {
        if (!iface->StudioDrawPlayer)
            return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    if (!replace_fn_ptr(
            reinterpret_cast<void**>(&iface->StudioDrawPlayer),
            reinterpret_cast<void*>(detour_iface_StudioDrawPlayer),
            reinterpret_cast<void**>(&s_orig_iface_draw_player)))
        return false;

    s_pstudio_iface = iface;
    s_iface_hooked = true;
    return true;
}

static void unhook_studio_remap()
{
    if (!s_remap_hooked)
        return;

    auto* api = game::client_studio_api();
    if (api)
    {
        if (s_orig_studio_remap)
        {
            replace_fn_ptr(
                reinterpret_cast<void**>(&api->StudioSetRemapColors),
                reinterpret_cast<void*>(s_orig_studio_remap),
                nullptr);
        }

        if (s_orig_api_draw_points)
        {
            replace_fn_ptr(
                reinterpret_cast<void**>(&api->StudioDrawPoints),
                reinterpret_cast<void*>(s_orig_api_draw_points),
                nullptr);
        }

        if (s_orig_studio_lighting)
        {
            replace_fn_ptr(
                reinterpret_cast<void**>(&api->StudioSetupLighting),
                reinterpret_cast<void*>(s_orig_studio_lighting),
                nullptr);
        }
    }

    s_orig_studio_remap     = nullptr;
    s_orig_api_draw_points  = nullptr;
    s_orig_studio_lighting  = nullptr;
    s_remap_hooked          = false;
}

static void unhook_pstudio_iface()
{
    if (!s_iface_hooked || !s_pstudio_iface)
        return;

    if (s_orig_iface_draw_player)
    {
        replace_fn_ptr(
            reinterpret_cast<void**>(&s_pstudio_iface->StudioDrawPlayer),
            reinterpret_cast<void*>(s_orig_iface_draw_player),
            nullptr);
    }

    s_orig_iface_draw_player = nullptr;
    s_pstudio_iface = nullptr;
    s_iface_hooked = false;
}

bool visuals::init_engine_hooks()
{
    if (!game::in_match())
        return false;

    if (s_hooks_attached)
    {
        ensure_studio_hooks();
        return true;
    }

    ensure_studio_hooks();

    s_entity_update_addr      = pattern_scan_any(k_pat_entity_update);
    s_studio_draw_addr        = pattern_scan_any(k_pat_studio_draw);
    s_studio_draw_player_addr = pattern_scan_any(k_pat_studio_draw_player);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    hook_gl_color();

    if (s_studio_draw_addr)
    {
        s_orig_studio_draw = reinterpret_cast<R_GLStudioDrawPoints_t>(s_studio_draw_addr);
        DetourAttach(reinterpret_cast<void**>(&s_orig_studio_draw), detour_R_GLStudioDrawPoints);
    }

    if (s_studio_draw_player_addr)
    {
        s_orig_studio_draw_player = reinterpret_cast<R_StudioDrawPlayer_t>(s_studio_draw_player_addr);
        DetourAttach(reinterpret_cast<void**>(&s_orig_studio_draw_player), detour_R_StudioDrawPlayer);
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

void visuals::ensure_studio_hooks()
{
    if (!GetModuleHandleA("client.dll"))
        return;

    hook_studio_remap();
    hook_pstudio_iface();
    hook_studio_renderer();


    if (s_hooks_attached)
        hook_hw_gl_dispatch();
}

void visuals::shutdown_engine_hooks()
{
    if (s_orig_entity_update || s_orig_studio_draw || s_orig_studio_draw_player)
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        if (s_orig_entity_update)
            DetourDetach(reinterpret_cast<void**>(&s_orig_entity_update), detour_CL_ProcessEntityUpdate);
        if (s_orig_studio_draw)
            DetourDetach(reinterpret_cast<void**>(&s_orig_studio_draw), detour_R_GLStudioDrawPoints);
        if (s_orig_studio_draw_player)
            DetourDetach(reinterpret_cast<void**>(&s_orig_studio_draw_player), detour_R_StudioDrawPlayer);
        DetourTransactionCommit();
        s_orig_entity_update      = nullptr;
        s_orig_studio_draw        = nullptr;
        s_orig_studio_draw_player = nullptr;
    }

    unhook_gl_color();
    unhook_hw_gl_dispatch();
    unhook_studio_renderer();
    unhook_studio_remap();
    unhook_pstudio_iface();

    s_entity_update_addr      = nullptr;
    s_studio_draw_addr        = nullptr;
    s_studio_draw_player_addr = nullptr;
    s_hooks_attached          = false;
    s_hw_gl_lookup_done       = false;
    s_pstudio_iface_lookup_done = false;
    memset(s_player_bones, 0, sizeof(s_player_bones));
    memset(s_player_esp, 0, sizeof(s_player_esp));

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

static void clear_frame_visual_caches()
{
    for (int i = 1; i <= MAX_CLIENTS; ++i)
        s_player_esp[i].has_data = false;


}

void visuals::draw_world_esp_gl()
{
    if (!game::in_match())
        return;

    const bool draw_skel = menu_vars::skeleton_esp;
    const bool draw_box  = menu_vars::box_esp;
    if (!draw_skel && !draw_box)
        return;

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int width  = viewport[2];
    const int height = viewport[3];
    if (width <= 0 || height <= 0)
        return;

    GLboolean depth_was = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    for (int i = 1; i <= MAX_CLIENTS; ++i)
    {
        if (draw_box && s_player_esp[i].has_data)
            draw_player_box_gl(i);

        if (draw_skel && s_player_bones[i].has_data)
        {
            const auto& esp = s_player_esp[i];
            const byte r = esp.has_data ? esp.color_r : 200;
            const byte g = esp.has_data ? esp.color_g : 200;
            const byte b = esp.has_data ? esp.color_b : 200;
            draw_player_skeleton_gl(i, r, g, b);
        }
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor3f(1.0f, 1.0f, 1.0f);
    if (depth_was)
        glEnable(GL_DEPTH_TEST);
}

void visuals::finish_frame_esp()
{
    clear_frame_visual_caches();
}

void visuals::draw_skeleton_esp(OSHGui::Drawing::Graphics& gfx)
{
    (void)gfx;
}

void visuals::draw_box_esp(OSHGui::Drawing::Graphics& gfx)
{
    (void)gfx;
}

void visuals::draw_name_esp(OSHGui::Drawing::Graphics& gfx, const OSHGui::Drawing::FontPtr& font)
{
    if (!menu_vars::name_esp || !game::in_match() || !font)
        return;

    for (int i = 1; i <= MAX_CLIENTS; ++i)
    {
        const auto& esp = s_player_esp[i];
        if (!esp.has_data || !esp.name[0])
            continue;

        box_metrics_t box{};
        if (!project_esp_box(esp, box))
            continue;

        const float text_w = font->GetTextExtent(esp.name);
        const float text_h = font->GetFontHeight();
        const float label_x = box.top.x - text_w * 0.5f;
        const float label_y = box.top.y - text_h - 2.0f;
        const auto color = OSHGui::Drawing::Color::FromRGB(esp.color_r, esp.color_g, esp.color_b);
        gfx.DrawString(esp.name, font, color, label_x, label_y);
    }
}

void visuals::draw_weapon_esp(OSHGui::Drawing::Graphics& gfx, const OSHGui::Drawing::FontPtr& font)
{
    if (!menu_vars::weapon_esp || !game::in_match() || !font)
        return;

    for (int i = 1; i <= MAX_CLIENTS; ++i)
    {
        const auto& esp = s_player_esp[i];
        if (!esp.has_data || !esp.weapon[0])
            continue;

        box_metrics_t box{};
        if (!project_esp_box(esp, box))
            continue;

        const float text_w = font->GetTextExtent(esp.weapon);
        const float label_x = box.bot.x - text_w * 0.5f;
        const float label_y = box.bot.y + 2.0f;
        const auto color = OSHGui::Drawing::Color::FromRGB(esp.color_r, esp.color_g, esp.color_b);
        gfx.DrawString(esp.weapon, font, color, label_x, label_y);
    }
}

static const char* team_label(int team)
{
    if (team == TEAM_TERRORIST) return "T";
    if (team == TEAM_CT)        return "CT";
    return "--";
}

void visuals::update_debug_status(semtex_shared_t* s)
{
    if (!s)
        return;

    s->show_teammates = menu_vars::teammates ? 1 : 0;
    s->local_slot       = game::in_match() ? game::local_slot() : -1;
    s->local_team       = game::in_match() ? local_team_number() : TEAM_UNASSIGNED;
    s->local_extra_team = 0;

    if (game::in_match() && s->local_slot >= 0)
    {
        if (auto* extra = game_extra_info())
            s->local_extra_team = static_cast<int>(extra[s->local_slot + 1].teamnumber);
    }

    int cached = 0;
    int weapons = 0;
    int draw_enemies = 0;
    int draw_teammates = 0;

    char line[192]{};
    int pos = sprintf_s(line, "you slot=%d team=%s tab=%d filter=%s | ",
        s->local_slot,
        team_label(s->local_team),
        s->local_extra_team,
        s->show_teammates ? "ALL" : "ENEMIES");

    for (int slot = 0; slot < MAX_CLIENTS && pos > 0 && pos < static_cast<int>(sizeof(line) - 24); ++slot)
    {
        if (slot == s->local_slot)
            continue;

        const int team = team_for_slot(slot);
        const bool is_tm = is_teammate_for_slot(slot);
        const bool would_draw = s->show_teammates || !is_tm;

        const auto& esp = s_player_esp[slot + 1];
        if (!esp.has_data && team == TEAM_UNASSIGNED)
            continue;

        if (esp.has_data)
        {
            ++cached;
            if (esp.weapon[0])
                ++weapons;
        }

        if (would_draw && team != TEAM_UNASSIGNED)
        {
            if (is_tm)
                ++draw_teammates;
            else
                ++draw_enemies;
        }

        const char* wpn = esp.weapon[0] ? esp.weapon : "-";
        pos += sprintf_s(line + pos, sizeof(line) - pos, "%d:%s%s:%s ",
            slot + 1,
            team_label(team),
            is_tm ? "*" : "",
            wpn);
    }

    s->esp_cached          = cached;
    s->weapon_cached       = weapons;
    s->esp_draw_enemies    = draw_enemies;
    s->esp_draw_teammates  = draw_teammates;
    strncpy_s(s->team_snapshot, line, _TRUNCATE);
}

bool visuals::get_player_bone_pos(int entity_index, int bone_index, Vector& out)
{
    if (entity_index <= 0 || entity_index > MAX_CLIENTS)
        return false;

    if (bone_index < 0 || bone_index >= MAXSTUDIOBONES)
        return false;

    const auto& bones = s_player_bones[entity_index];
    if (!bones.has_data || bone_index >= bones.num_bones)
        return false;

    const Vector& pos = bones.positions[bone_index];
    if (pos.IsZero())
        return false;

    out = pos;
    return true;
}

bool visuals::get_player_head_pos(int entity_index, Vector& out)
{
    if (visuals::get_player_bone_pos(entity_index, 7, out))
        return true;

    if (entity_index > 0 && entity_index <= MAX_CLIENTS)
    {
        const auto& esp = s_player_esp[entity_index];
        if (esp.has_data && !esp.origin.IsZero())
        {
            const float head_z = esp.maxs.z > 0.f ? esp.maxs.z : 36.f;
            out = esp.origin + Vector(0.f, 0.f, head_z);
            return true;
        }
    }

    cl_entity_t* ent = game::cl_entity(entity_index);
    if (!ent || !ent->player || ent->origin.IsZero())
        return false;

    float head_z = ent->curstate.maxs.z;
    if (head_z <= 0.f)
        head_z = 36.f;

    out = ent->origin + Vector(0.f, 0.f, head_z);
    return true;
}
