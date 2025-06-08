#include <windows.h>
#include "OverlayWindow.h"
#include "Utils.h"
#include <fstream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Vider log au démarrage
    {
        std::ofstream logFile("overlay_log.txt", std::ios::trunc);
    }

    const wchar_t CLASS_NAME[] = L"OverlayWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,
        CLASS_NAME,
        L"Overlay ShopTitans",
        WS_POPUP,
        100, 100, 400, 200,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    UpdateOverlayData();

    ShowWindow(hwnd, nCmdShow);

    SetTimer(hwnd, 1, 60000, NULL); // IDT_TIMER1 = 1

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
