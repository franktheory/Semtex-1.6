#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct SDL_Window;

struct SDL_version
{
    unsigned char major;
    unsigned char minor;
    unsigned char patch;
};

enum SDL_SYSWM_TYPE
{
    SDL_SYSWM_UNKNOWN,
    SDL_SYSWM_WINDOWS,
};

struct SDL_SysWMinfo
{
    SDL_version version;
    SDL_SYSWM_TYPE subsystem;
    union
    {
        struct
        {
            HWND window;
        } win;
    } info;
};

struct SDL_Event
{
    unsigned char data[128];
};

using SDL_GetWindowWMInfo_t = int(__cdecl*)(SDL_Window*, SDL_SysWMinfo*);
using SDL_SetRelativeMouseMode_t = int(__cdecl*)(int);
using SDL_GetRelativeMouseMode_t = int(__cdecl*)();
using SDL_ShowCursor_t = int(__cdecl*)(int);

inline void SDL_VERSION(SDL_version* v)
{
    v->major = 2;
    v->minor = 0;
    v->patch = 0;
}
