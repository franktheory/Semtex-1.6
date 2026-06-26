#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <memory>
#include <vector>
#include <GL/glew.h>
#include <GL/gl.h>
#include "../../oshgui/OSHGui.hpp"
#include "../../oshgui/Drawing/OpenGL/OpenGLRenderer.hpp"
#include "../../oshgui/Drawing/FontManager.hpp"
#include "../../oshgui/Drawing/FreeTypeFont.hpp"
#include "../../oshgui/Drawing/RenderTarget.hpp"

class c_renderer
{
public:
    OSHGui::Drawing::RenderTargetPtr m_render_target;
    OSHGui::Drawing::GeometryBufferPtr m_geometry;
    std::vector<OSHGui::Drawing::FontPtr> m_fonts;

    HDC   m_hdc      = nullptr;
    HGLRC m_game_ctx = nullptr;
    HGLRC m_gui_ctx  = nullptr;
    bool  m_gl_ready = false;

    c_renderer() = default;

    bool init_gl_context(HDC hdc);
    void bind_gui_context() const;
    void bind_game_context() const;
    void shutdown_gl();

    void init();
    void begin();
    void end() const;
    void render_overlay();
    void render_if_enabled();

    OSHGui::Application* get_instance() const;
    OSHGui::Drawing::Renderer& get_renderer() const;
};
