#pragma once

#include "../util/pattern.h"

struct pattern_ref_t
{
    const char* module;
    const char* signature;
    int         offset;
    int         deref;
};

struct pattern_entry_t
{
    const char*       name;
    pattern_ref_t     refs[4];
};


inline uint8_t* pattern_scan_any(const pattern_entry_t& entry)
{
    for (const auto& ref : entry.refs)
    {
        if (!ref.signature)
            break;
        if (uint8_t* found = pattern::scan_module(ref.module, ref.signature, ref.offset, ref.deref))
            return found;
    }
    return nullptr;
}

inline const pattern_entry_t k_pat_cl = {
    "cl",
    {

        { "hw.dll", "A1 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? C6 45 FC 02 8B 40 04", 0, 0 },
        { "hw.dll", "A1 ? ? ? ? C7 05 ? ? ? ?", 0, 0 },
        { "hw.dll", "68 ? ? ? ? E8 ? ? ? ? B8 ? ? ? ? 83 C4 0C", 1, 1 },
        { "hw.dll", "68 ? ? ? ? E8 ? ? ? ? B8 ? ? ? ? 83 C4 0C", 1, 1 },
    },
};

inline const pattern_entry_t k_pat_engine_studio_api = {
    "engine_studio_api",
    {
        { "hw.dll", "68 ? ? ? ? 68 ? ? ? ? 6A 01 FF D0 83 C4 0C", 1, 1 },
        { "hw.dll", "68 ? ? ? ? 68 ? ? ? ? 6A 01 FF D0 83 C4 0C", 1, 1 },
        { "hw.dll", "68 ? ? ? ? 68 ? ? ? ? 6A 01 FF D0 83 C4 0C", 1, 1 },
    },
};

inline const pattern_entry_t k_pat_pstudiohdr = {
    "pstudiohdr",
    {

        { "hw.dll", "F3 0F 11 05 ? ? ? ? A3 ? ? ? ? 5D C3", 9, 0 },
        { "hw.dll", "A3 ? ? ? ? 8B 53 64 03 C8 DD 5C 24 10 DD 44 24 10 DC 0D", 0, 0 },
        { "hw.dll", "A3 ? ? ? ? 8B 43 04 D9 03 89 55 90 8B 55 24 89 45 8C", 0, 0 },
        { "hw.dll", "A1 ? ? ? ? 8B 53 64 03 E8 DD 5C 24 0C DD 44 24 0C DC 0D", 1, 1 },
    },
};

inline const pattern_entry_t k_pat_extra_info = {
    "g_PlayerExtraInfo",
    {
        { "client.dll", "66 89 99 ? ? ? ? 66 89 A9 ? ? ? ? 66 89 91 ? ? ? ? 66 89 81 ? ? ? ? 7D 09 66 C7 81", 3, 0 },
        { "client.dll", "83 3D ? ? ? ? 02 75 24 A1 ? ? ? ? 80 78 19 00 75 19", 10, 1 },
        { "client.dll", "0F BF 87 ? ? ? ? 8B 16 50 68 ? ? ? ? 8B CE FF 52 ? 8D 4C AD 00 66 8B 04 8D", 3, 0 },
    },
};

inline const pattern_entry_t k_pat_scr_fov = {
    "scr_fov_value",
    {
        { "hw.dll", "D9 05 ? ? ? ? D8 0D ? ? ? ? DE C1", 2, 0 },
        { "hw.dll", "D9 05 ? ? ? ? D8 0D ? ? ? ? DE C1", 2, 0 },
    },
};

inline const pattern_entry_t k_pat_entity_update = {
    "CL_ProcessEntityUpdate",
    {
        { "hw.dll", "56 8B 74 24 08 57 8B 8E D8 02 00 00 8B 86 B4 02 00 00 51 89 06", 0, 0 },
        { "hw.dll", "55 8B EC 56 8B 75 08 57 8B 8E D8 02 00 00 8B 86 B4 02 00 00 51 89 06", 0, 0 },
    },
};

inline const pattern_entry_t k_pat_studio_draw = {
    "R_GLStudioDrawPoints",
    {
        { "hw.dll", "55 8B EC 83 EC 48 A1 ? ? ? ? 8B 0D ? ? ? ? 53 56 8B 70 54", 0, 0 },
        { "hw.dll", "83 EC 48 8B 0D ? ? ? ? 8B 15 ? ? ? ? 53 55 8B 41 54", 0, 0 },
        { "hw.dll", "83 EC 44 A1 ? ? ? ? 8B 0D ? ? ? ? 53 55 56 8B 70 54 8B 40 60 57 03 C1", 0, 0 },
        { "hw.dll", "55 8B EC 81 EC B8 00 00 00 53 56 8B DA", 0, 0 },
    },
};

