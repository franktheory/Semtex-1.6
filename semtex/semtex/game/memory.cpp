#include "memory.h"
#include "cl_cmd.h"
#include "cl_runtime.h"
#include "pattern_defs.h"
#include "../sdk/usercmd.h"
#include "../sdk/pm_types.h"
#include <cstdio>
#include <cstring>

static uintptr_t              s_cl_slot        = 0;
static int*                   s_key_dest       = nullptr;
static client_static_t*       s_cls            = nullptr;
static bool                   s_session_seen   = false;
static uintptr_t              s_cl_funcs       = 0;
static void*                  s_pmove          = nullptr;
static engine_studio_api_t*   s_studio_api     = nullptr;
static engine_studio_api_t*   s_client_studio  = nullptr;
static r_studio_interface_t*  s_client_iface   = nullptr;
static studio_model_renderer_t* s_client_renderer_vtable = nullptr;
static studiohdr_t**          s_pstudiohdr     = nullptr;
static extra_player_info_t*   s_extra_info     = nullptr;
static float*                 s_scr_fov        = nullptr;
static bool                   s_ready             = false;
static bool                   s_patterns_resolved = false;
static char                   s_last_error[128] = "not initialized";

static int s_pat_cl = 0;
static int s_pat_studio = 0;
static int s_pat_client_studio = 0;
static int s_pat_studio_renderer = 0;
static int s_pat_pstudio = 0;
static int s_pat_extra = 0;
static int s_pat_cl_ent = 0;
static uintptr_t s_cl_entities_slot = 0;
static uint8_t*  s_hud_studio_fn = nullptr;

struct cl_clientfunc_table_t
{
    char _pad_before_hud_studio[0x9C];
    int (*HUD_GetStudioModelInterface)(int, r_studio_interface_t**, engine_studio_api_t*);
};

static bool is_executable_ptr(const void* ptr);

static size_t module_image_size(HMODULE mod)
{
    if (!mod)
        return 0;

    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
    const auto nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
    return nt->OptionalHeader.SizeOfImage;
}

static bool ptr_in_module(uintptr_t address, HMODULE mod)
{
    if (!address || !mod)
        return false;

    const uintptr_t base = reinterpret_cast<uintptr_t>(mod);
    return address >= base && address < base + module_image_size(mod);
}

static uint8_t* find_cstr_in_module(HMODULE mod, const char* str)
{
    if (!mod || !str)
        return nullptr;

    const size_t image_size = module_image_size(mod);
    const size_t len = strlen(str);
    auto* base = reinterpret_cast<uint8_t*>(mod);

    if (!image_size || len + 1 > image_size)
        return nullptr;

    for (size_t i = 0; i + len + 1 <= image_size; ++i)
    {
        if (memcmp(base + i, str, len + 1) == 0)
            return base + i;
    }

    return nullptr;
}

static uint8_t* find_push_imm32(HMODULE mod, uintptr_t imm)
{
    if (!mod)
        return nullptr;

    const size_t image_size = module_image_size(mod);
    auto* base = reinterpret_cast<uint8_t*>(mod);

    for (size_t i = 0; i + 5 <= image_size; ++i)
    {
        if (base[i] == 0x68 && *reinterpret_cast<uint32_t*>(base + i + 1) == static_cast<uint32_t>(imm))
            return base + i;
    }

    return nullptr;
}

