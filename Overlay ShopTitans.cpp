#include <windows.h>
#include <windowsx.h>
#include <string>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Texte à afficher
std::wstring overlayText = L"Argent : 9999\nXP : 123456";

// Variables globales pour le déplacement
POINT clickPos;
bool isDragging = false;
// Contrôle du mode interactif
bool isInteractive = false;

// GDI+ token global
ULONG_PTR gdiplusToken;

// Fonction utilitaire pour laisser passer le clic gauche à la fenêtre sous-jacente
void PassThroughClick(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    POINT ptScreen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ClientToScreen(hwnd, &ptScreen);

    HWND hwndBelow = WindowFromPoint(ptScreen);
    if (hwndBelow && hwndBelow != hwnd) {
        POINT ptClient = ptScreen;
        ScreenToClient(hwndBelow, &ptClient);

        LPARAM newLParam = MAKELPARAM(ptClient.x, ptClient.y);

        PostMessage(hwndBelow, WM_LBUTTONDOWN, wParam, newLParam);
        PostMessage(hwndBelow, WM_LBUTTONUP, 0, newLParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Initialisation GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    const wchar_t CLASS_NAME[] = L"OverlayWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
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

    if (!hwnd) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    }

    // Fond noir transparent via colorkey
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Arrêt GDI+
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        if (isInteractive) {
            // Fond noir semi-transparent avec GDI+
            Gdiplus::Graphics graphics(hdc);
            Gdiplus::SolidBrush semiTransparentBrush(Gdiplus::Color(128, 0, 0, 0)); // 128 = 50% alpha
            graphics.FillRectangle(&semiTransparentBrush, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
        }
        else {
            // Fond noir transparent (via colorkey)
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }

        // Texte vert avec fond transparent
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 255, 0));
        DrawText(hdc, overlayText.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_WORDBREAK);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!isInteractive) {
            PassThroughClick(hwnd, wParam, lParam);
            return 0;
        }
        isDragging = true;
        GetCursorPos(&clickPos);
        SetCapture(hwnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (isDragging) {
            POINT newPos;
            GetCursorPos(&newPos);
            int dx = newPos.x - clickPos.x;
            int dy = newPos.y - clickPos.y;

            RECT rect;
            GetWindowRect(hwnd, &rect);
            MoveWindow(hwnd, rect.left + dx, rect.top + dy,
                rect.right - rect.left, rect.bottom - rect.top, TRUE);

            clickPos = newPos;
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (!isInteractive) {
            return 0;
        }
        isDragging = false;
        ReleaseCapture();
        return 0;
    }

    case WM_RBUTTONDOWN: {
        HMENU hMenu = CreatePopupMenu();
        AppendMenu(hMenu, MF_STRING, 1, L"Minimiser");
        AppendMenu(hMenu, MF_STRING, 2, L"Fermer");
        if (isInteractive)
            AppendMenu(hMenu, MF_STRING, 3, L"\u2714 Repasser en mode transparent");
        else
            AppendMenu(hMenu, MF_STRING, 3, L"\u25CB Activer le deplacement");

        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);

        if (cmd == 1) {
            ShowWindow(hwnd, SW_MINIMIZE);
        }
        else if (cmd == 2) {
            PostQuitMessage(0);
        }
        else if (cmd == 3) {
            isInteractive = !isInteractive;
            InvalidateRect(hwnd, NULL, TRUE);
        }

        return 0;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}