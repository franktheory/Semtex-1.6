#include "menu.h"
#include "menu_controls.h"
#include "menu_vars.h"
#include "../config/config.h"
#include "../inc.h"
#include "../../shared/shared.h"

extern semtex_shared_t* g_shm();

static void menu_step(int step)
{
    auto s = g_shm();
    if (s)
        s->init_step = step;
}

namespace menu_layout
{
    constexpr int k_form_w = 600;
    constexpr int k_form_h = 480;
    constexpr int k_border_pad = 6;
    constexpr int k_caption_h = 44;
    constexpr int k_inset = 4;
    constexpr int k_tab_button_h = 25;
    constexpr int k_container_y = k_caption_h - 7;
    constexpr int k_inner_w = k_form_w - k_border_pad * 2;
    constexpr int k_container_h = k_form_h - k_container_y - k_border_pad;
    constexpr int k_page_count = 5;
    constexpr int k_tab_area_w = k_inner_w - k_inset * 2;
    constexpr int k_tab_btn_w = k_tab_area_w / k_page_count;
    constexpr int k_tab_w = k_tab_btn_w * k_page_count;
    constexpr int k_tab_x = k_inset + (k_tab_area_w - k_tab_w) / 2;
    constexpr int k_tab_h = k_container_h - k_inset;
    constexpr int k_tab_page_h = k_tab_h - k_tab_button_h - 4;
    constexpr int k_groupbox_h = k_tab_page_h - 10;
}

void c_main_form::init_tabs()
{
    m_tab_control = new OSHGui::TabControl();
    m_tab_control->SetFont(g_renderer.get_instance()->GetDefaultFont());

    m_pages.resize(PAGE_MAX);
    for (auto& page : m_pages)
        page = std::make_shared<OSHGui::TabPage>();

    m_pages[PAGE_LEGITBOT]->SetText("Legitbot");
    m_pages[PAGE_RAGEBOT]->SetText("Ragebot");
    m_pages[PAGE_VISUALS]->SetText("Visuals");
    m_pages[PAGE_MISC]->SetText("Miscellaneous");
    m_pages[PAGE_CONFIG]->SetText("Configuration");

    m_tab_control->SetSize(menu_layout::k_tab_w, menu_layout::k_tab_h);
    m_tab_control->SetBackColor(OSHGui::Drawing::Color::FromARGB(70, 0, 0, 0));
    m_tab_control->SetLocation(menu_layout::k_tab_x, 0);
    m_tab_control->SetButtonWidth(menu_layout::k_tab_btn_w);

    for (auto& page : m_pages)
        m_tab_control->AddTabPage(page.get());

    m_tab_control->SetSize(menu_layout::k_tab_w, menu_layout::k_tab_h);

    AddControl(m_tab_control);
}

void c_main_form::legitbot_tab()
{
    auto* page = m_pages[PAGE_LEGITBOT].get();
    auto* aim_box = new menu_ctrl::groupbox("Aimbot", 17, 6, 260, menu_layout::k_groupbox_h);

    page->AddControl(aim_box);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    new menu_ctrl::checkbox_keybind("Enable aimbot", aim_box, &menu_vars::aimbot_enabled, &menu_vars::aimbot_key, &menu_vars::aimbot_key_mode, &menu_vars::aimbot_key_state, 260);
    new menu_ctrl::slider("FOV", aim_box, 1.f, 30.f, &menu_vars::aimbot_fov, 5, " deg");
    new menu_ctrl::checkbox("Show FOV circle", aim_box, &menu_vars::aimbot_fov_circle);
    new menu_ctrl::slider("Smoothing", aim_box, 1.f, 30.f, &menu_vars::aimbot_smooth, 0, 1.f, "");

    const std::vector<OSHGui::Misc::AnsiString> hitboxes = {
        "Head", "Neck", "Chest", "Stomach", "Pelvis"
    };
    new menu_ctrl::multi_combo("Hitboxes", hitboxes, aim_box, &menu_vars::aimbot_hitboxes, 260);

    auto* recoil_box = new menu_ctrl::groupbox("Recoil", 290, 6, 260, 60);
    page->AddControl(recoil_box);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    new menu_ctrl::checkbox("No recoil", recoil_box, &menu_vars::norecoil_enabled);
}

