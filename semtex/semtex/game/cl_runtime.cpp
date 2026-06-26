#include "cl_runtime.h"

#include "../sdk/pm_types.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>

namespace cl_runtime
{
    std::ptrdiff_t viewangles = cl_off::viewangles;
    std::ptrdiff_t cmd        = cl_off::cmd;
    std::ptrdiff_t punchangle = cl_off::punchangle;

    static bool s_calibrated = false;

    static void set_layout(std::ptrdiff_t va_off)
    {
        viewangles = va_off;
        cmd        = va_off - static_cast<std::ptrdiff_t>(sizeof(usercmd_t));
        punchangle = va_off + 12;
        s_calibrated = true;
    }

    void reset()
    {
        viewangles = cl_off::viewangles;
        cmd        = cl_off::cmd;
        punchangle = cl_off::punchangle;
        s_calibrated = false;
    }

    bool ready()
    {
        return s_calibrated;
    }

    bool read_punch_global(float out[3])
    {
        out[0] = 0.f;
        out[1] = 0.f;
        out[2] = 0.f;

        HMODULE client = GetModuleHandleA("client.dll");
        if (!client)
            return false;

        __try
        {
            auto* pm_slot = reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uintptr_t>(client) + k_playermove_ptr_rva);
            const uintptr_t pm = *pm_slot;
            if (!pm)
                return false;

            const float* punch = reinterpret_cast<const float*>(pm + pm_off::punchangle);
            out[0] = punch[0];
            out[1] = punch[1];
            out[2] = punch[2];
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool calibrate_from_client_globals(client_state_t* cl)
    {
        if (!cl || s_calibrated)
            return false;

        HMODULE client = GetModuleHandleA("client.dll");
        if (!client)
            return false;

        __try
        {
            const float* globals = reinterpret_cast<const float*>(
                reinterpret_cast<uintptr_t>(client) + k_client_viewangles_rva);

            const auto* base = reinterpret_cast<const char*>(cl);
            for (std::ptrdiff_t off = 0x1000; off < 0x1C0000; off += 4)
            {
                const float* va = reinterpret_cast<const float*>(base + off);
                if (std::fabs(va[0] - globals[0]) > 0.2f
                    || std::fabs(va[1] - globals[1]) > 0.2f
                    || std::fabs(va[2] - globals[2]) > 0.2f)
                    continue;

                const auto* cmd_candidate = reinterpret_cast<const usercmd_t*>(
                    base + off - static_cast<std::ptrdiff_t>(sizeof(usercmd_t)));
                if (std::fabs(cmd_candidate->viewangles[0] - globals[0]) > 0.2f
                    || std::fabs(cmd_candidate->viewangles[1] - globals[1]) > 0.2f
                    || std::fabs(cmd_candidate->viewangles[2] - globals[2]) > 0.2f)
                    continue;

                set_layout(off);
                return true;
            }
            return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool angles_close(const float a[3], const float b[3], float eps)
    {
        return std::fabs(a[0] - b[0]) <= eps
            && std::fabs(a[1] - b[1]) <= eps
            && std::fabs(a[2] - b[2]) <= eps;
    }

    void calibrate(client_state_t* cl, const usercmd_t* ucmd)
    {
        if (!cl || !ucmd || s_calibrated)
            return;

        if (calibrate_from_client_globals(cl))
            return;

        const float target[3] = { ucmd->viewangles[0], ucmd->viewangles[1], ucmd->viewangles[2] };
        auto* base = reinterpret_cast<char*>(cl);

        for (std::ptrdiff_t off = 0x1000; off < 0x1C0000; off += 4)
        {
            __try
            {
                const float* va = reinterpret_cast<const float*>(base + off);
                if (!angles_close(va, target, 0.2f))
                    continue;

                const auto* cmd_candidate = reinterpret_cast<const usercmd_t*>(
                    base + off - static_cast<std::ptrdiff_t>(sizeof(usercmd_t)));
                if (!angles_close(cmd_candidate->viewangles, target, 0.2f))
                    continue;

                set_layout(off);
                return;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
}