static bool cldll_table_matches(uintptr_t base, uintptr_t createmove_fn, uintptr_t studio_fn, HMODULE client)
{
    if (!base || !createmove_fn || !studio_fn)
        return false;

    __try
    {
        const uintptr_t slot_createmove = *reinterpret_cast<uintptr_t*>(base + cldll_off::HUD_CL_CreateMove);
        const uintptr_t slot_studio     = *reinterpret_cast<uintptr_t*>(base + cldll_off::HUD_Studio_Interface);
        if (slot_createmove != createmove_fn || slot_studio != studio_fn)
            return false;

        if (!is_executable_ptr(reinterpret_cast<void*>(slot_createmove)))
            return false;

        const uintptr_t slot_init = *reinterpret_cast<uintptr_t*>(base);
        return ptr_in_module(slot_init, client);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static uintptr_t scan_cldll_table_in_module(HMODULE mod, uintptr_t createmove_fn, uintptr_t studio_fn, HMODULE client)
{
    if (!mod)
        return 0;

    const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(mod);
    const auto nt  = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(mod) + dos->e_lfanew);
    auto* section  = IMAGE_FIRST_SECTION(nt);

    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
    {
        if (!(section->Characteristics & (IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA)))
            continue;

        const uintptr_t start = reinterpret_cast<uintptr_t>(mod) + section->VirtualAddress;
        const size_t    size  = section->Misc.VirtualSize;
        if (size < sizeof(void*))
            continue;

        for (size_t off = 0; off + sizeof(void*) <= size; off += sizeof(void*))
        {
            const uintptr_t slot_addr = start + off;
            __try
            {
                if (*reinterpret_cast<uintptr_t*>(slot_addr) != createmove_fn)
                    continue;

                const uintptr_t candidate = slot_addr - cldll_off::HUD_CL_CreateMove;
                if (cldll_table_matches(candidate, createmove_fn, studio_fn, client))
                    return candidate;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    return 0;
}

static uintptr_t resolve_active_cldll_table(uintptr_t client_table, uintptr_t createmove_fn, uintptr_t studio_fn)
{
    HMODULE client = GetModuleHandleA("client.dll");
    HMODULE hw     = GetModuleHandleA("hw.dll");
    if (!client || !createmove_fn || !studio_fn)
        return 0;

    if (uintptr_t hw_table = scan_cldll_table_in_module(hw, createmove_fn, studio_fn, client))
        return hw_table;

    if (client_table && cldll_table_matches(client_table, createmove_fn, studio_fn, client))
        return client_table;

    if (uintptr_t client_table_scan = scan_cldll_table_in_module(client, createmove_fn, studio_fn, client))
        return client_table_scan;

    return 0;
}

static bool cldll_table_looks_valid(uintptr_t table, HMODULE hw)
{
    if (!table || !ptr_in_module(table, hw))
        return false;

    __try
    {
        const uintptr_t init_fn = *reinterpret_cast<uintptr_t*>(table);
        const uintptr_t createmove_fn = *reinterpret_cast<uintptr_t*>(table + cldll_off::HUD_CL_CreateMove);
        if (!init_fn || !createmove_fn)
            return false;

        if (!is_executable_ptr(reinterpret_cast<void*>(init_fn)))
            return false;
        if (!is_executable_ptr(reinterpret_cast<void*>(createmove_fn)))
            return false;

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static uintptr_t resolve_cl_funcs_from_screenfade(HMODULE hw, HMODULE client)
{
    uint8_t* screenfade = find_cstr_in_module(hw, "ScreenFade");
    if (!screenfade)
        return 0;

    uint8_t* push_ref = find_push_imm32(hw, reinterpret_cast<uintptr_t>(screenfade));
    if (!push_ref)
        return 0;

    static const int k_export_offs[] = { 0x13, 0x18, 0x17 };
    for (int off : k_export_offs)
    {
        __try
        {
            const uintptr_t export_ptr = *reinterpret_cast<uintptr_t*>(push_ref + off);
            if (!export_ptr)
                continue;


            if (cldll_table_looks_valid(export_ptr, hw))
                return export_ptr;

            if (!client || !ptr_in_module(export_ptr, client))
                continue;

            auto* client_funcs = reinterpret_cast<cl_clientfunc_table_t*>(export_ptr);
            auto* fn = reinterpret_cast<uint8_t*>(client_funcs->HUD_GetStudioModelInterface);
            if (!fn || !ptr_in_module(reinterpret_cast<uintptr_t>(fn), client))
                continue;

            const uintptr_t client_table = export_ptr + cldll_off::base_from_studio_iface;
            const uintptr_t createmove_fn = *reinterpret_cast<uintptr_t*>(client_table + cldll_off::HUD_CL_CreateMove);
            const uintptr_t studio_fn     = reinterpret_cast<uintptr_t>(fn);
            if (!createmove_fn || !is_executable_ptr(reinterpret_cast<void*>(createmove_fn)))
                continue;

            if (uintptr_t hw_table = resolve_active_cldll_table(client_table, createmove_fn, studio_fn))
                return hw_table;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    return 0;
}

static uintptr_t resolve_cl_funcs_base()
{
    if (s_cl_funcs)
        return s_cl_funcs;

    HMODULE hw = GetModuleHandleA("hw.dll");
    HMODULE client = GetModuleHandleA("client.dll");
    if (!hw)
        return 0;

    s_cl_funcs = resolve_cl_funcs_from_screenfade(hw, client);
    return s_cl_funcs;
}

static bool validate_pmove_ptr(void* pm)
{
    if (!pm)
        return false;

    __try
    {
        const uintptr_t addr = reinterpret_cast<uintptr_t>(pm);
        if (s_cl_funcs && addr >= s_cl_funcs && addr < s_cl_funcs + 0x400)
            return false;

        const int flags = pm_flags(pm);
        if (flags < 0)
            return false;

        const int mt = pm_movetype(pm);
        if (mt < 0 || mt > 32)
            return false;

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void* resolve_pmove_from_screenfade(HMODULE hw)
{
    if (s_pmove && validate_pmove_ptr(s_pmove))
        return s_pmove;

    if (uint8_t* match = pattern_scan_any(k_pat_pmove))
    {
        void* pm = reinterpret_cast<void*>(match);
        if (validate_pmove_ptr(pm))
        {
            s_pmove = pm;
            return s_pmove;
        }
    }

    uint8_t* screenfade = find_cstr_in_module(hw, "ScreenFade");
    if (!screenfade)
        return nullptr;

    uint8_t* push_ref = find_push_imm32(hw, reinterpret_cast<uintptr_t>(screenfade));
    if (!push_ref)
        return nullptr;

    static const int k_pmove_offs[] = { 0x18, 0x17, 0x13 };
    for (int off : k_pmove_offs)
    {
        __try
        {
            void** slot = reinterpret_cast<void**>(push_ref + off);
            if (!slot)
                continue;

            if (validate_pmove_ptr(*slot))
            {
                s_pmove = *slot;
                return s_pmove;
            }

            void* indirect = *slot;
            if (indirect && validate_pmove_ptr(*reinterpret_cast<void**>(indirect)))
            {
                s_pmove = *reinterpret_cast<void**>(indirect);
                return s_pmove;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    return nullptr;
}

static uint8_t* resolve_hud_get_studio_fn()
{
    if (s_hud_studio_fn)
        return s_hud_studio_fn;

    HMODULE client = GetModuleHandleA("client.dll");
    if (client)
    {
        auto* export_fn = reinterpret_cast<uint8_t*>(GetProcAddress(client, "HUD_GetStudioModelInterface"));
        if (export_fn && ptr_in_module(reinterpret_cast<uintptr_t>(export_fn), client))
        {
            s_hud_studio_fn = export_fn;
            return s_hud_studio_fn;
        }
    }

    if (uint8_t* match = pattern_scan_any(k_pat_hud_studio_model_iface))
    {
        s_hud_studio_fn = match;
        return s_hud_studio_fn;
    }

    HMODULE hw = GetModuleHandleA("hw.dll");
    if (!hw || !client)
        return nullptr;

    uint8_t* screenfade = find_cstr_in_module(hw, "ScreenFade");
    if (!screenfade)
        return nullptr;

    uint8_t* push_ref = find_push_imm32(hw, reinterpret_cast<uintptr_t>(screenfade));
    if (!push_ref)
        return nullptr;

    static const int k_export_offs[] = { 0x13, 0x18, 0x17 };
    for (int off : k_export_offs)
    {
        __try
        {
            auto export_ptr = *reinterpret_cast<uintptr_t*>(push_ref + off);
            if (!ptr_in_module(export_ptr, client))
                continue;

            auto* client_funcs = reinterpret_cast<cl_clientfunc_table_t*>(export_ptr);
            auto* fn = reinterpret_cast<uint8_t*>(client_funcs->HUD_GetStudioModelInterface);
            if (!fn || !ptr_in_module(reinterpret_cast<uintptr_t>(fn), client))
                continue;

            s_hud_studio_fn = fn;
            return s_hud_studio_fn;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    return nullptr;
}

static bool parse_hud_studio_from_fn(uint8_t* fn, uintptr_t* out_studio, uintptr_t* out_iface)
{
    if (!fn || !out_studio || !out_iface)
        return false;

    *out_studio = 0;
    *out_iface = 0;

    HMODULE client = GetModuleHandleA("client.dll");
    if (!client)
        return false;

    size_t rep_pos = 0;
    for (size_t i = 0; i + 1 < 0x80; ++i)
    {
        if (fn[i] == 0xF3 && fn[i + 1] == 0xA5)
        {
            rep_pos = i;
            break;
        }
    }

    if (!rep_pos)
        return false;

    for (size_t i = rep_pos; i > 0; --i)
    {
        if (fn[i] == 0xBF)
        {
            const uintptr_t imm = *reinterpret_cast<uintptr_t*>(fn + i + 1);
            if (ptr_in_module(imm, client))
            {
                *out_studio = imm;
                break;
            }
        }
    }

    for (size_t i = 0; i + 6 <= rep_pos; ++i)
    {
        if (fn[i] == 0xC7 && fn[i + 1] == 0x00)
        {
            const uintptr_t imm = *reinterpret_cast<uintptr_t*>(fn + i + 2);
            if (ptr_in_module(imm, client))
            {
                *out_iface = imm;
                break;
            }
        }
    }

    return *out_studio != 0;
}

static bool is_executable_ptr(const void* ptr)
{
    if (!ptr)
        return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(ptr, &mbi, sizeof(mbi)))
        return false;

    const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.State == MEM_COMMIT) && ((mbi.Protect & exec) != 0);
}


static bool vtable_looks_valid(uintptr_t vtable_va)
{
    if (!vtable_va)
        return false;


    static const char* const k_mods[] = { "hw.dll", "client.dll", nullptr };
    HMODULE owner = nullptr;
    for (int mi = 0; k_mods[mi]; ++mi)
    {
        HMODULE h = GetModuleHandleA(k_mods[mi]);
        if (h && ptr_in_module(vtable_va, h))
        {
            owner = h;
            break;
        }
    }
    if (!owner)
        return false;

    auto* slots = reinterpret_cast<uintptr_t*>(vtable_va);
    __try
    {
        int code_count = 0;
        for (int i = 0; i < 20; ++i)
        {
            if (is_executable_ptr(reinterpret_cast<void*>(slots[i])))
                ++code_count;
        }

        if (code_count < 15)
            return false;


        if (!is_executable_ptr(reinterpret_cast<void*>(slots[18])))
            return false;
        if (!is_executable_ptr(reinterpret_cast<void*>(slots[19])))
            return false;

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}


static constexpr uintptr_t k_hw_renderer_init_rva  = 0x00228C30;
static constexpr uintptr_t k_hw_renderer_vtbl_rva  = 0x002C99D8;

static bool try_resolve_client_renderer_vtable()
{
    if (s_client_renderer_vtable)
        return true;

    HMODULE hw = GetModuleHandleA("hw.dll");
    if (!hw)
        return false;


    if (uint8_t* result = pattern_scan_any(k_pat_studio_model_renderer))
    {
        const uintptr_t vtable_va = reinterpret_cast<uintptr_t>(result);
        if (vtable_looks_valid(vtable_va))
        {
            s_client_renderer_vtable = reinterpret_cast<studio_model_renderer_t*>(vtable_va);
            s_pat_studio_renderer = 1;
            return true;
        }
    }


    {
        const uintptr_t hw_base = reinterpret_cast<uintptr_t>(hw);
        uint8_t* init_fn = reinterpret_cast<uint8_t*>(hw_base + k_hw_renderer_init_rva);
        __try
        {

            if (init_fn[0] == 0x56 && init_fn[1] == 0x8B && init_fn[2] == 0xF1 && init_fn[3] == 0xE8)
            {

                if (init_fn[8] == 0xC7 && init_fn[9] == 0x06)
                {
                    const uintptr_t vtable_va = *reinterpret_cast<uint32_t*>(init_fn + 10);
                    if (vtable_looks_valid(vtable_va))
                    {
                        s_client_renderer_vtable = reinterpret_cast<studio_model_renderer_t*>(vtable_va);
                        s_pat_studio_renderer = 2;
                        return true;
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }


    {
        const uintptr_t hw_base = reinterpret_cast<uintptr_t>(hw);
        const uintptr_t vtable_va = hw_base + k_hw_renderer_vtbl_rva;
        if (vtable_looks_valid(vtable_va))
        {
            s_client_renderer_vtable = reinterpret_cast<studio_model_renderer_t*>(vtable_va);
            s_pat_studio_renderer = 3;
            return true;
        }
    }

    return false;
}

static bool try_resolve_client_studio()
{
    if (s_client_studio)
        return true;

    if (!GetModuleHandleA("client.dll"))
        return false;

    uint8_t* fn = resolve_hud_get_studio_fn();
    if (!fn)
        return false;

    HMODULE client = GetModuleHandleA("client.dll");

    uintptr_t studio_addr = 0;
    uintptr_t iface_addr = 0;
    if (parse_hud_studio_from_fn(fn, &studio_addr, &iface_addr))
    {
        __try
        {
            auto* api = reinterpret_cast<engine_studio_api_t*>(studio_addr);
            if (api->StudioSetRemapColors && api->StudioGetBoneTransform && api->GetCurrentEntity)
            {
                s_client_studio = api;
                s_pat_client_studio = 1;
            }

            if (iface_addr)
            {
                auto* iface = reinterpret_cast<r_studio_interface_t*>(iface_addr);
                if (iface->StudioDrawPlayer)
                    s_client_iface = iface;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (s_client_studio)
        return true;

    static const int k_studio_offs[] = { 0x1A, 0x30 };
    for (int off : k_studio_offs)
    {
        __try
        {
            const uintptr_t studio_addr = *reinterpret_cast<uintptr_t*>(fn + off);
            if (!ptr_in_module(studio_addr, client))
                continue;

            auto* api = reinterpret_cast<engine_studio_api_t*>(studio_addr);
            if (!api->StudioSetRemapColors || !api->StudioGetBoneTransform || !api->GetCurrentEntity)
                continue;

            s_client_studio = api;
            s_pat_client_studio = 1;
            break;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    static const int k_iface_offs[] = { 0x20, 0x36 };
    for (int off : k_iface_offs)
    {
        __try
        {
            const uintptr_t iface_addr = *reinterpret_cast<uintptr_t*>(fn + off);
            if (!ptr_in_module(iface_addr, client))
                continue;

            auto* iface = reinterpret_cast<r_studio_interface_t*>(iface_addr);
            if (!iface->StudioDrawPlayer)
                continue;

            s_client_iface = iface;
            break;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    try_resolve_client_renderer_vtable();
    return s_client_studio != nullptr;
}

static void fail(const char* msg)
{
    strcpy_s(s_last_error, msg);
    s_ready = false;
}

static client_state_t* live_cl()
{
    if (!s_cl_slot)
        return nullptr;

    __try
    {
        return *reinterpret_cast<client_state_t**>(s_cl_slot);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static studiohdr_t** resolve_pstudiohdr(uint8_t* match)
{
    if (!match)
        return nullptr;

    if (match[0] == 0xA3)
        return reinterpret_cast<studiohdr_t**>(*reinterpret_cast<uintptr_t*>(match + 1));

    return reinterpret_cast<studiohdr_t**>(match);
}

static bool validate_runtime()
{
    if (!live_cl() || !s_studio_api || !s_pstudiohdr)
        return false;

    __try
    {
        if (!s_studio_api->GetViewInfo || !s_studio_api->PlayerInfo)
            return false;

        float org[3]{}, up[3]{}, right[3]{}, vpn[3]{};
        s_studio_api->GetViewInfo(org, up, right, vpn);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool try_resolve_extra_from_teamname(uintptr_t teamname_addr)
{
    if (!teamname_addr)
        return false;

    auto* arr = reinterpret_cast<extra_player_info_t*>(teamname_addr - 0x2C);
    (void)arr[1].teamnumber;
    s_extra_info = arr;
    return true;
}

static bool try_resolve_extra()
{
    if (s_extra_info)
        return true;

    HMODULE client = GetModuleHandleA("client.dll");
    if (!client)
        return false;


    if (uint8_t* match = pattern::scan_module("client.dll", "6B F0 74 8D BC 35 ? ? ? ?", 0, 0))
    {
        __try
        {
            const uintptr_t teamname = *reinterpret_cast<uint32_t*>(match + 7);
            if (try_resolve_extra_from_teamname(teamname))
            {
                s_pat_extra = 1;
                return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }


    __try
    {
        auto* arr = reinterpret_cast<extra_player_info_t*>(
            reinterpret_cast<uintptr_t>(client) + 0x1257D8);
        (void)arr[1].teamnumber;
        s_extra_info = arr;
        s_pat_extra = 2;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    if (uint8_t* p = pattern_scan_any(k_pat_extra_info))
    {
        __try
        {
            auto* arr = reinterpret_cast<extra_player_info_t*>(p);
            (void)arr[1].teamnumber;
            s_extra_info = arr;
            s_pat_extra = 3;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    return false;
}

static void clear_state()
{
    s_cl_slot = 0;
    s_key_dest = nullptr;
    s_cls = nullptr;
    s_session_seen = false;
    s_cl_funcs = 0;
    s_pmove = nullptr;
    s_studio_api = nullptr;
    s_client_studio = nullptr;
    s_client_iface = nullptr;
    s_client_renderer_vtable = nullptr;
    s_hud_studio_fn = nullptr;
    s_pstudiohdr = nullptr;
    s_extra_info = nullptr;
    s_scr_fov = nullptr;
    s_cl_entities_slot = 0;
    s_ready = false;
    s_patterns_resolved = false;
    s_pat_cl = s_pat_studio = s_pat_client_studio = s_pat_studio_renderer = s_pat_pstudio = s_pat_extra = s_pat_cl_ent = 0;
}

static bool resolve_cl_entities_slot()
{
    if (s_cl_entities_slot)
        return true;

    if (uint8_t* match = pattern_scan_any(k_pat_cl_entities))
    {
        s_cl_entities_slot = *reinterpret_cast<uintptr_t*>(match);
        if (s_cl_entities_slot)
        {
            s_pat_cl_ent = 1;
            return true;
        }
    }

    return false;
}

static bool resolve_patterns()
{
    s_pat_cl = s_pat_studio = s_pat_pstudio = s_pat_extra = s_pat_cl_ent = 0;

    if (uint8_t* p = pattern_scan_any(k_pat_cl))
    {
        if (p[0] == 0xA1)
            s_cl_slot = *reinterpret_cast<uintptr_t*>(p + 1);
        else
            s_cl_slot = reinterpret_cast<uintptr_t>(p);

        if (s_cl_slot)
            s_pat_cl = 1;
    }

    if (uint8_t* p = pattern_scan_any(k_pat_engine_studio_api))
    {
        s_studio_api = reinterpret_cast<engine_studio_api_t*>(p);
        s_pat_studio = 1;
    }

    if (uint8_t* p = pattern_scan_any(k_pat_pstudiohdr))
    {
        s_pstudiohdr = resolve_pstudiohdr(p);
        if (s_pstudiohdr)
            s_pat_pstudio = 1;
    }

    try_resolve_extra();
    resolve_cl_entities_slot();
    try_resolve_client_studio();

    if (uint8_t* p = pattern_scan_any(k_pat_scr_fov))
        s_scr_fov = reinterpret_cast<float*>(p);

    if (uint8_t* p = pattern_scan_any(k_pat_key_dest))
        s_key_dest = reinterpret_cast<int*>(p);

    return s_cl_slot && s_studio_api && s_pstudiohdr;
}

static bool ensure_patterns()
{
    if (s_patterns_resolved)
        return s_cl_slot && s_studio_api && s_pstudiohdr;

    if (!resolve_patterns())
        return false;

    s_patterns_resolved = true;
    return s_cl_slot && s_studio_api && s_pstudiohdr;
}

bool game::init()
{
    if (!GetModuleHandleA("hw.dll"))
    {
        fail("hw.dll not loaded");
        return false;
    }

    if (!ensure_patterns())
    {
        fail("pattern fail (hw.dll)");
        return false;
    }

    if (!live_cl())
    {
        fail("waiting for match (cl null)");
        return false;
    }

    if (!validate_runtime())
    {
        fail("pattern matched but runtime validation failed");
        return false;
    }

    s_ready = true;
    strcpy_s(s_last_error, "ok");
    return true;
}

bool game::tick()
{
    if (!GetModuleHandleA("hw.dll"))
    {
        if (s_ready || s_patterns_resolved)
            clear_state();
        fail("hw.dll not loaded");
        return false;
    }

    if (!ensure_patterns())
    {
        fail("pattern fail (hw.dll)");
        return false;
    }

    if (!s_ready)
    {
        if (!live_cl())
        {
            fail("waiting for match (cl null)");
            return false;
        }

        if (!validate_runtime())
        {
            fail("pattern matched but runtime validation failed");
            return false;
        }

        s_ready = true;
        strcpy_s(s_last_error, "ok");
    }
    else if (!live_cl() || !validate_runtime())
    {
        s_ready = false;
        s_cls = nullptr;
        strcpy_s(s_last_error, "waiting for match (cl null)");
        return false;
    }

    try_resolve_extra();
    try_resolve_client_studio();
    try_resolve_client_renderer_vtable();
    return true;
}

void game::shutdown()
{
    clear_state();
    strcpy_s(s_last_error, "shutdown");
}

int game::local_slot()
{
    if (auto* api = studio_api(); api && api->GetViewEntity)
    {
        __try
        {
            if (cl_entity_t* ve = api->GetViewEntity())
            {
                if (ve->player && ve->index > 0)
                    return ve->index - 1;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (auto* cl = live_cl())
    {
        const int slot = cl_playernum(cl);
        if (slot >= 0 && slot < MAX_CLIENTS)
            return slot;
    }

    return -1;
}

float game::view_fov()
{
    if (s_scr_fov)
    {
        __try
        {
            const float fov = *s_scr_fov;
            if (fov >= 60.f && fov <= 150.f)
                return fov;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return 90.f;
}

float game::client_time()
{
    if (s_studio_api && s_studio_api->GetTimes)
    {
        __try
        {
            int framecount = 0;
            double current = 0.0;
            double old = 0.0;
            s_studio_api->GetTimes(&framecount, &current, &old);
            if (current > 0.0)
                return static_cast<float>(current);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (auto* cl = live_cl())
        return static_cast<float>(cl_time(cl));

    return 0.f;
}

client_state_t* game::cl() { return live_cl(); }

uintptr_t game::cl_dll_funcs()
{
    return resolve_cl_funcs_base();
}

engine_studio_api_t* game::studio_api() { return s_studio_api; }
engine_studio_api_t* game::client_studio_api()
{
    try_resolve_client_studio();
    return s_client_studio;
}

r_studio_interface_t* game::client_studio_iface()
{
    try_resolve_client_studio();
    return s_client_iface;
}

studio_model_renderer_t* game::client_studio_renderer_vtable()
{
    try_resolve_client_renderer_vtable();
    return s_client_renderer_vtable;
}

bool game::client_studio_chrome_api_ready()
{
    auto* api = client_studio_api();
    if (!api)
        return false;

    __try
    {
        return api->SetForceFaceFlags && api->SetChromeOrigin
            && is_executable_ptr(reinterpret_cast<void*>(api->SetForceFaceFlags))
            && is_executable_ptr(reinterpret_cast<void*>(api->SetChromeOrigin));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

studiohdr_t** game::pstudiohdr() { return s_pstudiohdr; }

cl_entity_t* game::cl_entity(int entity_index)
{
    if (!s_cl_entities_slot || entity_index <= 0)
        return nullptr;

    __try
    {
        auto* base = *reinterpret_cast<uint8_t**>(s_cl_entities_slot);
        if (!base)
            return nullptr;

        auto* ent = reinterpret_cast<cl_entity_t*>(base + entity_index * sizeof(cl_entity_t));
        if (!ent || !ent->player)
            return nullptr;

        return ent;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

bool game::ready() { return s_ready; }

static bool cls_state_looks_valid(int state)
{
    return state >= CA_DEDICATED && state <= CA_ACTIVE;
}

static bool cls_ptr_looks_valid(client_static_t* cls, HMODULE hw)
{
    if (!cls)
        return false;

    const uintptr_t addr = reinterpret_cast<uintptr_t>(cls);
    if (!ptr_in_module(addr, hw))
        return false;

    __try
    {
        return cls_state_looks_valid(cls->state);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static client_static_t* resolve_cls()
{
    if (s_cls)
        return s_cls;

    HMODULE hw = GetModuleHandleA("hw.dll");
    if (!hw)
        return nullptr;

    if (uint8_t* match = pattern_scan_any(k_pat_cls))
    {
        auto* cls = reinterpret_cast<client_static_t*>(match);
        if (cls_ptr_looks_valid(cls, hw))
        {
            s_cls = cls;
            return s_cls;
        }
    }

    return nullptr;
}

static bool key_dest_is_menu()
{
    if (!s_key_dest)
        return false;

    __try
    {
        return *s_key_dest == KEY_MENU;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool has_match_teams()
{
    auto* extra = game_extra_info();
    if (!extra)
        return false;

    __try
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            const int team = static_cast<int>(extra[i + 1].teamnumber);
            if (team == TEAM_TERRORIST || team == TEAM_CT)
                return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return false;
}

static bool has_scoreboard_players()
{
    auto* cl = live_cl();
    if (!cl)
        return false;

    __try
    {
        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (game::cl_player(cl, i).name[0])
                return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return false;
}

static bool has_entity_teams()
{
    for (int i = 1; i <= MAX_CLIENTS; ++i)
    {
        cl_entity_t* ent = game::cl_entity(i);
        if (!ent || !ent->player)
            continue;

        __try
        {
            const int team = ent->curstate.team;
            if (team == TEAM_TERRORIST || team == TEAM_CT)
                return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    return false;
}

static bool session_evidence()
{
    return has_match_teams() || has_scoreboard_players() || has_entity_teams();
}

bool game::movement_allowed()
{
    if (!s_ready)
        return false;

    if (key_dest_is_menu())
        return false;

    if (client_static_t* cls = resolve_cls())
    {
        __try
        {
            if (cls->state == CA_DISCONNECTED
                && !has_match_teams()
                && !has_entity_teams())
                return false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    return true;
}

bool game::in_match()
{
    if (!movement_allowed())
    {
        s_session_seen = false;
        return false;
    }

    if (session_evidence())
    {
        s_session_seen = true;
        return true;
    }

    if (s_session_seen)
    {
        if (client_static_t* cls = resolve_cls())
        {
            __try
            {
                if (cls->state != CA_DISCONNECTED)
                    return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        s_session_seen = false;
    }

    return false;
}

void* game::playermove_state()
{
    if (!movement_allowed())
        return nullptr;

    HMODULE hw = GetModuleHandleA("hw.dll");
    if (!hw)
        return nullptr;

    return resolve_pmove_from_screenfade(hw);
}

const char* game::last_error() { return s_last_error; }

int game::pat_cl() { return s_pat_cl; }
int game::pat_studio_api() { return s_pat_studio; }
int game::pat_client_studio() { return s_pat_client_studio; }
int game::pat_studio_renderer() { return s_pat_studio_renderer; }
int game::pat_pstudiohdr() { return s_pat_pstudio; }
int game::pat_extra_info() { return s_pat_extra; }
int game::pat_cl_entities() { return s_pat_cl_ent; }

extra_player_info_t* game_extra_info()
{
    try_resolve_extra();
    return s_extra_info;
}

usercmd_t* game::client_cmd()
{
    auto* state = live_cl();
    if (!state)
        return nullptr;

    __try
    {
        const std::ptrdiff_t off = cl_runtime::ready() ? cl_runtime::cmd : cl_off::cmd;
        return reinterpret_cast<usercmd_t*>(reinterpret_cast<char*>(state) + off);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

bool game::cmd_is_client_state(usercmd_t* cmd)
{
    if (!cmd)
        return false;

    auto* cl_cmd = client_cmd();
    return cl_cmd && cmd == cl_cmd;
}

float* game::punch_from_cmd(usercmd_t* cmd)
{
    if (!cmd || !cmd_is_client_state(cmd))
        return nullptr;

    return reinterpret_cast<float*>(reinterpret_cast<char*>(cmd) + sizeof(usercmd_t) + sizeof(float) * 3);
}

bool game::fn_in_client_dll(const void* fn)
{
    if (!fn)
        return false;

    return ptr_in_module(reinterpret_cast<uintptr_t>(fn), GetModuleHandleA("client.dll"));
}

int game::resolve_postruncmd_vtable_index(uintptr_t table)
{
    if (!table)
        return static_cast<int>(cldll_off::HUD_PostRunCmd / sizeof(void*));

    HMODULE client = GetModuleHandleA("client.dll");
    if (!client)
        return static_cast<int>(cldll_off::HUD_PostRunCmd / sizeof(void*));

    const uintptr_t createmove = *reinterpret_cast<uintptr_t*>(table + cldll_off::HUD_CL_CreateMove);
    const uintptr_t calc_ref   = *reinterpret_cast<uintptr_t*>(table + cldll_off::HUD_CalcRef);

    for (int idx = 23; idx <= 27; ++idx)
    {
        __try
        {
            const uintptr_t fn = *reinterpret_cast<uintptr_t*>(table + idx * sizeof(void*));
            if (!fn || fn == createmove || fn == calc_ref)
                continue;
            if (!ptr_in_module(fn, client))
                continue;
            if (!is_executable_ptr(reinterpret_cast<void*>(fn)))
                continue;
            return idx;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    return static_cast<int>(cldll_off::HUD_PostRunCmd / sizeof(void*));
}
