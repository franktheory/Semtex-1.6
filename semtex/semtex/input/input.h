#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "../../oshgui/OSHGui.hpp"
#include "../sdl/sdl_minimal.h"

struct SDL_Window;

class c_input
{
public:
    c_input() = default;

    bool init_sdl(SDL_Window* window, OSHGui::Application* instance);
    bool init(const char* window_name, OSHGui::Application* instance);
    bool init_hwnd(HWND wnd, OSHGui::Application* instance);

    void poll_frame();

    bool handle(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    bool remove();
    bool key_pressed(int vk) const;

    WNDPROC get_original_wndproc() const { return m_original_wndproc; }
    HWND get_hwnd() const { return m_window; }

private:
    bool process_message(LPMSG msg, WPARAM wparam, LPARAM lparam);
    void update_sdl_capture(bool menu_open);

    HWND m_window = nullptr;
    WNDPROC m_original_wndproc = nullptr;
    OSHGui::Application* m_instance = nullptr;
    bool m_keys[256]{};

    SDL_SetRelativeMouseMode_t m_sdl_set_relative_mouse = nullptr;
    SDL_GetRelativeMouseMode_t m_sdl_get_relative_mouse = nullptr;
    SDL_ShowCursor_t m_sdl_show_cursor = nullptr;
    bool m_menu_was_open = false;
    int  m_saved_relative_mouse = 1;

    void resolve_sdl(HMODULE sdl);
};
