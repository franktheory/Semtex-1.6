#include "trace.h"

#include "memory.h"

#include "../sdk/engine_off.h"
#include "../sdk/event_api.h"
#include "../sdk/hl_types.h"
#include "../sdk/pmtrace.h"
#include "../sdk/pm_types.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

namespace
{
    using pm_trace_line_t = pmtrace_t*(__cdecl*)(float* start, float* end, int flags, int usehull, int ignore_pe);
    using get_max_clients_t = int(__cdecl*)();

    event_api_t*    s_event_api  = nullptr;
    pm_trace_line_t s_trace_line = nullptr;
    int             s_usehull    = 2;

    static bool ptr_in_module(uintptr_t addr, HMODULE mod)
    {
        if (!mod || !addr)
            return false;

        MODULEINFO info{};
        if (!GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info)))
            return false;

        const uintptr_t base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
        return addr >= base && addr < base + info.SizeOfImage;
    }

    static bool looks_like_code_ptr(void* fn)
    {
        if (!fn)
            return false;

        const uintptr_t addr = reinterpret_cast<uintptr_t>(fn);
        return ptr_in_module(addr, GetModuleHandleA("hw.dll"))
            || ptr_in_module(addr, GetModuleHandleA("sw.dll"))
            || ptr_in_module(addr, GetModuleHandleA("client.dll"));
    }

    static int pm_usehull(void* pm)
    {
        __try
        {
            return *reinterpret_cast<int*>(static_cast<char*>(pm) + 188);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 2;
        }
    }

    static bool validate_engine(void* eng)
    {
        if (!eng)
            return false;

        __try
        {
            auto fn = *reinterpret_cast<get_max_clients_t*>(
                static_cast<char*>(eng) + engine_off::GetMaxClients);
            if (!looks_like_code_ptr(reinterpret_cast<void*>(fn)))
                return false;

            const int max_clients = fn();
            return max_clients >= 8 && max_clients <= 64;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool validate_event_api(event_api_t* api)
    {
        if (!api)
            return false;

        if (!looks_like_code_ptr(reinterpret_cast<void*>(api->EV_PlayerTrace)))
            return false;
        if (!looks_like_code_ptr(reinterpret_cast<void*>(api->EV_IndexFromTrace)))
            return false;

        __try
        {
            float start[3] = { 0.f, 0.f, 100.f };
            float end[3]   = { 64.f, 0.f, 100.f };
            pmtrace_t tr{};

            if (api->EV_SetTraceHull)
                api->EV_SetTraceHull(2);

            api->EV_PlayerTrace(start, end, PM_GLASS_IGNORE, -1, &tr);
            api->EV_IndexFromTrace(&tr);
            return tr.fraction >= 0.f && tr.fraction <= 1.f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool validate_trace_line(pm_trace_line_t fn)
    {
        if (!fn)
            return false;

        __try
        {
            float start[3] = { 0.f, 0.f, 100.f };
            float end[3]   = { 64.f, 0.f, 100.f };

            pmtrace_t* tr = fn(start, end, 1, 2, -1);
            return tr && tr->fraction >= 0.f && tr->fraction <= 1.f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static event_api_t* locate_event_api(void* eng)
    {
        __try
        {
            auto* api = *reinterpret_cast<event_api_t**>(
                static_cast<char*>(eng) + engine_off::pEventAPI);
            if (validate_event_api(api))
                return api;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        auto* slots = reinterpret_cast<void**>(eng);
        for (int i = 75; i <= 95; ++i)
        {
            __try
            {
                auto* api = reinterpret_cast<event_api_t*>(slots[i]);
                if (validate_event_api(api))
                    return api;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        return nullptr;
    }

    static bool try_bind_engine(void* eng)
    {
        if (!validate_engine(eng))
            return false;

        s_event_api = locate_event_api(eng);

        __try
        {
            auto fn = *reinterpret_cast<pm_trace_line_t*>(
                static_cast<char*>(eng) + engine_off::PM_TraceLine);
            if (looks_like_code_ptr(reinterpret_cast<void*>(fn)) && validate_trace_line(fn))
                s_trace_line = fn;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        return s_event_api != nullptr || s_trace_line != nullptr;
    }

    static void bind_engine_trace()
    {
        if (s_event_api || s_trace_line)
            return;

        const uintptr_t cl_table = game::cl_dll_funcs();
        if (!cl_table)
            return;

        const auto* init_fn = reinterpret_cast<const uint8_t*>(
            *reinterpret_cast<uintptr_t*>(cl_table));
        if (!init_fn)
            return;

        static const int k_engine_ptr_offs[] = { 0x22, 0x1C, 0x1D, 0x37, 0x18, 0x1A, 0x2A, 0x30 };

        for (const int off : k_engine_ptr_offs)
        {
            __try
            {
                void* eng = *reinterpret_cast<void**>(const_cast<uint8_t*>(init_fn + off));
                if (try_bind_engine(eng))
                    return;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        for (int off = 0x10; off < 0x80; ++off)
        {
            __try
            {
                void* eng = *reinterpret_cast<void**>(const_cast<uint8_t*>(init_fn + off));
                if (try_bind_engine(eng))
                    return;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }

    static bool trace_hit_entity(int ent, int target_entity)
    {
        if (target_entity <= 0 || ent <= 0)
            return false;

        return ent == target_entity || ent == target_entity - 1;
    }

    static void prepare_player_trace()
    {
        if (!s_event_api)
            return;

        if (s_event_api->EV_SetUpPlayerPrediction)
            s_event_api->EV_SetUpPlayerPrediction(1, 1);

        if (s_event_api->EV_SetSolidPlayers)
            s_event_api->EV_SetSolidPlayers(-1);

        if (s_event_api->EV_SetTraceHull)
            s_event_api->EV_SetTraceHull(2);
    }

    static bool line_clear_event(const Vector& from, const Vector& to, int target_entity)
    {
        float start[3] = { from.x, from.y, from.z };
        float end[3]   = { to.x, to.y, to.z };

        pmtrace_t tr{};
        prepare_player_trace();
        s_event_api->EV_PlayerTrace(start, end, PM_GLASS_IGNORE, -1, &tr);

        const int hit = s_event_api->EV_IndexFromTrace(&tr);
        if (trace_hit_entity(hit, target_entity))
            return true;

        return trace_hit_entity(tr.ent, target_entity);
    }

    static bool line_clear_pm(const Vector& from, const Vector& to, int target_entity)
    {
        float start[3] = { from.x, from.y, from.z };
        float end[3]   = { to.x, to.y, to.z };

        pmtrace_t* tr = s_trace_line(start, end, 1, s_usehull, -1);
        if (!tr)
            return false;

        if (tr->startsolid || tr->allsolid)
            return false;

        if (trace_hit_entity(tr->ent, target_entity))
            return true;

        return false;
    }
}

void trace::bind_pmove(void* pm)
{
    bind_engine_trace();

    if (!pm)
        return;

    __try
    {
        s_usehull = pm_usehull(pm);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        s_usehull = 2;
    }
}

bool trace::line_clear(const Vector& from, const Vector& to, int target_entity)
{
    if (target_entity <= 0)
        return false;

    __try
    {
        if (s_event_api && s_event_api->EV_PlayerTrace && s_event_api->EV_IndexFromTrace)
            return line_clear_event(from, to, target_entity);

        if (s_trace_line)
            return line_clear_pm(from, to, target_entity);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return true;
}
