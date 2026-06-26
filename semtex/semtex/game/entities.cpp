#include "entities.h"
#include "memory.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

extern extra_player_info_t* game_extra_info();

static std::unordered_map<int, player_cache_t> s_players;

static player_info_t* query_player_info(int slot)
{
    if (slot < 0)
        return nullptr;

    player_info_t* pinfo = nullptr;

    if (auto* api = game::studio_api(); api && api->PlayerInfo)
    {
        __try
        {
            pinfo = api->PlayerInfo(slot);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            pinfo = nullptr;
        }
    }

    if ((!pinfo || !pinfo->name[0]) && game::cl())
        pinfo = &game::cl_player(game::cl(), slot);

    if (!pinfo || !pinfo->name[0])
        return nullptr;

    return pinfo;
}

static entity_state_t* query_player_state(int slot)
{
    const int entity_num = slot + 1;
    auto* api = game::studio_api();
    if (!api || !api->GetPlayerState)
        return nullptr;

    __try
    {
        entity_state_t* st = api->GetPlayerState(entity_num);
        if (st && st->number == entity_num && !st->origin.IsZero())
            return st;

        st = api->GetPlayerState(slot);
        if (st && st->number == entity_num && !st->origin.IsZero())
            return st;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return nullptr;
}

void player_cache_t::update_entity(cl_entity_t* entity)
{
    ent = entity;
}

void player_cache_t::try_update_entity(int expected_index, cl_entity_t* entity)
{
    if (!entity || !entity->player || entity->index != expected_index)
        return;
    ent = entity;
}

bool player_cache_t::entity_matches_slot() const
{
    if (!ent || info_index < 0)
        return false;
    return ent->player && ent->index == info_index + 1;
}

void player_cache_t::update_info(int index, player_info_t* pinfo)
{
    info       = pinfo;
    info_index = index;
    auto* base = game_extra_info();
    extra      = base ? &base[index + 1] : nullptr;
}

bool player_cache_t::valid() const
{
    return info && info->name[0] != '\0';
}

bool player_cache_t::alive() const
{
    if (extra)
    {
        if (extra->health > 0)
            return true;
        if (extra->dead)
            return false;
    }
    return true;
}

bool player_cache_t::is_local() const
{
    if (info_index < 0)
        return false;
    return info_index == game::local_slot();
}

void entities::on_entity_update(cl_entity_t* ent)
{
    if (!ent || !ent->player || ent->index <= 0)
        return;

    auto& cache = s_players[ent->index];
    cache.try_update_entity(ent->index, ent);

    const int slot = ent->index - 1;
    if (cache.info_index != slot)
        refresh_player_slot(slot);
}

void entities::refresh_player_slot(int slot)
{
    if (!game::in_match() || slot < 0)
        return;

    const int key = slot + 1;
    player_info_t* pinfo = query_player_info(slot);

    if (!pinfo)
    {
        s_players.erase(key);
        return;
    }

    auto& cache = s_players[key];
    cache.update_info(slot, pinfo);
    cache.state = query_player_state(slot);

    if (!cache.entity_matches_slot())
    {
        if (cl_entity_t* ent = game::cl_entity(key))
            cache.try_update_entity(key, ent);
    }
}

void entities::refresh_player_info()
{
    if (!game::in_match())
        return;

    for (int i = 0; i < MAX_CLIENTS; ++i)
        refresh_player_slot(i);
}

const std::unordered_map<int, player_cache_t>& entities::players()
{
    return s_players;
}

void entities::clear()
{
    s_players.clear();
}
