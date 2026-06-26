#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <iostream>
#include "../shared/shared.h"

static DWORD find_process(const char* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe{}; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32First(snap, &pe)) do {
        if (_stricmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
    } while (Process32Next(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static bool inject(DWORD pid, const std::string& dll_path, DWORD* out_load_error = nullptr)
{
    if (out_load_error)
        *out_load_error = 0;

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) { std::cerr << "OpenProcess failed: " << GetLastError() << "\n"; return false; }

    void* mem = VirtualAllocEx(proc, nullptr, dll_path.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { std::cerr << "VirtualAllocEx failed: " << GetLastError() << "\n"; CloseHandle(proc); return false; }

    WriteProcessMemory(proc, mem, dll_path.c_str(), dll_path.size() + 1, nullptr);

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA")),
        mem, 0, nullptr);
    if (!thread)
    {
        std::cerr << "CreateRemoteThread failed: " << GetLastError() << "\n";
        VirtualFreeEx(proc, mem, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(thread, 8000);

    DWORD remote_module = 0;
    GetExitCodeThread(thread, &remote_module);
    CloseHandle(thread);
    VirtualFreeEx(proc, mem, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (remote_module == 0)
    {
        std::cerr << "LoadLibraryA in target process returned NULL (DLL did not load).\n"
                  << "Common causes: wrong architecture (must be Win32), missing VC runtime, or bad DLL path.\n";
        if (out_load_error)
            *out_load_error = remote_module;
        return false;
    }

    return true;
}

static HANDLE open_shm_mapping(const char** matched_name)
{
    static const char* names[] = {
        SEMTEX_SHARED_NAME,
        "semtex_debug_shm_v11",
        "semtex_debug_shm_v10",
        "semtex_debug_shm_v9",
        "semtex_debug_shm_v8",
        "semtex_debug_shm_v7",
        "semtex_debug_shm_v6",
        "semtex_debug_shm_v4",
        "semtex_debug_shm_v3",
        "semtex_debug_shm_v2",
        "semtex_debug_shm"
    };
    for (const char* name : names)
    {
        HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
        if (h)
        {
            if (matched_name)
                *matched_name = name;
            return h;
        }
    }
    if (matched_name)
        *matched_name = nullptr;
    return nullptr;
}

static const char* team_name(int32_t team)
{
    switch (team)
    {
        case 1: return "T";
        case 2: return "CT";
        default: return "?";
    }
}

static void print_status(const semtex_shared_t* s)
{
    constexpr int k_lines = 39;
    std::cout << "\r\033[" << k_lines << "A";

    if (s->magic != SEMTEX_SHM_MAGIC || s->version < 6u || s->version > SEMTEX_SHM_VERSION)
    {
        std::cout
            << "  *** SHM VERSION MISMATCH - rebuild injector.exe + semtex.dll ***\n"
            << "  magic: 0x" << std::hex << s->magic << " ver: " << std::dec << s->version
            << " (expected 6-" << SEMTEX_SHM_VERSION << ")          \n";
        for (int i = 2; i < k_lines; ++i)
            std::cout << "                                                             \n";
        std::cout.flush();
        return;
    }

    auto yn = [](int32_t v) -> const char* { return v ? "YES" : "NO "; };

    std::cout
        << "  hook fired:     " << yn(s->hook_fired)
        << "   swaps: " << s->swap_call_count << "        \n"
        << "  renderer init:  " << yn(s->renderer_ok)
        << "   menu init: " << yn(s->menu_ok) << "         \n"
        << "  input init:     " << yn(s->input_ok)
        << "   oshgui init: " << yn(s->oshgui_init) << "   \n"
        << "  menu visible:   " << yn(s->menu_visible)
        << "   (INSERT toggles)           \n"
        << "  HWND detected:  0x" << std::hex << s->hwnd << std::dec << "         \n"
        << "  init step:      " << std::dec << s->init_step << "                              \n";

    if (s->version >= 6u)
    {
        std::cout
            << "  game patterns:  cl=" << yn(s->pat_cl)
            << " studio=" << yn(s->pat_studio_api)
            << " client_studio=" << yn(s->pat_client_studio)
            << " hdr=" << yn(s->pat_pstudiohdr)
            << " extra=" << yn(s->pat_extra_info) << "\n"
            << "  studio hooks: remap=" << yn(s->pat_studio_draw)
            << " player=" << yn(s->pat_studio_draw_player) << "\n"
            << "  game_ok:        " << yn(s->game_ok)
            << "   visuals_ok: " << yn(s->visuals_ok) << "         \n";
    }
    else
    {
        std::cout
            << "  (rebuild for pattern/hook debug)                           \n"
            << "                                                             \n"
            << "  game_ok:        " << yn(s->game_ok)
            << "   visuals_ok: " << yn(s->visuals_ok) << "         \n";
    }

    if (s->version >= 13u)
    {
        std::cout
            << "  postruncmd hook: " << yn(s->postruncmd_hook_ok)
            << "   idx: " << s->postruncmd_vtable_index << "       \n"
            << "  recoil cmd match: " << yn(s->recoil_cmd_match)
            << "   punch p/y: " << (s->recoil_punch_pitch / 100.f)
            << " / " << (s->recoil_punch_yaw / 100.f) << "       \n";
    }

    if (s->version >= 12u)
    {
        std::cout
            << "  recoil punch ok: " << yn(s->recoil_punch_valid)
            << "   applies: " << s->recoil_apply_count << "       \n";
    }

    if (s->version >= 11u)
    {
        std::cout
            << "  --- aimbot debug ---                                        \n"
            << "  calc ref hook:  " << yn(s->aimbot_hook_ok)
            << "   enabled: " << yn(s->aimbot_enabled_flag)
            << "   key down: " << yn(s->aimbot_key_down) << "       \n"
            << "  bound key vk:   0x" << std::hex << s->aimbot_bound_key << std::dec
            << "   last fail: " << s->aimbot_last_fail << "                  \n"
            << "  scan enemies:  " << s->aimbot_scan_players
            << "   head ok: " << s->aimbot_head_ok
            << "   targets: " << s->aimbot_target_found << "       \n"
            << "  angles applied: " << s->aimbot_angles_applied
            << "   calc ref calls: " << s->aimbot_calc_ref_calls << "       \n";
    }
    else if (s->version >= 10u)
    {
        std::cout
            << "  --- aimbot debug ---                                        \n"
            << "  calc ref hook:  " << yn(s->aimbot_hook_ok)
            << "   enabled: " << yn(s->aimbot_enabled_flag)
            << "   key down: " << yn(s->aimbot_key_down) << "       \n"
            << "  bound key vk:   0x" << std::hex << s->aimbot_bound_key << std::dec
            << "   last fail: " << s->aimbot_last_fail << "                  \n"
            << "  targets found:  " << s->aimbot_target_found
            << "   angles applied: " << s->aimbot_angles_applied
            << "   calc ref: " << s->aimbot_calc_ref_calls << "       \n";
    }
    else
    {
        std::cout
            << "  (rebuild for aimbot debug - need SHM v10+)                  \n"
            << "                                                             \n"
            << "                                                             \n";
    }

    if (s->version >= 9u)
    {
        std::cout
            << "  pmove:          0x" << std::hex << s->pmove_addr
            << "  flags: 0x" << s->bhop_pm_flags
            << "  ground: " << std::dec << s->bhop_ground << "       \n"
            << "  bhop active:    " << s->bhop_active_hits << " hits with active=1       \n";
    }
    else if (s->version >= 8u)
    {
        std::cout
            << "  movement hook:  " << yn(s->movement_hook_ok)
            << "   createmove calls: " << s->createmove_count << "       \n"
            << "  cl_funcs:       0x" << std::hex << s->cl_funcs_addr
            << "  fn: 0x" << s->createmove_fn_addr << std::dec << "   \n";
    }
    else
    {
        std::cout
            << "  (rebuild for movement debug - need SHM v8+)               \n"
            << "                                                             \n";
    }

    if (s->version >= 7u)
    {
        std::cout
            << "  --- team / ESP debug ---                                   \n"
            << "  local slot:     " << s->local_slot
            << "   team: " << team_name(s->local_team)
            << "   tablist raw: " << s->local_extra_team << "           \n"
            << "  show teammates: " << yn(s->show_teammates)
            << "   cached ESP: " << s->esp_cached
            << "   weapons: " << s->weapon_cached << "       \n"
            << "  draw enemies:   " << s->esp_draw_enemies
            << "   draw teammates: " << s->esp_draw_teammates << "         \n"
            << "  players (*=teammate, -=no weapon):                        \n"
            << "  " << (s->team_snapshot[0] ? s->team_snapshot : "(none yet)") << "\n";
    }
    else
    {
        std::cout
            << "  (rebuild for team/ESP debug - need SHM v7+)                \n"
            << "                                                             \n"
            << "                                                             \n"
            << "                                                             \n"
            << "                                                             \n"
            << "                                                             \n"
            << "                                                             \n";
    }

    std::cout
        << "  last error:     " << (s->last_error[0] ? s->last_error : "(none)") << "                    \n";

    std::cout.flush();
}

int main(int argc, char* argv[])
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    std::string dll_path;
    if (argc > 1)
    {
        dll_path = argv[1];
    }
    else
    {
        char self[MAX_PATH];
        GetModuleFileNameA(nullptr, self, MAX_PATH);
        dll_path = self;
        auto pos = dll_path.find_last_of("\\/");
        if (pos != std::string::npos) dll_path = dll_path.substr(0, pos + 1);
        dll_path += "semtex.dll";
    }

    std::cout << "semtex injector\n";
    std::cout << "DLL: " << dll_path << "\n\n";

    if (GetFileAttributesA(dll_path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        std::cerr << "DLL not found: " << dll_path << "\n";
        std::cin.get(); return 1;
    }

    const char* targets[] = { "hl.exe", "cs.exe", "cstrike.exe" };
    DWORD pid = 0;
    std::cout << "Waiting for CS 1.6 process...\n";
    while (!pid)
    {
        for (auto& name : targets) { pid = find_process(name); if (pid) { std::cout << "Found " << name << " (PID " << pid << ")\n"; break; } }
        if (!pid) Sleep(500);
    }

    std::cout << "Injecting " << dll_path << " ...\n";
    if (!inject(pid, dll_path))
    {
        std::cerr << "Injection failed.\n";
        std::cin.get();
        return 1;
    }
    std::cout << "Injected! Waiting for DLL shared memory (" << SEMTEX_SHARED_NAME << ")...\n";

    HANDLE shm_handle = nullptr;
    semtex_shared_t* shm = nullptr;
    const char* shm_name = nullptr;
    for (int i = 0; i < 50; i++)
    {
        shm_handle = open_shm_mapping(&shm_name);
        if (shm_handle)
            break;
        Sleep(100);
    }

    if (!shm_handle)
    {
        std::cout << "\nERROR: Shared memory not found after 5s.\n"
                  << "Expected: " << SEMTEX_SHARED_NAME << "\n"
                  << "The DLL likely failed to load or DllMain crashed before creating SHM.\n"
                  << "Make sure semtex.dll and injector.exe were rebuilt together (Release|Win32).\n";
        std::cin.get();
        return 1;
    }

    if (shm_name && strcmp(shm_name, SEMTEX_SHARED_NAME) != 0)
    {
        std::cout << "WARNING: DLL is using older SHM name \"" << shm_name
                  << "\". Rebuild semtex.dll to match injector.\n";
    }

    shm = reinterpret_cast<semtex_shared_t*>(MapViewOfFile(shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(semtex_shared_t)));
    if (!shm)
    {
        std::cout << "MapViewOfFile failed: " << GetLastError() << "\n";
        std::cin.get(); return 1;
    }

    std::cout << "\nLive debug (updates every 500ms) -- press Ctrl+C to exit injector:\n\n";

    for (int i = 0; i < 28; i++) std::cout << "\n";

    while (true)
    {
        if (find_process("hl.exe") == 0 && find_process("cs.exe") == 0 && find_process("cstrike.exe") == 0)
        {
            std::cout << "\nGame process exited.\n";
            break;
        }

        print_status(shm);
        Sleep(500);
    }

    UnmapViewOfFile(shm);
    CloseHandle(shm_handle);
    std::cin.get();
    return 0;
}