void c_main_form::ragebot_tab()
{
    auto* page = m_pages[PAGE_RAGEBOT].get();
    auto* aa_box = new menu_ctrl::groupbox("Anti-aim", 17, 6, 260, 170);

    page->AddControl(aa_box);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    new menu_ctrl::checkbox("Enable anti-aim", aa_box, &menu_vars::antiaim_enabled);

    const std::vector<OSHGui::Misc::AnsiString> yaw_modes = { "Spin", "180" };
    new menu_ctrl::combo("Yaw", yaw_modes, aa_box, 2, &menu_vars::antiaim_yaw_mode, 260);

    const std::vector<OSHGui::Misc::AnsiString> pitch_modes = { "Up", "Down", "Zero" };
    new menu_ctrl::combo("Pitch", pitch_modes, aa_box, 3, &menu_vars::antiaim_pitch_mode, 260);

    auto* spin_slider = new menu_ctrl::slider("Spin speed", aa_box, 1.f, 100.f, &menu_vars::antiaim_spin_speed, 0, 30.f, "");
    spin_slider->SetVisible(menu_vars::antiaim_yaw_mode == 0);

    auto* spin_timer = new OSHGui::Timer();
    spin_timer->SetInterval(50);
    spin_timer->Start();
    aa_box->AddControl(spin_timer);

    spin_timer->GetTickEvent() += OSHGui::TickEventHandler([spin_slider](Control*)
    {
        spin_slider->SetVisible(menu_vars::antiaim_yaw_mode == 0);
    });

    auto* rage_box = new menu_ctrl::groupbox("Aimbot", 290, 6, 260, menu_layout::k_groupbox_h);

    page->AddControl(rage_box);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    new menu_ctrl::checkbox_keybind("Enable ragebot", rage_box, &menu_vars::ragebot_enabled, &menu_vars::ragebot_key, &menu_vars::ragebot_key_mode, &menu_vars::ragebot_key_state, 260);
    new menu_ctrl::checkbox("Auto fire", rage_box, &menu_vars::ragebot_auto_fire);
    new menu_ctrl::checkbox("Auto stop", rage_box, &menu_vars::ragebot_auto_stop);
    new menu_ctrl::checkbox("Autowall check", rage_box, &menu_vars::ragebot_autowall);

    const std::vector<OSHGui::Misc::AnsiString> rage_hitboxes = {
        "Head", "Neck", "Chest", "Stomach", "Pelvis"
    };
    new menu_ctrl::multi_combo("Hitboxes", rage_hitboxes, rage_box, &menu_vars::ragebot_hitboxes, 260);
}

void c_main_form::visuals_tab()
{
    auto* page = m_pages[PAGE_VISUALS].get();
    auto* esp_box = new menu_ctrl::groupbox("Player ESP", 17, 6, 260, menu_layout::k_groupbox_h);
    auto* chams_box = new menu_ctrl::groupbox("Chams", 290, 6, 260, 120);

    page->AddControl(esp_box);
    page->AddControl(chams_box);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    new menu_ctrl::checkbox("Enable visuals", esp_box, &menu_vars::visuals_enabled);
    new menu_ctrl::checkbox("Box ESP",        esp_box, &menu_vars::box_esp);
    new menu_ctrl::checkbox("Name ESP",       esp_box, &menu_vars::name_esp);
    new menu_ctrl::checkbox("Weapon ESP",     esp_box, &menu_vars::weapon_esp);
    new menu_ctrl::checkbox("Skeleton ESP",   esp_box, &menu_vars::skeleton_esp);
    new menu_ctrl::checkbox("Show teammates", esp_box, &menu_vars::teammates);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    auto* chams_cb = new menu_ctrl::checkbox("Chams", chams_box, &menu_vars::chams);
    new menu_ctrl::color_picker(chams_box, chams_cb, menu_vars::chams_vis_color);

    auto* chams_xqz_cb = new menu_ctrl::checkbox("Chams XQZ", chams_box, &menu_vars::chams_xqz);
    new menu_ctrl::color_picker(chams_box, chams_xqz_cb, menu_vars::chams_xqz_color);

    auto* world_box = new menu_ctrl::groupbox("World", 290, 130, 260, 100);
    page->AddControl(world_box);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    new menu_ctrl::checkbox_keybind("Third person", world_box, &menu_vars::thirdperson_enabled, &menu_vars::thirdperson_key, &menu_vars::thirdperson_key_mode, &menu_vars::thirdperson_key_state, 260);

    const std::vector<OSHGui::Misc::AnsiString> removals = { "Visual recoil" };
    new menu_ctrl::multi_combo("Removals", removals, world_box, &menu_vars::visual_removals, 260);
}

void c_main_form::misc_tab()
{
    auto* page = m_pages[PAGE_MISC].get();
    auto* movement_box = new menu_ctrl::groupbox("Movement", 17, 6, 260, 80);

    page->AddControl(movement_box);

    g_menu.set_x_pos(19);
    g_menu.set_y_pos(10);

    new menu_ctrl::checkbox("Bunny hop", movement_box, &menu_vars::bhop_enabled);
}

std::string c_main_form::selected_profile_name() const
{
    if (!m_profile_list)
        return {};

    const int index = m_profile_list->GetSelectedIndex();
    if (index < 0)
        return {};

    auto* item = m_profile_list->GetItem(index);
    if (!item)
        return {};

    return item->GetItemText();
}

