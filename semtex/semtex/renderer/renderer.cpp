#include "renderer.h"
#include "../features/visuals.h"
#include "../features/aimbot.h"
#include "../game/world.h"
#include "../menu/menu_vars.h"

bool c_renderer::init_gl_context(HDC hdc)
{
    if (!hdc)
        return false;

    m_hdc = hdc;
    m_game_ctx = wglGetCurrentContext();
    if (!m_game_ctx)
        return false;

    m_gui_ctx = wglCreateContext(m_hdc);
    if (!m_gui_ctx)
        return false;

    if (!wglMakeCurrent(m_hdc, m_gui_ctx))
        return false;

    wglShareLists(m_gui_ctx, m_game_ctx);
    m_gl_ready = true;
    return true;
}

void c_renderer::bind_gui_context() const
{
    if (m_hdc && m_gui_ctx)
        wglMakeCurrent(m_hdc, m_gui_ctx);
}

void c_renderer::bind_game_context() const
{
    if (m_hdc && m_game_ctx)
        wglMakeCurrent(m_hdc, m_game_ctx);
}

void c_renderer::shutdown_gl()
{
    if (m_gui_ctx)
    {
        wglMakeCurrent(m_hdc, nullptr);
        wglDeleteContext(m_gui_ctx);
        m_gui_ctx = nullptr;
    }

    m_game_ctx = nullptr;
    m_hdc = nullptr;
    m_gl_ready = false;
}

void c_renderer::init()
{
    OSHGui::Application::Initialize(std::make_unique<OSHGui::Drawing::OpenGLRenderer>());

    auto font = OSHGui::Drawing::FontManager::LoadFont("Verdana", 7.f, true);
    m_fonts.push_back(font);

    get_instance()->SetDefaultFont(m_fonts[0]);
}

void c_renderer::begin()
{
    if (!get_instance())
        return;

    GLint vp[4]{};
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] > 0 && vp[3] > 0)
        get_renderer().SetDisplaySize(OSHGui::Drawing::SizeF(static_cast<float>(vp[2]), static_cast<float>(vp[3])));

    m_render_target = get_renderer().GetDefaultRenderTarget();
    m_geometry = get_renderer().CreateGeometryBuffer();
    get_renderer().BeginRendering();
}

void c_renderer::end() const
{
    if (!m_render_target || !m_geometry)
        return;

    m_render_target->Activate();
    m_render_target->Draw(*m_geometry);
    m_render_target->Deactivate();

    get_instance()->Render();
    get_instance()->GetRenderer().EndRendering();
}

void c_renderer::render_overlay()
{
    if (!m_gl_ready)
        return;

    const bool draw_visuals = menu_vars::visuals_enabled;
    const bool draw_aimbot  = menu_vars::aimbot_enabled && menu_vars::aimbot_fov_circle;

    if (!draw_visuals && !draw_aimbot)
        return;

    bind_game_context();
    world::capture_matrices();

    if (draw_visuals)
        visuals::draw_world_esp_gl();

    bind_gui_context();

    GLint prev_prog = 0;
    if (GLEW_VERSION_2_0)
        glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
    glUseProgram(0);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (auto* app = get_instance())
        app->GetRenderSurface().Invalidate();

    begin();

    if (!m_fonts.empty())
    {
        OSHGui::Drawing::Graphics gfx(*m_geometry);

        if (draw_visuals)
        {
            visuals::draw_box_esp(gfx);
            visuals::draw_skeleton_esp(gfx);
            visuals::draw_name_esp(gfx, m_fonts[0]);
            visuals::draw_weapon_esp(gfx, m_fonts[0]);
        }

        if (draw_aimbot)
            aimbot::draw_fov_circle(gfx);
    }

    end();

    if (draw_visuals)
        visuals::finish_frame_esp();

    if (prev_prog)
        glUseProgram(static_cast<GLuint>(prev_prog));

    bind_game_context();
}

void c_renderer::render_if_enabled()
{
    auto* app = get_instance();
    if (!app || !app->IsEnabled() || !m_gl_ready)
        return;

    bind_gui_context();

    GLint prev_prog = 0;
    if (GLEW_VERSION_2_0)
        glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
    glUseProgram(0);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    app->GetRenderSurface().Invalidate();

    begin();
    end();

    if (prev_prog)
        glUseProgram(static_cast<GLuint>(prev_prog));

    bind_game_context();
}

OSHGui::Application* c_renderer::get_instance() const
{
    return OSHGui::Application::InstancePtr();
}

OSHGui::Drawing::Renderer& c_renderer::get_renderer() const
{
    return get_instance()->GetRenderer();
}
