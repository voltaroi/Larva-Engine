#include "WindowUtils.h"
#include <windows.h>

namespace
{
    // Plein écran exclusif : la procédure de la fenêtre est interceptée pour rendre l'écran au bureau quand le jeu perd
    // le focus (Alt+Tab), et le reprendre à son retour
    WNDPROC originalProc = nullptr;
    HWND exclusiveHwnd = nullptr;
    bool exclusiveOn = false, displayTaken = false;

    bool monitorOf(HWND hwnd, MONITORINFOEXA &mi)
    {
        mi.cbSize = sizeof(mi);
        return GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi) != 0;
    }

    // L'écran passe en mode plein écran (à sa définition actuelle), la fenêtre le couvre, au-dessus de tout
    void takeDisplay(HWND hwnd)
    {
        MONITORINFOEXA mi;
        if (!monitorOf(hwnd, mi))
            return;
        DEVMODEA dm = {};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsA(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        {
            dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
            displayTaken = ChangeDisplaySettingsExA(mi.szDevice, &dm, NULL, CDS_FULLSCREEN, NULL) == DISP_CHANGE_SUCCESSFUL;
            monitorOf(hwnd, mi);
        }
        SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    }

    void releaseDisplay(HWND hwnd)
    {
        if (displayTaken)
            ChangeDisplaySettingsExA(NULL, NULL, NULL, 0, NULL);
        displayTaken = false;
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    LRESULT CALLBACK exclusiveProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (exclusiveOn && hwnd == exclusiveHwnd)
        {
            if (msg == WM_ACTIVATEAPP)
            {
                if (wParam)
                {
                    if (IsIconic(hwnd))
                        ShowWindow(hwnd, SW_RESTORE);
                    takeDisplay(hwnd);
                }
                else
                {
                    releaseDisplay(hwnd);
                    ShowWindow(hwnd, SW_MINIMIZE);
                }
            }
            else if (msg == WM_DESTROY)
                releaseDisplay(hwnd);
        }
        return CallWindowProcA(originalProc, hwnd, msg, wParam, lParam);
    }

    // Barre de titre sombre (Windows 10 1809+ / 11) : dwmapi chargée à la volée, sans effet sur les Windows plus anciens
    void darkTitleBar(HWND hwnd)
    {
        static HMODULE dwm = LoadLibraryA("dwmapi.dll");
        if (!dwm)
            return;
        typedef HRESULT(WINAPI * SetAttr)(HWND, DWORD, LPCVOID, DWORD);
        static SetAttr set = (SetAttr)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (!set)
            return;
        BOOL dark = TRUE;
        if (FAILED(set(hwnd, 20, &dark, sizeof(dark)))) // DWMWA_USE_IMMERSIVE_DARK_MODE
            set(hwnd, 19, &dark, sizeof(dark));         // même attribut avant Windows 10 20H1
        COLORREF caption = RGB(13, 13, 15), text = RGB(200, 200, 205);
        set(hwnd, 35, &caption, sizeof(caption)); // DWMWA_CAPTION_COLOR (Windows 11)
        set(hwnd, 36, &text, sizeof(text));       // DWMWA_TEXT_COLOR
        set(hwnd, 34, &caption, sizeof(caption)); // DWMWA_BORDER_COLOR
    }

    HWND findWindow(const char *name)
    {
        HWND hwnd = FindWindowA(NULL, name);
        return hwnd ? hwnd : WindowFromDC(wglGetCurrentDC());
    }
}

void WindowUtils::setFullscreenBorderless(int screenWidth, int screenHeight)
{
    HWND hwnd = FindWindowA(NULL, name);
    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
}

void WindowUtils::setFullscreen()
{
    HWND hwnd = FindWindowA(NULL, name);
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
    HWND hwnd = FindWindowA(NULL, name);
    SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(hwnd, HWND_TOP,
                 (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                 (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
                 width, height,
                 SWP_SHOWWINDOW);
}

void WindowUtils::setWindowMode(WindowMode newMode, int windowedWidth, int windowedHeight)
{
    if (windowedWidth > 0 && windowedHeight > 0)
        windowedW = windowedWidth, windowedH = windowedHeight;
    mode = newMode;
    HWND hwnd = findWindow(name);
    if (!hwnd)
        return;
    // On quitte le plein écran exclusif : l'écran revient au bureau, la fenêtre n'est plus au-dessus de tout
    if (exclusiveOn && newMode != WindowMode::Fullscreen)
    {
        exclusiveOn = false;
        releaseDisplay(hwnd);
    }
    MONITORINFOEXA mi;
    const bool hasMonitor = monitorOf(hwnd, mi);
    switch (newMode)
    {
    case WindowMode::Windowed:
    {
        // La fenêtre (zone cliente de windowedW x windowedH, sans dépasser l'écran) centrée sur son écran
        const RECT work = hasMonitor ? mi.rcWork : RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        RECT r = {0, 0, windowedW, windowedH};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        int ww = r.right - r.left, wh = r.bottom - r.top;
        if (ww > work.right - work.left)
            ww = work.right - work.left;
        if (wh > work.bottom - work.top)
            wh = work.bottom - work.top;
        darkTitleBar(hwnd);
        SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_NOTOPMOST, work.left + (work.right - work.left - ww) / 2, work.top + (work.bottom - work.top - wh) / 2, ww, wh,
                     SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        break;
    }
    case WindowMode::Borderless:
    {
        const RECT mon = hasMonitor ? mi.rcMonitor : RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        break;
    }
    case WindowMode::Fullscreen:
    default:
        if (!originalProc)
            originalProc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)exclusiveProc);
        exclusiveHwnd = hwnd;
        exclusiveOn = true;
        takeDisplay(hwnd);
        break;
    }
}

const char* WindowUtils::windowModeName(WindowMode m)
{
    switch (m)
    {
    case WindowMode::Windowed:
        return "FENETRE";
    case WindowMode::Borderless:
        return "PLEIN ECRAN FENETRE";
    default:
        return "PLEIN ECRAN";
    }
}

int WindowUtils::getFOV()
{
    return FOV;
}

void WindowUtils::setFOV(int fov)
{
    FOV = fov;
}

void WindowUtils::setName(const char* newName)
{
    name = newName;
    glutSetWindowTitle(name);
}
