#include <windows.h>
#include <stdio.h>
#include <string.h>
BOOL CALLBACK EnumProc(HWND hWnd, LPARAM lp) {
    char cls[256], title[256];
    GetClassNameA(hWnd, cls, 256);
    GetWindowTextA(hWnd, title, 256);
    if (strlen(title) > 0)
        printf("Class:[%-36s] Title:[%s]\n", cls, title);
    return TRUE;
}
int main() { EnumWindows(EnumProc, 0); return 0; }
