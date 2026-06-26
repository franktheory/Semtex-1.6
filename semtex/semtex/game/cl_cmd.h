#pragma once

#include "../sdk/usercmd.h"

namespace game
{
    usercmd_t* client_cmd();
    bool       cmd_is_client_state(usercmd_t* cmd);
    float*     punch_from_cmd(usercmd_t* cmd);
    bool       fn_in_client_dll(const void* fn);
    int        resolve_postruncmd_vtable_index(uintptr_t cl_funcs_table);
}