void c_main_form::refresh_profile_list()
{
    if (!m_profile_list)
        return;

    const auto profiles = config::list_profiles();
    m_profile_list->Clear();

    for (const auto& profile : profiles)
        m_profile_list->AddItem(profile.c_str());

    if (!profiles.empty())
    {
        if (m_profile_selected >= 0 && m_profile_selected < static_cast<int>(profiles.size()))
            m_profile_list->SetSelectedIndex(m_profile_selected);
        else
            m_profile_list->SetSelectedIndex(0);
    }
    else
    {
        m_profile_selected = 0;
    }
}

void c_main_form::config_tab()
{
    auto* page = m_pages[PAGE_CONFIG].get();
    auto* profiles_box = new menu_ctrl::groupbox("Profiles", 17, 6, 260, menu_layout::k_groupbox_h);
    auto* actions_box = new menu_ctrl::groupbox("Configurations", 290, 6, 260, menu_layout::k_groupbox_h);

    page->AddControl(profiles_box);
    page->AddControl(actions_box);

    g_menu.set_x_pos(12);
    g_menu.set_y_pos(18);

    const int list_h = menu_layout::k_groupbox_h - 30;
    auto* list = new menu_ctrl::listbox("", profiles_box, {}, &m_profile_selected, list_h / 18);
    list->SetLocation(12, 18);
    list->SetSize(236, list_h);
    m_profile_list = list;

    m_profile_list->GetSelectedIndexChangedEvent() += OSHGui::SelectedIndexChangedEventHandler([this](Control*)
    {
        if (m_profile_list)
            m_profile_selected = m_profile_list->GetSelectedIndex();

        if (m_profile_name)
        {
            const std::string name = selected_profile_name();
            if (!name.empty())
                m_profile_name->SetText(name.c_str());
        }
    });

    refresh_profile_list();

    g_menu.set_x_pos(12);
    g_menu.set_y_pos(18);

    auto* name_field = new menu_ctrl::text_field("", actions_box, 260);
    m_profile_name = name_field;

    new menu_ctrl::button("New", actions_box, [this]()
    {
        if (!m_profile_name)
            return;

        std::string name = m_profile_name->GetText();
        if (!config::sanitize_name(name))
        {
            OSHGui::MessageBox::Show("Enter a valid profile name.", "Configuration");
            return;
        }

        if (!config::create(name.c_str()))
        {
            OSHGui::MessageBox::Show("Could not create profile. It may already exist.", "Configuration");
            return;
        }

        refresh_profile_list();

        if (m_profile_list)
            m_profile_list->SetSelectedItem(name.c_str());
    });

    new menu_ctrl::button("Save", actions_box, [this]()
    {
        const std::string name = selected_profile_name();
        if (name.empty())
        {
            OSHGui::MessageBox::Show("Select a profile to save.", "Configuration");
            return;
        }

        if (!config::save(name.c_str()))
            OSHGui::MessageBox::Show("Failed to save profile.", "Configuration");
    });

    new menu_ctrl::button("Load", actions_box, [this]()
    {
        const std::string name = selected_profile_name();
        if (name.empty())
        {
            OSHGui::MessageBox::Show("Select a profile to load.", "Configuration");
            return;
        }

        if (!config::load(name.c_str()))
        {
            OSHGui::MessageBox::Show("Failed to load profile.", "Configuration");
            return;
        }

        OSHGui::MessageBox::Show("Profile loaded. Reinject to refresh menu controls.", "Configuration");
    });

    new menu_ctrl::button("Delete", actions_box, [this]()
    {
        const std::string name = selected_profile_name();
        if (name.empty())
        {
            OSHGui::MessageBox::Show("Select a profile to delete.", "Configuration");
            return;
        }

        OSHGui::MessageBox::ShowDialog(
            "Are you sure you want to delete the selected profile?",
            "Configuration",
            OSHGui::MessageBoxButtons::YesNo,
            [this, name](OSHGui::DialogResult result)
            {
                if (result != OSHGui::DialogResult::Yes)
                    return;

                if (!config::remove(name.c_str()))
                {
                    OSHGui::MessageBox::Show("Failed to delete profile.", "Configuration");
                    return;
                }

                refresh_profile_list();
            });
    });
}

void c_main_form::init_component()
{
    SetText("semtex");
    SetSize(600, 480);
    SetLocation(100, 100);
    SetBackColor(OSHGui::Drawing::Color::FromARGB(70, 0, 0, 0));

    init_tabs();
    legitbot_tab();
    ragebot_tab();
    visuals_tab();
    misc_tab();
    config_tab();
}

void c_menu::init()
{
    menu_step(1);

    g_renderer.get_instance()->SetPrimaryColor(OSHGui::Drawing::Color::FromARGB(255, 210, 45, 45));

    m_form = std::static_pointer_cast<OSHGui::Form>(std::make_shared<c_main_form>());
    menu_step(2);

    m_form->SetFont(g_renderer.get_instance()->GetDefaultFont());
    menu_step(3);

    g_renderer.get_instance()->Run(m_form);
    menu_step(4);

    g_renderer.get_instance()->Disable();
    menu_step(5);
}
