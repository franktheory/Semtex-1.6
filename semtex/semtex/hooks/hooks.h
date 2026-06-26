#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using wglSwapBuffers_t = BOOL(WINAPI*)(HDC);

class c_hooks
{
public:
    wglSwapBuffers_t m_orig_swap = nullptr;

    bool init();
    bool release();
};

namespace hook
{
    BOOL WINAPI wglSwapBuffers(HDC hdc);
}
