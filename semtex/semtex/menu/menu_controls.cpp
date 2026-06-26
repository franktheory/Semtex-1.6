#include "menu_controls.h"
#include "keybind.h"
#include "../../oshgui/Controls/Timer.hpp"

static OSHGui::Drawing::FontPtr menu_font()
{
    return g_renderer.get_instance()->GetDefaultFont();
}

static OSHGui::Drawing::Color menu_accent()
{
    return g_renderer.get_instance()->GetPrimaryColor();
}

static void init_slider(menu_ctrl::slider* bar, OSHGui::Control* parent, float min, float max, int precision,
                        const OSHGui::Misc::AnsiString& text, const std::string& suffix)
{
    bar->SetFont(menu_font());
    bar->SetBackColor(menu_accent());
    bar->SetMinimum(min);
    bar->SetMaximum(max);
    bar->SetPrecision(precision);
    bar->SetText(text);
    bar->SetAppendText(suffix);
    parent->AddControl(bar);
}

menu_ctrl::groupbox::groupbox(const AnsiString& text, int x, int y, int w, int h)
{
    SetFont(menu_font());
    SetBounds(x, y, w, h);
    SetText(text);
    g_menu.set_y_pos(10);
}

menu_ctrl::checkbox::checkbox(const AnsiString& text, int x, int y, Control* parent, bool* value)
{
    SetBackColor(menu_accent());
    SetFont(menu_font());
    SetLocation(x, y);
    SetText(text);
    SetChecked(*value);
    parent->AddControl(this);

    GetCheckedChangedEvent() += OSHGui::CheckedChangedEventHandler([this, value](Control*)
    {
        *value = GetChecked();
    });
}

menu_ctrl::checkbox::checkbox(const AnsiString& text, Control* parent, bool* value)
    : checkbox(text, g_menu.get_x_pos(), g_menu.get_y_pos(), parent, value)
{
    g_menu.push_y_pos(18);
}

menu_ctrl::slider::slider(const AnsiString& text, Control* parent, float min, float max, int* value, int default_value, const std::string& suffix)
{
    SetLocation(parent->GetWidth() / 2 - GetWidth() / 2, g_menu.get_y_pos() + 4);
    init_slider(this, parent, min, max, 0, text, suffix);
    SetValue(static_cast<float>(*value));

    GetValueChangedEvent() += OSHGui::ValueChangedEventHandler([this, value](Control*)
    {
        *value = static_cast<int>(GetValue());
    });

    g_menu.push_y_pos(GetSize().Height + 10);
}

menu_ctrl::slider::slider(const AnsiString& text, Control* parent, float min, float max, float* value, int precision, float default_value, const std::string& suffix)
{
    SetLocation(parent->GetWidth() / 2 - GetWidth() / 2, g_menu.get_y_pos() + 4);
    init_slider(this, parent, min, max, precision, text, suffix);
    SetValue(*value);

    GetValueChangedEvent() += OSHGui::ValueChangedEventHandler([this, value](Control*)
    {
        *value = GetValue();
    });

    g_menu.push_y_pos(GetSize().Height + 10);
}

menu_ctrl::combo::combo(const AnsiString& text, const std::vector<AnsiString>& items, Control* parent, int max_items, int* value, int parent_width)
{
    SetLocation(parent_width / 2 - GetWidth() / 2 - 3, g_menu.get_y_pos() + 12);
    SetFont(menu_font());
    SetMaxShowItems(max_items);

    auto* label = new OSHGui::Label();
    label->SetFont(menu_font());
    label->SetForeColor(OSHColor::FromRGB(201, 201, 201));
    label->SetLocation(GetLeft(), GetTop() - 12);
    label->SetText(text);
    parent->AddControl(label);

    for (const auto& item : items)
        AddItem(item);

    SetSelectedIndex(*value, false);
    parent->AddControl(this);

    GetSelectedIndexChangedEvent() += OSHGui::SelectedIndexChangedEventHandler([this, value](Control*)
    {
        *value = GetSelectedIndex();
    });

    g_menu.push_y_pos(40);
}

