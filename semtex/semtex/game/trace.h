#pragma once

#include "../sdk/hl_types.h"

namespace trace
{
    void bind_pmove(void* pm);
    bool line_clear(const Vector& from, const Vector& to, int target_entity = 0);
}
