#include "inc.h"
#include "../shared/shared.h"
#include "features/visuals.h"
#include "features/movement.h"

c_renderer g_renderer{};
c_menu     g_menu{};
c_input    g_input{};
c_hooks    g_hooks{};

static HMODULE g_module = nullptr;


static HANDLE           s_shm_handle = nullptr;
static semtex_shared_t* s_shm        = nullptr;

semtex_shared_t* g_shm()  { return s_shm; }

static void shm_create()
{
    s_shm_handle = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                      0, sizeof(semtex_shared_t), SEMTEX_SHARED_NAME);
    if (s_shm_handle)
        s_shm = reinterpret_cast<semtex_shared_t*>(
            MapViewOfFile(s_shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(semtex_shared_t)));
    if (s_shm)
    {
        ZeroMemory(s_shm, sizeof(semtex_shared_t));
        s_shm->magic = SEMTEX_SHM_MAGIC;
        s_shm->version = SEMTEX_SHM_VERSION;
    }
}

static void shm_destroy()
{
    if (s_shm)        { UnmapViewOfFile(s_shm);        s_shm = nullptr; }
    if (s_shm_handle) { CloseHandle(s_shm_handle);     s_shm_handle = nullptr; }
}

static void shm_log_init(const char* msg)
{

    if (s_shm && !s_shm->hook_fired)
        strcpy_s(s_shm->last_error, msg);
}

static void unload()
{
    g_input.remove();
    movement::shutdown_hooks();
    visuals::shutdown_engine_hooks();
    g_hooks.release();
    Sleep(200);
    shm_destroy();
}

static DWORD WINAPI cheat_init(LPVOID)
{
    shm_log_init("thread started, waiting for SDL_app window");


    int waited = 0;
    while (!FindWindowA("SDL_app", nullptr))
    {
        Sleep(200);
        waited += 200;
        if (waited > 30000)
        {
            shm_log_init("TIMEOUT: SDL_app window never appeared (>30s)");
            FreeLibraryAndExitThread(g_module, 0);
        }
    }

    shm_log_init("SDL_app found, sleeping 300ms then hooking");
    Sleep(300);

    if (!g_hooks.init())
        FreeLibraryAndExitThread(g_module, 0);

    shm_log_init("hooks installed, waiting for wglSwapBuffers (ESP inits when hw.dll loads)");
    return 1;
}

static DWORD WINAPI cheat_free(LPVOID)
{
    while (!(GetAsyncKeyState(VK_END) & 0x8000))
        Sleep(50);

    unload();
    FreeLibraryAndExitThread(g_module, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason, LPVOID)
{
    if (ul_reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        g_module = hModule;


        shm_create();
        if (s_shm) strcpy_s(s_shm->last_error, "DllMain attached, threads starting");

        CloseHandle(CreateThread(nullptr, 0, cheat_init, nullptr, 0, nullptr));
        CloseHandle(CreateThread(nullptr, 0, cheat_free, nullptr, 0, nullptr));
    }
    else if (ul_reason == DLL_PROCESS_DETACH)
    {
        shm_destroy();
    }
    return TRUE;
}
