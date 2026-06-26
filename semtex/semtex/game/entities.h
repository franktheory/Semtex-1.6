#pragma once

#include "../sdk/sdk.h"
#include <unordered_map>

struct player_cache_t
{
    cl_entity_t*         ent = nullptr;
    entity_state_t*      state = nullptr;
    player_info_t*       info = nullptr;
    extra_player_info_t* extra = nullptr;
    int                  info_index = -1;

    void update_entity(cl_entity_t* entity);
    void try_update_entity(int expected_index, cl_entity_t* entity);
    void update_info(int index, player_info_t* pinfo);

    bool valid() const;
    bool entity_matches_slot() const;
    bool alive() const;
    bool is_local() const;
};

namespace entities
{
    void on_entity_update(cl_entity_t* ent);
    void refresh_player_info();
    void refresh_player_slot(int slot);

    const std::unordered_map<int, player_cache_t>& players();
    void clear();
}
