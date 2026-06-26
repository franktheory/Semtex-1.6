#pragma once

struct ref_params_t;
struct usercmd_t;
struct semtex_shared_t;

namespace norecoil
{
    void on_createmove(usercmd_t* cmd);
    void on_calc_ref_pre(ref_params_t* params);
    void on_calc_ref_post(ref_params_t* params);
    void update_debug_status();
}