inline const pattern_entry_t k_pat_studio_draw_player = {
    "R_StudioDrawPlayer",
    {
        { "hw.dll", "55 8B EC 81 EC DC 0B 00 00 53 8B 5D 0C 56 57 8B 4B 04 49", 0, 0 },
    },
};

inline const pattern_entry_t k_pat_pstudio_api = {
    "pStudioAPI",
    {
        { "hw.dll", "68 ? ? ? ? 6A 01 FF D0 83 C4 0C 85 C0 75 12 68 ? ? ? ?", 1, 1 },
        { "hw.dll", "8B 0D ? ? ? ? 8B 35 ? ? ? ? D1 EA", 2, 0 },
        { "hw.dll", "68 ? ? ? ? 68 ? ? ? ? 6A 01 FF D0 83 C4 0C", 1, 1 },
    },
};


inline const pattern_entry_t k_pat_cl_entities = {
    "cl_entities",
    {
        { "hw.dll", "55 8B EC 8B 45 08 85 C0 78 13 25 FF 0F 00 00 69 C0 B8 0B 00 00 03 05 ? ? ? ? 5D C3", 22, 0 },
    },
};


inline const pattern_entry_t k_pat_hud_studio_model_iface = {
    "HUD_GetStudioModelInterface",
    {
        { "client.dll", "83 7C 24 04 01 74 03 33 C0 C3 8B 44 24 08 56 8B 74 24 10 57 B9 2E 00 00 00", 0, 0 },
        { "client.dll", "83 7C 24 04 01 74 03 33 C0 C3 8B 44 24 08 56 8B 74 24 10 57 B9 2E 00 00 00 BF", 0, 0 },
    },
};


inline const pattern_entry_t k_pat_studio_model_renderer = {
    "studio_model_renderer",
    {
        { "hw.dll",     "56 8B F1 E8 ? ? ? ? C7 06 ? ? ? ? C6 86 ? ? ? ? ? 8B C6 5E C3", 0x0A, 1 },
        { "hw.dll",     "56 8B F1 E8 ? ? ? ? C7 06",                                      0x0A, 1 },
        { "client.dll", "56 8B F1 E8 ? ? ? ? C7 06 ? ? ? ? C6 86 ? ? ? ? ? 8B C6 5E C3", 0x0A, 1 },
        { "client.dll", "56 8B F1 E8 ? ? ? ? 89 35 ? ? ? ? C6 86 ? ? ? ? ? 8B C6 5E C3", 0x0A, 1 },
    },
};


inline const pattern_entry_t k_pat_hw_glcolor4f = {
    "hw_glcolor4f",
    {
        { "hw.dll", "F3 0F 11 04 24 FF 15 ? ? ? ? 5D C2 10 00", 7, 0 },
    },
};

inline const pattern_entry_t k_pat_hw_glcolor3f = {
    "hw_glcolor3f",
    {
        { "hw.dll", "F3 0F 11 04 24 FF 15 ? ? ? ? 5D C3", 7, 0 },
    },
};

inline const pattern_entry_t k_pat_key_dest = {
    "key_dest",
    {
        { "hw.dll", "89 1D ? ? ? ? E8 ? ? ? ? 8B 7D 14 83 C4 08 3B FB 75 10 E8", 2, 0 },
        { "hw.dll", "89 1D ? ? ? ? E8 ? ? ? ? 8B 7C 24 24 83 C4 08 3B FB 75 10 E8", 2, 0 },
    },
};

inline const pattern_entry_t k_pat_cls = {
    "cls",
    {
        { "hw.dll", "C7 05 ? ? ? ? 01 00 00 00", 2, 1 },
    },
};

inline const pattern_entry_t k_pat_pmove = {
    "pmove",
    {
        { "hw.dll", "8B 15 ? ? ? ? 8D 0C C5 ? ? ? ? 2B C8 C1 E1 05", 2, 1 },
        { "hw.dll", "8B 15 ? ? ? ? 8D 0C C5 ? ? ? ? 2B C8", 2, 1 },
    },
};
