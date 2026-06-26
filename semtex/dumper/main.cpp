#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <iostream>
#include <string>

static DWORD find_process(const char* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;

    if (Process32First(snap, &pe))
    {
        do
        {
            if (_stricmp(pe.szExeFile, name) == 0)
            {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &pe));
    }

    CloseHandle(snap);
    return pid;
}

static bool inject(DWORD pid, const std::string& dll_path)
{
    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc)
    {
        std::cerr << "OpenProcess failed: " << GetLastError() << "\n";
        return false;
    }

    void* mem = VirtualAllocEx(proc, nullptr, dll_path.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem)
    {
        std::cerr << "VirtualAllocEx failed: " << GetLastError() << "\n";
        CloseHandle(proc);
        return false;
    }

    WriteProcessMemory(proc, mem, dll_path.c_str(), dll_path.size() + 1, nullptr);

    HANDLE thread = CreateRemoteThread(
        proc,
        nullptr,
        0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA")),
        mem,
        0,
        nullptr);

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
        std::cerr << "LoadLibraryA in target process returned NULL.\n"
                  << "Use Release|Win32 builds and keep dumper.dll next to Dumper.exe.\n";
        return false;
    }

    return true;
}

static std::string sibling_dll_path(const char* dll_name)
{
    char self[MAX_PATH]{};
    GetModuleFileNameA(nullptr, self, MAX_PATH);

    std::string path = self;
    const auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos)
        path = path.substr(0, pos + 1);
    path += dll_name;
    return path;
}

static bool wait_for_dump_file(const std::string& path, int timeout_ms)
{
    for (int elapsed = 0; elapsed < timeout_ms; elapsed += 100)
    {
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad))
        {
            if (fad.nFileSizeLow > 64)
                return true;
        }
        Sleep(100);
    }
    return false;
}

static std::string dump_output_path()
{
    return sibling_dll_path("semtex_offset_dump.txt");
}

int main(int argc, char* argv[])
{
    std::string dll_path;
    if (argc > 1)
        dll_path = argv[1];
    else
        dll_path = sibling_dll_path("dumper.dll");

    std::cout << "semtex offset dumper\n";
    std::cout << "Payload: " << dll_path << "\n\n";

    if (GetFileAttributesA(dll_path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        std::cerr << "dumper.dll not found: " << dll_path << "\n";
        std::cin.get();
        return 1;
    }

    const char* targets[] = { "cs.exe", "hl.exe", "cstrike.exe" };
    DWORD pid = 0;

    std::cout << "Waiting for CS 1.6 process...\n";
    while (!pid)
    {
        for (const char* name : targets)
        {
            pid = find_process(name);
            if (pid)
            {
                std::cout << "Found " << name << " (PID " << pid << ")\n";
                break;
            }
        }
        if (!pid)
            Sleep(500);
    }

    std::cout << "Injecting dumper.dll ...\n";
    if (!inject(pid, dll_path))
    {
        std::cerr << "Injection failed.\n";
        std::cin.get();
        return 1;
    }

    const std::string dump_path = dump_output_path();
    std::cout << "Waiting for dump file...\n";

    if (wait_for_dump_file(dump_path, 15000))
    {
        std::cout << "\nDump complete:\n  " << dump_path << "\n";
        std::cout << "\nOpen that file in Notepad to read all offsets.\n";
    }
    else
    {
        std::cout << "\nTimed out waiting for:\n  " << dump_path << "\n"
                  << "If the game was still loading, run Dumper.exe again from the main menu.\n";
    }

    std::cin.get();
    return 0;
}
