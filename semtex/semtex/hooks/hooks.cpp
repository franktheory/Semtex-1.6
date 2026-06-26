#include "hooks.h"
#include "../inc.h"
#include "../config/config.h"
#include "../features/visuals.h"
#include "../features/aimbot.h"
#include "../features/norecoil.h"
#include "../features/movement.h"
#include "../game/memory.h"
#include "../../detours/detours.h"
#include "../../shared/shared.h"
#include <exception>

extern semtex_shared_t* g_shm();

static volatile bool g_unloading = false;
static volatile bool g_in_hook   = false;
static volatile LONG g_init_done = 0;

static void shm_log(const char* msg)
{
    auto s = g_shm();
    if (s)
        strcpy_s(s->last_error, msg);
}

static void shm_log_status()
{
    auto s = g_shm();
    if (!s)
        return;

    if (s->renderer_ok && s->menu_ok && s->input_ok)
    {
        if (s->game_ok)
            shm_log("ALL OK - press INSERT to toggle menu");
        else if (!GetModuleHandleA("hw.dll"))
            shm_log("menu OK - waiting for hw.dll (join a map for ESP)");
    }
    else if (!s->menu_ok)
        shm_log("FAILED: menu did not initialize (see init_step)");
    else if (!s->renderer_ok)
        shm_log("FAILED: renderer did not initialize");
    else if (!s->input_ok)
        shm_log("WARN: input init failed (INSERT polling still works)");
}

static void shm_update_game_status()
{
    auto s = g_shm();
    if (!s)
        return;

    s->game_ok = game::in_match() ? 1 : 0;
    s->visuals_ok = (game::in_match() && visuals::hooks_attached()) ? 1 : 0;
    s->pat_cl = game::pat_cl();
    s->pat_studio_api = game::pat_studio_api();
    s->pat_client_studio = game::pat_client_studio();
    s->pat_pstudiohdr = game::pat_pstudiohdr();
    s->pat_extra_info = game::pat_extra_info();
    s->pat_studio_draw = visuals::has_studio_remap_hook() ? 1 : 0;
    s->pat_studio_draw_player = visuals::has_studio_draw_player_hook() ? 1 : 0;
    movement::update_debug_status(s);
    aimbot::update_debug_status();
    norecoil::update_debug_status();

    if (!s->game_ok)
        strcpy_s(s->last_error, game::last_error());
    else if (!game::pat_client_studio())
        shm_log("game OK (waiting for client.dll studio copy)");
    else if (!visuals::has_studio_remap_hook())
        shm_log("game OK (client studio found; StudioSetRemapColors hook pending)");
    else if (!visuals::has_studio_draw_player_hook())
        shm_log("game OK (studio draw player hook pending)");
    else
        shm_log("game OK (skeleton via client studio hook)");
}

static void try_init_visuals()
{
    if (!GetModuleHandleA("hw.dll"))
        return;

    game::tick();

    if (game::in_match())
    {
        if (!visuals::hooks_attached())
            visuals::init_engine_hooks();
        else if (visuals::studio_hooks_pending())
            visuals::ensure_studio_hooks();
    }
    else if (visuals::hooks_attached())
        visuals::shutdown_engine_hooks();

    if (game::movement_allowed())
    {
        if (!movement::hooks_attached())
            movement::init_hooks();
    }
    else if (movement::hooks_attached())
        movement::shutdown_hooks();

    shm_update_game_status();
}

bool c_hooks::init()
{
    HMODULE opengl = GetModuleHandleA("opengl32.dll");
    if (!opengl)
        opengl = LoadLibraryA("opengl32.dll");
    if (!opengl)
    {
        shm_log("ERROR: opengl32.dll not found");
        return false;
    }

    m_orig_swap = reinterpret_cast<wglSwapBuffers_t>(GetProcAddress(opengl, "wglSwapBuffers"));
    if (!m_orig_swap)
    {
        shm_log("ERROR: wglSwapBuffers not found");
        return false;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<void**>(&m_orig_swap), hook::wglSwapBuffers);
    const LONG r = DetourTransactionCommit();
    if (r != NO_ERROR)
    {
        char buf[128];
        sprintf_s(buf, "ERROR: DetourTransactionCommit failed code %ld", r);
        shm_log(buf);
        return false;
    }

    shm_log("wglSwapBuffers hooked OK");
    return true;
}

