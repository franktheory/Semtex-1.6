#pragma once

struct ref_params_t;

namespace thirdperson
{
    bool init_hooks();
    void shutdown_hooks();

    void on_calc_ref_post(ref_params_t* params);
    bool active();
}
