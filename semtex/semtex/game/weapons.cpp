#include "weapons.h"

#include "../game/memory.h"
#include "../sdk/hl_types.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstring>

namespace
{
    struct model_name_view_t
    {
        char name[64];
    };

    static int local_weaponmodel()
    {
        const int slot = game::local_slot();
        if (slot < 0)
            return 0;

        cl_entity_t* ent = game::cl_entity(slot + 1);
        if (!ent)
            return 0;

        if (ent->curstate.weaponmodel)
            return ent->curstate.weaponmodel;

        if (ent->prevstate.weaponmodel)
            return ent->prevstate.weaponmodel;

        auto* api = game::client_studio_api();
        if (!api)
            api = game::studio_api();

        if (api && api->GetPlayerState)
        {
            __try
            {
                if (entity_state_t* st = api->GetPlayerState(slot))
                {
                    if (st->weaponmodel)
                        return st->weaponmodel;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        return 0;
    }

    static bool resolve_weapon_name(char* out, size_t out_size)
    {
        if (!out || out_size == 0)
            return false;

        out[0] = '\0';

        const int weaponmodel = local_weaponmodel();
        if (!weaponmodel)
            return false;

        auto* api = game::client_studio_api();
        if (!api)
            api = game::studio_api();

        if (!api || !api->GetModelByIndex)
            return false;

        model_t* mdl = nullptr;
        __try
        {
            mdl = api->GetModelByIndex(weaponmodel);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }

        if (!mdl)
            return false;

        const char* model_name = reinterpret_cast<model_name_view_t*>(mdl)->name;
        if (!model_name[0])
            return false;

        const char* weapon = std::strstr(model_name, "/p_");
        if (!weapon)
            weapon = std::strstr(model_name, "\\p_");
        if (!weapon)
            return false;

        weapon += 3;
        strncpy_s(out, out_size, weapon, _TRUNCATE);

        if (char* ext = std::strstr(out, ".mdl"))
            *ext = '\0';

        return out[0] != '\0';
    }

    static bool name_is(const char* name, const char* token)
    {
        return name && token && _stricmp(name, token) == 0;
    }

    static bool name_contains(const char* name, const char* token)
    {
        if (!name || !token)
            return false;

        return std::strstr(name, token) != nullptr;
    }
}

weapons::kind weapons::local_kind()
{
    char name[64]{};
    if (!resolve_weapon_name(name, sizeof(name)))
        return kind::unknown;

    if (name_is(name, "knife"))
        return kind::knife;

    if (name_is(name, "hegrenade") || name_is(name, "flashbang") || name_is(name, "smokegrenade"))
        return kind::grenade;

    if (name_is(name, "c4"))
        return kind::c4;

    if (name_contains(name, "grenade"))
        return kind::grenade;

    return kind::gun;
}

bool weapons::can_rage_shoot()
{
    const kind wpn = local_kind();
    return wpn == kind::gun || wpn == kind::unknown;
}
