#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <memory>
#include <string>
#include <vector>
#include "../../oshgui/OSHGui.hpp"

class c_main_form : public OSHGui::Form
{
private:
    enum Pages
    {
        PAGE_LEGITBOT,
        PAGE_RAGEBOT,
        PAGE_VISUALS,
        PAGE_MISC,
        PAGE_CONFIG,
        PAGE_MAX
    };

    std::vector<std::shared_ptr<OSHGui::TabPage>> m_pages;
    OSHGui::TabControl* m_tab_control = nullptr;

    void init_component();
    void init_tabs();
    void legitbot_tab();
    void ragebot_tab();
    void visuals_tab();
    void misc_tab();
    void config_tab();
    void refresh_profile_list();

    OSHGui::ListBox* m_profile_list = nullptr;
    OSHGui::TextBox* m_profile_name = nullptr;
    int m_profile_selected = 0;

    std::string selected_profile_name() const;

public:
    c_main_form() { init_component(); }
};

class c_menu
{
public:
    std::shared_ptr<OSHGui::Form> m_form;

    int m_control_x = 27;
    int m_control_y = 10;

    c_menu() = default;
    void init();

    void set_x_pos(int x) { m_control_x = x; }
    void set_y_pos(int y) { m_control_y = y; }
    void push_y_pos(int y) { m_control_y += y; }
    int  get_x_pos() const { return m_control_x; }
    int  get_y_pos() const { return m_control_y; }
};

extern c_menu g_menu;
