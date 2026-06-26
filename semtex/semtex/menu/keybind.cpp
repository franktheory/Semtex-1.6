#include "keybind.h"

#include "../../oshgui/Event/Key.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace keybind
{
    int vk_from_key(int stored_key)
    {
        return stored_key & static_cast<int>(OSHGui::Key::KeyCode);
    }

    mode parse_mode(int value)
    {
        switch (value)
        {
        case static_cast<int>(mode::toggle): return mode::toggle;
        case static_cast<int>(mode::always): return mode::always;
        default:                             return mode::hold;
        }
    }

    mode cycle_mode(mode current)
    {
        switch (current)
        {
        case mode::hold:   return mode::toggle;
        case mode::toggle: return mode::always;
        default:           return mode::hold;
        }
    }

    const char* mode_label(mode bind_mode)
    {
        switch (bind_mode)
        {
        case mode::toggle: return "Toggle";
        case mode::always: return "Always";
        default:           return "Hold";
        }
    }

    void reset_state(state_t& state)
    {
        state.toggle_on = false;
        state.prev_down = false;
    }

    bool active(int stored_key, mode bind_mode, state_t& state)
    {
        if (bind_mode == mode::always)
            return true;

        const int vk = vk_from_key(stored_key);
        if (vk <= 0)
            return false;

        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;

        if (bind_mode == mode::toggle)
        {
            if (down && !state.prev_down)
                state.toggle_on = !state.toggle_on;

            state.prev_down = down;
            return state.toggle_on;
        }

        return down;
    }
}
