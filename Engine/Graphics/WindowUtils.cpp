#include "WindowUtils.h"
#include <windows.h>

void WindowUtils::setFullscreenBorderless(int screenWidth, int screenHeight)
{
    HWND hwnd = FindWindowA(NULL, "Engine");
    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
}

void WindowUtils::setFullscreen()
{
    HWND hwnd = FindWindowA(NULL, "Engine");
    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
    {
        SetWindowPos(hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_SHOWWINDOW);
    }
}

void WindowUtils::setWindowed(int width, int height)
{
    HWND hwnd = FindWindowA(NULL, "Engine");
    SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOP,
                 (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
                 width, height,
                 SWP_SHOWWINDOW);
}

int WindowUtils::getFOV()
{
    return FOV;
}

void WindowUtils::setFOV(int fov)
{
    FOV = fov;
}