#include "world.h"
#include "memory.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>

static GLdouble s_model[16]{};
static GLdouble s_proj[16]{};
static GLint    s_viewport[4]{};
static bool     s_has_matrices = false;

static float dot3(const float axis[3], const Vector& v)
{
    return axis[0] * v.x + axis[1] * v.y + axis[2] * v.z;
}

static bool to_screen_glu(const Vector& world_pos, Vector2D& screen)
{
    if (!s_has_matrices)
        return false;

    GLdouble win_x = 0.0;
    GLdouble win_y = 0.0;
    GLdouble win_z = 0.0;

    if (!gluProject(world_pos.x, world_pos.y, world_pos.z, s_model, s_proj, s_viewport, &win_x, &win_y, &win_z))
        return false;

    if (win_z < 0.0 || win_z > 1.0)
        return false;

    screen.x = static_cast<float>(win_x);
    screen.y = static_cast<float>(s_viewport[3] - win_y);
    return true;
}

static bool to_screen_engine(const Vector& world_pos, Vector2D& screen)
{
    auto* api = game::studio_api();
    if (!api || !api->GetViewInfo || !s_has_matrices)
        return false;

    float org[3]{}, up[3]{}, right[3]{}, vpn[3]{};
    api->GetViewInfo(org, up, right, vpn);

    const Vector delta(world_pos.x - org[0], world_pos.y - org[1], world_pos.z - org[2]);
    const float z = dot3(vpn, delta);
    if (z < 0.1f)
        return false;

    const float x = dot3(right, delta);
    const float y = dot3(up, delta);

    const float width  = static_cast<float>(s_viewport[2]);
    const float height = static_cast<float>(s_viewport[3]);
    if (width <= 0.f || height <= 0.f)
        return false;

    const float fov      = game::view_fov();
    const float tan_half = tanf(fov * 3.14159265f / 360.f);
    const float aspect   = width / height;

    const float nx = x / (z * tan_half * aspect);
    const float ny = y / (z * tan_half);

    if (nx <= -1.f || nx >= 1.f || ny <= -1.f || ny >= 1.f)
        return false;

    screen.x = (nx * 0.5f + 0.5f) * width;
    screen.y = (-ny * 0.5f + 0.5f) * height;
    return true;
}

void world::capture_matrices()
{
    glGetDoublev(GL_MODELVIEW_MATRIX, s_model);
    glGetDoublev(GL_PROJECTION_MATRIX, s_proj);
    glGetIntegerv(GL_VIEWPORT, s_viewport);
    s_has_matrices = s_viewport[2] > 0 && s_viewport[3] > 0;
}

bool world::has_screen()
{
    return s_has_matrices && s_viewport[2] > 0 && s_viewport[3] > 0;
}

bool world::to_screen(const Vector& world_pos, Vector2D& screen)
{
    if (to_screen_glu(world_pos, screen))
        return true;

    return to_screen_engine(world_pos, screen);
}

Vector2D world::screen_size()
{
    return { static_cast<float>(s_viewport[2]), static_cast<float>(s_viewport[3]) };
}
