#include "offset_dump.h"

#include "../semtex/game/pattern_defs.h"
#include "../semtex/sdk/sdk.h"
#include "../semtex/util/pattern.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    FILE* g_file = nullptr;

    size_t module_image_size(HMODULE mod)
    {
        if (!mod)
            return 0;
        const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
        const auto nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
        return nt->OptionalHeader.SizeOfImage;
    }

    bool ptr_in_module(uintptr_t address, HMODULE mod)
    {
        if (!address || !mod)
            return false;
        const uintptr_t base = reinterpret_cast<uintptr_t>(mod);
        return address >= base && address < base + module_image_size(mod);
    }

    bool default_dump_path(char* out, size_t out_size)
    {
        HMODULE self = GetModuleHandleA("dumper.dll");
        if (!self)
            return false;

        char module_path[MAX_PATH]{};
        if (!GetModuleFileNameA(self, module_path, MAX_PATH))
            return false;

        char* slash = strrchr(module_path, '\\');
        if (!slash)
            slash = strrchr(module_path, '/');
        if (slash)
            *(slash + 1) = '\0';

        snprintf(out, out_size, "%ssemtex_offset_dump.txt", module_path);
        return true;
    }

    void line(const char* fmt, ...)
    {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if (g_file)
            fprintf(g_file, "%s\n", buf);
    }

    void line_ptr(const char* name, const void* ptr, HMODULE mod)
    {
        if (!ptr)
        {
            line("%-28s NOT FOUND", name);
            return;
        }

        const uintptr_t va  = reinterpret_cast<uintptr_t>(ptr);
        const uintptr_t rva = mod ? (va - reinterpret_cast<uintptr_t>(mod)) : 0;
        line("%-28s VA 0x%08X  RVA 0x%08X", name, static_cast<unsigned>(va), static_cast<unsigned>(rva));
    }

    void line_slot(const char* name, uintptr_t slot, HMODULE mod)
    {
        if (!slot)
        {
            line("%-28s NOT FOUND", name);
            return;
        }

        if (mod && !ptr_in_module(slot, mod))
        {
            line("%-28s invalid slot VA 0x%08X (outside module)", name, static_cast<unsigned>(slot));
            return;
        }

        const uintptr_t rva = mod ? (slot - reinterpret_cast<uintptr_t>(mod)) : 0;
        line("%-28s slot VA 0x%08X  RVA 0x%08X", name, static_cast<unsigned>(slot), static_cast<unsigned>(rva));

        __try
        {
            const uintptr_t value = *reinterpret_cast<uintptr_t*>(slot);
            if (value)
                line("  -> deref VA 0x%08X", static_cast<unsigned>(value));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    uintptr_t resolve_cl_slot()
    {
        if (uint8_t* p = pattern_scan_any(k_pat_cl))
        {
            if (p[0] == 0xA1)
                return *reinterpret_cast<uintptr_t*>(p + 1);
            return reinterpret_cast<uintptr_t>(p);
        }
        return 0;
    }

    studiohdr_t** resolve_pstudiohdr()
    {
        if (uint8_t* p = pattern_scan_any(k_pat_pstudiohdr))
            return reinterpret_cast<studiohdr_t**>(p);
        return nullptr;
    }

    void write_dump()
    {
        HMODULE hw     = GetModuleHandleA("hw.dll");
        HMODULE client = GetModuleHandleA("client.dll");

        line("========== semtex offset dump ==========");
        line("Written to semtex_offset_dump.txt (safe file dump, no in-game hooks).");
        line_ptr("hw.dll base", hw, nullptr);
        line_ptr("client.dll base", client, nullptr);
        line("");

        line("--- pattern globals ---");

        const uintptr_t cl_slot = resolve_cl_slot();
        line_slot("cl", cl_slot, hw);

        if (uint8_t* p = pattern_scan_any(k_pat_engine_studio_api))
            line_ptr("engine_studio_api", p, hw);
        else
            line("%-28s NOT FOUND", "engine_studio_api");

        line_ptr("pstudiohdr", resolve_pstudiohdr(), hw);

        if (uint8_t* p = pattern_scan_any(k_pat_extra_info))
            line_ptr("g_PlayerExtraInfo", p, client);
        else
            line("%-28s NOT FOUND", "g_PlayerExtraInfo");

        if (uint8_t* p = pattern_scan_any(k_pat_scr_fov))
            line_ptr("scr_fov_value", p, hw);
        else
            line("%-28s NOT FOUND", "scr_fov_value");

        if (uint8_t* p = pattern_scan_any(k_pat_key_dest))
            line_ptr("key_dest", p, hw);
        else
            line("%-28s NOT FOUND", "key_dest");

        if (uint8_t* p = pattern_scan_any(k_pat_cls))
            line_ptr("cls", p, hw);
        else
            line("%-28s NOT FOUND", "cls");

        if (uint8_t* p = pattern_scan_any(k_pat_pmove))
            line_ptr("pmove", p, hw);
        else
            line("%-28s NOT FOUND", "pmove");

        if (uint8_t* p = pattern_scan_any(k_pat_cl_entities))
        {
            __try
            {
                const uintptr_t slot = *reinterpret_cast<uintptr_t*>(p);
                line_slot("cl_entities", slot, hw);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                line("%-28s pattern hit but slot read failed", "cl_entities");
            }
        }
        else
        {
            line("%-28s NOT FOUND", "cl_entities");
        }

        if (uint8_t* p = pattern_scan_any(k_pat_hud_studio_model_iface))
            line_ptr("HUD_GetStudioModelInterface", p, client);
        else if (client)
            line_ptr("HUD_GetStudioModelInterface", GetProcAddress(client, "HUD_GetStudioModelInterface"), client);
        else
            line("%-28s NOT FOUND", "HUD_GetStudioModelInterface");

        if (uint8_t* p = pattern_scan_any(k_pat_studio_model_renderer))
            line_ptr("studio_model_renderer vtable", p, hw);
        else
            line("%-28s NOT FOUND", "studio_model_renderer vtable");

        if (uint8_t* p = pattern_scan_any(k_pat_studio_draw))
            line_ptr("R_GLStudioDrawPoints", p, hw);
        else
            line("%-28s NOT FOUND", "R_GLStudioDrawPoints");

        if (uint8_t* p = pattern_scan_any(k_pat_studio_draw_player))
            line_ptr("R_StudioDrawPlayer", p, hw);
        else
            line("%-28s NOT FOUND", "R_StudioDrawPlayer");

        line("");
        line("--- HLSDK struct field offsets ---");
        line("client_state_t::time          0x%zX", static_cast<size_t>(cl_off::time));
        line("client_state_t::viewangles    0x%zX", static_cast<size_t>(cl_off::viewangles));
        line("client_state_t::cmd           0x%zX", static_cast<size_t>(cl_off::cmd));
        line("client_state_t::punchangle    0x%zX", static_cast<size_t>(cl_off::punchangle));
        line("client_state_t::onground      0x%zX", static_cast<size_t>(cl_off::onground));
        line("client_state_t::playernum     0x%zX", static_cast<size_t>(cl_off::playernum));
        line("client_state_t::maxclients    0x%zX", static_cast<size_t>(cl_off::maxclients));
        line("client_state_t::players       0x%zX", static_cast<size_t>(cl_off::players));
        line("frame_t::clientdata           0x%zX", static_cast<size_t>(frame_off::clientdata));
        line("frame_t sizeof                0x%zX", static_cast<size_t>(frame_off::frame_size));
        line("cl_entity_t::curstate         0x%zX", static_cast<size_t>(offsetof(cl_entity_t, curstate)));
        line("cl_entity_t::origin           0x%zX", static_cast<size_t>(offsetof(cl_entity_t, origin)));
        line("cl_entity_t sizeof            0x%zX", static_cast<size_t>(sizeof(cl_entity_t)));
        line("player_info_t::name           0x%zX", static_cast<size_t>(offsetof(player_info_t, name)));
        line("extra_player_info_t::team     0x%zX", static_cast<size_t>(offsetof(extra_player_info_t, teamnumber)));
        line("usercmd_t::viewangles         0x%zX", static_cast<size_t>(offsetof(usercmd_t, viewangles)));
        line("usercmd_t::buttons            0x%zX", static_cast<size_t>(offsetof(usercmd_t, buttons)));

        line("");
        line("--- cldll_func_t vtable byte offsets ---");
        line("HUD_CL_CreateMove             0x%zX", static_cast<size_t>(cldll_off::HUD_CL_CreateMove));
        line("HUD_CalcRef                   0x%zX", static_cast<size_t>(cldll_off::HUD_CalcRef));
        line("HUD_PostRunCmd                0x%zX", static_cast<size_t>(cldll_off::HUD_PostRunCmd));
        line("HUD_Studio_Interface          0x%zX", static_cast<size_t>(cldll_off::HUD_Studio_Interface));

        line("");
        line("--- engine_studio_api_t ---");
        line("StudioSetRemapColors            0x%zX", static_cast<size_t>(studio_api_off::StudioSetRemapColors));
        line("SetupPlayerModel                0x%zX", static_cast<size_t>(studio_api_off::SetupPlayerModel));
        line("StudioRenderModel vtable idx  %d", studio_api_off::StudioRenderModel_idx);
        line("StudioRenderFinal vtable idx  %d", studio_api_off::StudioRenderFinal_idx);

        line("");
        line("--- client.dll hardcoded RVAs ---");
        line("viewangles global RVA           0x%zX", static_cast<size_t>(cl_runtime::k_client_viewangles_rva));
        line("playermove ptr RVA              0x%zX", static_cast<size_t>(cl_runtime::k_playermove_ptr_rva));
        line("cl_yawspeed cvar RVA            0x%zX", static_cast<size_t>(cl_runtime::k_cl_yawspeed_cvar_rva));

        if (cl_slot)
        {
            __try
            {
                auto* cl = *reinterpret_cast<client_state_t**>(cl_slot);
                if (cl)
                {
                    line("");
                    line("--- live client_state (in match) ---");
                    line("cl VA                         0x%08X", static_cast<unsigned>(reinterpret_cast<uintptr_t>(cl)));
                    line("playernum                     %d", game::cl_playernum(cl));
                    line("maxclients                    %d", game::cl_maxclients(cl));
                }
                else
                {
                    line("");
                    line("cl pointer is null (main menu — join a map for live values).");
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                line("");
                line("cl pointer slot found but live read failed.");
            }
        }

        line("");
        line("========== dump complete ==========");
    }
}

offset_dump::result_t offset_dump::run()
{
    result_t result{};

    result.modules_ready = GetModuleHandleA("hw.dll") && GetModuleHandleA("client.dll");
    if (!result.modules_ready)
        return result;

    if (default_dump_path(result.file_path, sizeof(result.file_path)))
    {
        g_file = fopen(result.file_path, "w");
        result.file_ok = g_file != nullptr;
    }

    write_dump();

    if (g_file)
    {
        fclose(g_file);
        g_file = nullptr;
    }

    return result;
}