bool c_hooks::release()
{
    if (!m_orig_swap)
        return true;

    g_unloading = true;
    while (g_in_hook)
        Sleep(5);

    g_renderer.shutdown_gl();

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(reinterpret_cast<void**>(&m_orig_swap), hook::wglSwapBuffers);
    DetourTransactionCommit();
    m_orig_swap = nullptr;
    return true;
}

BOOL WINAPI hook::wglSwapBuffers(HDC hdc)
{
    if (g_unloading)
        return g_hooks.m_orig_swap(hdc);

    g_in_hook = true;

    auto s = g_shm();

    if (InterlockedCompareExchange(&g_init_done, 1, 0) == 0)
    {
        shm_log("creating dedicated GUI GL context...");

        if (!g_renderer.init_gl_context(hdc))
        {
            shm_log("ERROR: init_gl_context failed");
            if (s)
                s->init_step = -1;
        }
        else if (s)
        {
            s->init_step = 10;

            try
            {
                g_renderer.init();
                if (s)
                    s->renderer_ok = 1;
            }
            catch (const std::exception& ex)
            {
                char buf[256];
                sprintf_s(buf, "ERROR: renderer.init(): %s", ex.what());
                shm_log(buf);
            }
            catch (...)
            {
                shm_log("ERROR: renderer.init() threw");
            }

            if (s && s->renderer_ok)
            {
                try
                {
                    config::load("default");
                    g_menu.init();
                    if (s)
                        s->menu_ok = 1;
                }
                catch (const std::exception& ex)
                {
                    char buf[256];
                    sprintf_s(buf, "ERROR: menu.init(): %s", ex.what());
                    shm_log(buf);
                }
                catch (...)
                {
                    shm_log("ERROR: menu.init() threw");
                }
            }

            g_renderer.bind_game_context();

            HWND wnd = WindowFromDC(hdc);
            if (!wnd)
                wnd = FindWindowA("SDL_app", nullptr);
            if (s)
                s->hwnd = static_cast<int32_t>(reinterpret_cast<intptr_t>(wnd));

            if (s && s->menu_ok)
            {
                const bool input_ok = g_input.init_hwnd(wnd, g_renderer.get_instance());
                if (s)
                    s->input_ok = input_ok ? 1 : 0;
            }

            shm_log_status();
        }
    }

    g_input.poll_frame();

    try_init_visuals();

    if (!g_unloading && s && s->renderer_ok && game::in_match())
        visuals::refresh_players();

    if (!g_unloading && s && s->renderer_ok)
        g_renderer.render_overlay();

    if (!g_unloading && s && s->renderer_ok && s->menu_ok)
        g_renderer.render_if_enabled();

    if (s)
    {
        s->hook_fired = 1;
        s->swap_call_count++;
        s->game_ok = game::in_match() ? 1 : 0;
        s->visuals_ok = (game::in_match() && visuals::hooks_attached()) ? 1 : 0;
        s->pat_cl = game::pat_cl();
        s->pat_studio_api = game::pat_studio_api();
        s->pat_client_studio = game::pat_client_studio();
        s->pat_pstudiohdr = game::pat_pstudiohdr();
        s->pat_extra_info = game::pat_extra_info();
        s->pat_studio_draw = visuals::has_studio_remap_hook() ? 1 : 0;
        s->pat_studio_draw_player = visuals::has_studio_draw_player_hook() ? 1 : 0;
        movement::update_debug_status(s);
        aimbot::update_debug_status();
        norecoil::update_debug_status();
        s->oshgui_init = OSHGui::Application::HasBeenInitialized() ? 1 : 0;
        if (g_renderer.get_instance())
            s->menu_visible = g_renderer.get_instance()->IsEnabled() ? 1 : 0;

        if (game::in_match())
            visuals::update_debug_status(s);
    }

    g_in_hook = false;
    return g_hooks.m_orig_swap(hdc);
}
