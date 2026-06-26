#include "offset_dump.h"

#include <Windows.h>

static DWORD WINAPI dump_thread(LPVOID param)
{
    const HMODULE self = reinterpret_cast<HMODULE>(param);

    for (int i = 0; i < 100; ++i)
    {
        if (GetModuleHandleA("hw.dll") && GetModuleHandleA("client.dll"))
            break;
        Sleep(100);
    }


    Sleep(1000);

    offset_dump::run();

    Sleep(250);
    FreeLibraryAndExitThread(self, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(self);
        HANDLE t = CreateThread(nullptr, 0, dump_thread, self, 0, nullptr);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}
