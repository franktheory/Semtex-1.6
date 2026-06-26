#pragma once

#include "../sdk/sdk.h"

extra_player_info_t* game_extra_info();

namespace game
{
    bool init();
    bool tick();
    void shutdown();

    client_state_t* cl();
    uintptr_t       cl_dll_funcs();
    void*           playermove_state();

    engine_studio_api_t* studio_api();
    engine_studio_api_t* client_studio_api();
    r_studio_interface_t* client_studio_iface();
    studio_model_renderer_t* client_studio_renderer_vtable();
    bool client_studio_chrome_api_ready();
    studiohdr_t** pstudiohdr();
    cl_entity_t*    cl_entity(int entity_index);

    int  local_slot();

    bool ready();
    bool movement_allowed();
    bool in_match();
    const char* last_error();
    float view_fov();
    float client_time();

    int pat_cl();
    int pat_studio_api();
    int pat_client_studio();
    int pat_studio_renderer();
    int pat_pstudiohdr();
    int pat_extra_info();
    int pat_cl_entities();
}
