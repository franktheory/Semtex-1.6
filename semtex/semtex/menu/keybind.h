#pragma once

#include <cstdint>

namespace keybind
{
    enum class mode : int
    {
        hold   = 0,
        toggle = 1,
        always = 2,
    };

    struct state_t
    {
        bool toggle_on = false;
        bool prev_down = false;
    };

    int  vk_from_key(int stored_key);
    mode parse_mode(int value);
    mode cycle_mode(mode current);

    const char* mode_label(mode bind_mode);

    void reset_state(state_t& state);
    bool active(int stored_key, mode bind_mode, state_t& state);
}