menu_ctrl::listbox::listbox(const AnsiString& text, Control* parent, const std::vector<AnsiString>& items, int* selected_index, int height_items)
{
    const int top = text.empty() ? g_menu.get_y_pos() : g_menu.get_y_pos() + 14;
    SetLocation(g_menu.get_x_pos(), top);
    SetSize(220, height_items * 18 + 8);
    SetFont(menu_font());
    SetAutoScrollEnabled(true);
    ExpandSizeToShowItems(height_items);

    if (!text.empty())
    {
        auto* label = new OSHGui::Label();
        label->SetFont(menu_font());
        label->SetForeColor(OSHColor::FromRGB(201, 201, 201));
        label->SetLocation(GetLeft(), GetTop() - 14);
        label->SetText(text);
        parent->AddControl(label);
    }

    for (const auto& item : items)
        AddItem(item);

    if (*selected_index >= 0 && *selected_index < static_cast<int>(items.size()))
        SetSelectedIndex(*selected_index);

    parent->AddControl(this);

    GetSelectedIndexChangedEvent() += OSHGui::SelectedIndexChangedEventHandler([this, selected_index](Control*)
    {
        *selected_index = GetSelectedIndex();
    });

    g_menu.push_y_pos(GetSize().Height + (text.empty() ? 12 : 22));
}

menu_ctrl::radio::radio(const AnsiString& text, Control* parent, int* group_value, int option_index)
{
    SetBackColor(menu_accent());
    SetFont(menu_font());
    SetLocation(g_menu.get_x_pos(), g_menu.get_y_pos());
    SetText(text);
    SetChecked(*group_value == option_index);
    parent->AddControl(this);

    GetCheckedChangedEvent() += OSHGui::CheckedChangedEventHandler([group_value, option_index](Control* sender)
    {
        if (static_cast<radio*>(sender)->GetChecked())
            *group_value = option_index;
    });

    g_menu.push_y_pos(18);
}

menu_ctrl::hotkey::hotkey(const AnsiString& text, Control* parent, int* key_value, int parent_width)
{
    SetLocation(parent_width / 2 - GetWidth() / 2 - 3, g_menu.get_y_pos() + 12);
    SetFont(menu_font());

    auto* label = new OSHGui::Label();
    label->SetFont(menu_font());
    label->SetForeColor(OSHColor::FromRGB(201, 201, 201));
    label->SetLocation(GetLeft(), GetTop() - 12);
    label->SetText(text);
    parent->AddControl(label);

    SetHotkey(static_cast<OSHGui::Key>(*key_value));
    parent->AddControl(this);

    GetHotkeyChangedEvent() += OSHGui::HotkeyChangedEventHandler([this, key_value](Control*)
    {
        *key_value = static_cast<int>(GetHotkey());
    });

    g_menu.push_y_pos(36);
}

menu_ctrl::text_field::text_field(const AnsiString& label_text, Control* parent, int parent_width)
{
    const int x = g_menu.get_x_pos();
    const int field_w = parent_width - x * 2;
    const int top = label_text.empty() ? g_menu.get_y_pos() : g_menu.get_y_pos() + 14;

    SetLocation(x, top);
    SetFont(menu_font());
    SetSize(field_w, 22);

    if (!label_text.empty())
    {
        auto* label = new OSHGui::Label();
        label->SetFont(menu_font());
        label->SetForeColor(OSHColor::FromRGB(201, 201, 201));
        label->SetLocation(x, top - 14);
        label->SetText(label_text);
        parent->AddControl(label);
    }

    parent->AddControl(this);
    g_menu.push_y_pos(GetSize().Height + (label_text.empty() ? 10 : 14));
}

menu_ctrl::color_picker::color_picker(Control* parent, Control* align_to, float* col)
{
    const int y = align_to
        ? (align_to->GetType() == OSHGui::ControlType::ComboBox
            ? align_to->GetTop() + 6
            : align_to->GetTop() + 2)
        : g_menu.get_y_pos();

    SetLocation(parent->GetWidth() - 38, y);
    SetColor(OSHColor::FromARGB(col));
    parent->AddControl(this);

    auto* timer = new OSHGui::Timer();
    timer->SetInterval(50);
    timer->Start();
    parent->AddControl(timer);

    timer->GetTickEvent() += OSHGui::TickEventHandler([this, col](Control*)
    {
        SetColor(OSHColor::FromARGB(col));
    });

    GetColorChangedEvent() += OSHGui::ColorChangedEventHandler([this, col](Control*, const OSHColor& color)
    {
        col[0] = color.GetAlpha() * 255.f;
        col[1] = color.GetRed() * 255.f;
        col[2] = color.GetGreen() * 255.f;
        col[3] = color.GetBlue() * 255.f;
        SetColor(GetColor());
    });
}

