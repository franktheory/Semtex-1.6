#pragma once

struct semtex_shared_t;

namespace movement
{
    bool init_hooks();
    void shutdown_hooks();
    bool hooks_attached();
    void* cached_pmove();
    void update_debug_status(semtex_shared_t* shm);
}
