#pragma once

#include "hl_types.h"

constexpr int PM_STUDIO_IGNORE = 0x00000001;
constexpr int PM_GLASS_IGNORE  = 0x00000004;

struct pmtrace_t
{
    qboolean allsolid;
    qboolean startsolid;
    qboolean inopen;
    qboolean inwater;
    float    fraction;
    float    endpos[3];
    float    plane_normal[3];
    float    plane_dist;
    int      ent;
    float    deltavelocity[3];
    int      hitgroup;
};
