#pragma once

#include "../sdk/sdk.h"

namespace world
{
    void capture_matrices();
    bool has_screen();
    bool to_screen(const Vector& world, Vector2D& screen);
    Vector2D screen_size();
}
