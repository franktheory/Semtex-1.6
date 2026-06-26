#include "input.h"
#include "../inc.h"
#include "../../oshgui/Event/KeyboardMessage.hpp"

namespace
{
    static OSHGui::Key keyboard_modifiers()
    {
        OSHGui::Key modifier = OSHGui::Key::None;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
            modifier |= OSHGui::Key::Control;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
            modifier |= OSHGui::Key::Shift;
        if (GetAsyncKeyState(VK_MENU) & 0x8000)
            modifier |= OSHGui::Key::Alt;
        return modifier;
    }

    static void send_character(OSHGui::Application* instance, int vk, OSHGui::Key key)
    {
        BYTE keyboard_state[256]{};
        if (!GetKeyboardState(keyboard_state))
            return;

        WORD chars[2]{};
        const UINT scan = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        if (ToAscii(static_cast<UINT>(vk), scan, keyboard_state, chars, 0) != 1)
            return;

        const char ch = static_cast<char>(chars[0] & 0xFF);
        if (ch == '\0')
            return;

        instance->ProcessKeyboardMessage(
            OSHGui::KeyboardMessage(OSHGui::KeyboardState::Character, key, ch));
    }

    static void poll_keyboard(OSHGui::Application* instance)
    {
        static bool prev_down[256]{};

        for (int vk = 8; vk < 256; ++vk)
        {
            const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (down && !prev_down[vk])
            {
                const OSHGui::Key key = static_cast<OSHGui::Key>(vk) | keyboard_modifiers();
                instance->ProcessKeyboardMessage(
                    OSHGui::KeyboardMessage(OSHGui::KeyboardState::KeyDown, key, '\0'));
                send_character(instance, vk, key);
            }
            prev_down[vk] = down;
        }
    }
}

bool c_input::init_sdl(SDL_Window* window, OSHGui::Application* instance)
{
    if (!window || !instance)
        return false;

    m_instance = instance;

    HMODULE sdl = GetModuleHandleA("SDL2.dll");
    if (!sdl)
        return false;

    auto get_wm_info = reinterpret_cast<SDL_GetWindowWMInfo_t>(
        GetProcAddress(sdl, "SDL_GetWindowWMInfo"));
    resolve_sdl(sdl);

    if (!get_wm_info)
        return false;

    SDL_SysWMinfo wm{};
    SDL_VERSION(&wm.version);
    if (!get_wm_info(window, &wm))
        return false;

    m_window = wm.info.win.window;
    return m_window != nullptr;
}

void c_input::resolve_sdl(HMODULE sdl)
{
    if (!sdl)
        sdl = GetModuleHandleA("SDL2.dll");
    if (!sdl)
        return;

    m_sdl_set_relative_mouse = reinterpret_cast<SDL_SetRelativeMouseMode_t>(
        GetProcAddress(sdl, "SDL_SetRelativeMouseMode"));
    m_sdl_get_relative_mouse = reinterpret_cast<SDL_GetRelativeMouseMode_t>(
        GetProcAddress(sdl, "SDL_GetRelativeMouseMode"));
    m_sdl_show_cursor = reinterpret_cast<SDL_ShowCursor_t>(
        GetProcAddress(sdl, "SDL_ShowCursor"));
}

bool c_input::init_hwnd(HWND wnd, OSHGui::Application* instance)
{
    if (m_window || !instance || !wnd)
        return false;

    m_instance = instance;
    m_window = wnd;
    resolve_sdl(nullptr);
    return true;
}

bool c_input::init(const char* window_name, OSHGui::Application* instance)
{
    HWND wnd = FindWindowA(window_name, nullptr);
    return wnd && init_hwnd(wnd, instance);
}

void c_input::update_sdl_capture(bool menu_open)
{
    if (!m_sdl_show_cursor)
        return;

    if (menu_open && !m_menu_was_open)
    {
        if (m_sdl_get_relative_mouse)
            m_saved_relative_mouse = m_sdl_get_relative_mouse();

        if (m_sdl_set_relative_mouse)
            m_sdl_set_relative_mouse(0);

        m_sdl_show_cursor(1);
        ClipCursor(nullptr);
    }
    else if (!menu_open && m_menu_was_open)
    {
        m_sdl_show_cursor(0);

        if (m_sdl_set_relative_mouse && m_saved_relative_mouse)
            m_sdl_set_relative_mouse(1);
    }

    m_menu_was_open = menu_open;
}

void c_input::poll_frame()
{
    if (!m_instance || !m_window)
        return;

    static bool insert_prev = false;
    const bool insert_now = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insert_now && !insert_prev)
    {
        const bool opening = !m_instance->IsEnabled();
        m_instance->IsEnabled() ? m_instance->Disable() : m_instance->Enable();
        if (opening)
            m_instance->GetRenderSurface().Invalidate();
    }
    insert_prev = insert_now;

    const bool menu_open = m_instance->IsEnabled();
    update_sdl_capture(menu_open);

    if (!menu_open)
        return;

    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(m_window, &pt);

    const OSHGui::Drawing::PointI loc(pt.x, pt.y);
    m_instance->ProcessMouseMessage(
        OSHGui::MouseMessage(OSHGui::MouseState::Move, OSHGui::MouseButton::None, loc, 0));

    struct btn_t { int vk; OSHGui::MouseButton button; };
    static bool prev_down[5]{};
    const btn_t buttons[] = {
        { VK_LBUTTON,   OSHGui::MouseButton::Left },
        { VK_RBUTTON,   OSHGui::MouseButton::Right },
        { VK_MBUTTON,   OSHGui::MouseButton::Middle },
        { VK_XBUTTON1,  OSHGui::MouseButton::XButton1 },
        { VK_XBUTTON2,  OSHGui::MouseButton::XButton2 },
    };

    poll_keyboard(m_instance);

    for (int i = 0; i < 5; ++i)
    {
        const bool down = (GetAsyncKeyState(buttons[i].vk) & 0x8000) != 0;
        if (down && !prev_down[i])
            m_instance->ProcessMouseMessage(
                OSHGui::MouseMessage(OSHGui::MouseState::Down, buttons[i].button, loc, 0));
        else if (!down && prev_down[i])
            m_instance->ProcessMouseMessage(
                OSHGui::MouseMessage(OSHGui::MouseState::Up, buttons[i].button, loc, 0));
        prev_down[i] = down;
    }
}

bool c_input::handle(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    (void)hwnd;
    (void)msg;
    (void)wparam;
    (void)lparam;
    return false;
}

bool c_input::remove()
{
    if (m_menu_was_open)
        update_sdl_capture(false);

    m_window = nullptr;
    m_original_wndproc = nullptr;
    return true;
}

bool c_input::key_pressed(int vk) const
{
    if (vk <= 0)
        return false;


    if (vk < 256)
        return (GetAsyncKeyState(vk) & 0x8000) != 0;

    return false;
}

bool c_input::process_message(LPMSG msg, WPARAM wparam, LPARAM lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    return false;
}
