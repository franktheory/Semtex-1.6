#pragma once

#include "keybind.h"
#include "../inc.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace menu_ctrl
{
    using OSHGui::Control;
    using OSHColor = OSHGui::Drawing::Color;
    using OSHGui::Misc::AnsiString;

    class groupbox : public OSHGui::GroupBox
    {
    public:
        groupbox(const AnsiString& text, int x, int y, int w, int h);
    };

    class checkbox : public OSHGui::CheckBox
    {
    public:
        checkbox(const AnsiString& text, Control* parent, bool* value);
        checkbox(const AnsiString& text, int x, int y, Control* parent, bool* value);
    };

    class slider : public OSHGui::TrackBar
    {
    public:
        slider(const AnsiString& text, Control* parent, float min, float max, int* value, int default_value, const std::string& suffix = "");
        slider(const AnsiString& text, Control* parent, float min, float max, float* value, int precision, float default_value, const std::string& suffix = "");
    };

    class combo : public OSHGui::ComboBox
    {
    public:
        combo(const AnsiString& text, const std::vector<AnsiString>& items, Control* parent, int max_items, int* value, int parent_width);
    };

    class listbox : public OSHGui::ListBox
    {
    public:
        listbox(const AnsiString& text, Control* parent, const std::vector<AnsiString>& items, int* selected_index, int height_items = 4);
    };

    class radio : public OSHGui::RadioButton
    {
    public:
        radio(const AnsiString& text, Control* parent, int* group_value, int option_index);
    };

    class hotkey : public OSHGui::HotkeyControl
    {
    public:
        hotkey(const AnsiString& text, Control* parent, int* key_value, int parent_width);
    };

    class text_field : public OSHGui::TextBox
    {
    public:
        text_field(const AnsiString& label, Control* parent, int parent_width);
    };

    class color_picker : public OSHGui::ColorButton
    {
    public:
        color_picker(Control* parent, Control* align_to, float* rgba);
    };

    class button : public OSHGui::Button
    {
    public:
        button(const AnsiString& text, Control* parent, std::function<void()> on_click);
    };

    class checkbox_keybind
    {
    public:
        checkbox_keybind(const AnsiString& text, Control* parent, bool* value, int* key_value, int* mode_value, keybind::state_t* bind_state, int parent_width);
    };

    class multi_combo : public OSHGui::ComboBox
    {
    public:
        multi_combo(const AnsiString& text, const std::vector<AnsiString>& items, Control* parent, uint32_t* mask, int parent_width);
    };
}