menu_ctrl::button::button(const AnsiString& text, Control* parent, std::function<void()> on_click)
{
    const int x = g_menu.get_x_pos();
    const int w = parent->GetWidth() - x * 2;

    SetForeColor(OSHColor::FromARGB(255, 201, 201, 201));
    SetFont(menu_font());
    SetLocation(x, g_menu.get_y_pos());
    SetSize(w, 22);
    SetText(text);
    parent->AddControl(this);

    GetClickEvent() += OSHGui::ClickEventHandler([on_click](Control*)
    {
        if (on_click)
            on_click();
    });

    g_menu.push_y_pos(26);
}

static OSHGui::Misc::AnsiString multi_combo_summary(const std::vector<OSHGui::Misc::AnsiString>& items, uint32_t mask)
{
    OSHGui::Misc::AnsiString out;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if ((mask & (1u << i)) == 0)
            continue;
        if (!out.empty())
            out += ", ";
        out += items[i];
    }
    if (out.empty())
        out = "None";
    return out;
}

menu_ctrl::multi_combo::multi_combo(const AnsiString& text, const std::vector<AnsiString>& items, Control* parent, uint32_t* mask, int parent_width)
{
    SetLocation(parent_width / 2 - GetWidth() / 2 - 3, g_menu.get_y_pos() + 12);
    SetFont(menu_font());
    SetMaxShowItems(static_cast<int>(items.size()) + 1);

    auto* label = new OSHGui::Label();
    label->SetFont(menu_font());
    label->SetForeColor(OSHColor::FromRGB(201, 201, 201));
    label->SetLocation(GetLeft(), GetTop() - 12);
    label->SetText(text);
    parent->AddControl(label);

    parent->AddControl(this);

    auto refresh = [this, items, mask]()
    {
        Clear();
        for (size_t i = 0; i < items.size(); ++i)
        {
            const bool on = (*mask & (1u << i)) != 0;
            AddItem((on ? "[x] " : "[ ] ") + items[i]);
        }
        SetText(multi_combo_summary(items, *mask));
    };

    refresh();

    GetSelectedIndexChangedEvent() += OSHGui::SelectedIndexChangedEventHandler([this, items, mask, refresh](Control*)
    {
        const int idx = GetSelectedIndex();
        if (idx < 0 || idx >= static_cast<int>(items.size()))
            return;

        *mask ^= (1u << idx);
        refresh();
    });

    g_menu.push_y_pos(40);
}

menu_ctrl::checkbox_keybind::checkbox_keybind(const AnsiString& text, Control* parent, bool* value, int* key_value, int* mode_value, keybind::state_t* bind_state, int parent_width)
{
    const int y = g_menu.get_y_pos();
    const int x = g_menu.get_x_pos();

    auto* cb = new OSHGui::CheckBox();
    cb->SetBackColor(menu_accent());
    cb->SetFont(menu_font());
    cb->SetLocation(x, y);
    cb->SetText(text);
    cb->SetChecked(*value);
    parent->AddControl(cb);

    cb->GetCheckedChangedEvent() += OSHGui::CheckedChangedEventHandler([value](Control* sender)
    {
        *value = static_cast<OSHGui::CheckBox*>(sender)->GetChecked();
    });

    auto* mode_label = new OSHGui::Label();
    mode_label->SetFont(menu_font());
    mode_label->SetForeColor(OSHColor::FromRGB(160, 160, 160));
    mode_label->SetLocation(parent_width - 132, y + 2);
    mode_label->SetSize(34, 18);
    parent->AddControl(mode_label);

    auto* hk = new OSHGui::HotkeyControl();
    hk->SetFont(menu_font());
    hk->SetSize(90, 24);
    hk->SetLocation(parent_width - 96, y - 2);
    hk->SetHotkey(static_cast<OSHGui::Key>(*key_value));
    parent->AddControl(hk);

    auto refresh_mode = [mode_label, mode_value]()
    {
        mode_label->SetText(keybind::mode_label(keybind::parse_mode(*mode_value)));
    };

    refresh_mode();

    hk->GetHotkeyChangedEvent() += OSHGui::HotkeyChangedEventHandler([key_value](Control* sender)
    {
        const auto key = static_cast<OSHGui::HotkeyControl*>(sender)->GetHotkey();
        *key_value = static_cast<int>(key) & static_cast<int>(OSHGui::Key::KeyCode);
    });

    hk->GetHotkeyRightClickEvent() += OSHGui::HotkeyRightClickEventHandler([mode_value, bind_state, refresh_mode](Control*)
    {
        *mode_value = static_cast<int>(keybind::cycle_mode(keybind::parse_mode(*mode_value)));
        if (bind_state)
            keybind::reset_state(*bind_state);
        refresh_mode();
    });

    g_menu.push_y_pos(18);
}
