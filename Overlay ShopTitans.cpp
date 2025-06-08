#include <windows.h> 
#include <windowsx.h>  // <-- Important pour GET_X_LPARAM, GET_Y_LPARAM
#include <string>
#include <wininet.h>
#include <json/json.h> // Pour JSONCPP (lib à installer)
#pragma comment(lib, "wininet.lib")
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>

#define IDT_TIMER1 1
#define UPDATE_INTERVAL_MS 60000  // 1 minute

void LogToFile(const std::string& message) {
    std::ofstream logFile("overlay_log.txt", std::ios::app); // "a" = append
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Texte à afficher
std::wstring overlayText = L"Argent : 9999\nXP : 123456";

// Variables globales pour le déplacement
POINT clickPos;
bool isDragging = false;
// Contrôle du mode interactif
bool isInteractive = false;

// Déclaration globale URL-encodée du PlayerUID
const std::string playerUid = "TheLynkYT%2349851"; // # encodé en %23

// Fonction utilitaire pour laisser passer le clic gauche à la fenêtre sous-jacente
void PassThroughClick(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    // Coordonnées écran du clic
    POINT ptScreen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ClientToScreen(hwnd, &ptScreen);

    // Trouver la fenêtre sous le curseur (autre que la fenêtre overlay)
    HWND hwndBelow = WindowFromPoint(ptScreen);
    if (hwndBelow && hwndBelow != hwnd) {
        // Convertir coordonnées écran en client pour la fenêtre cible
        POINT ptClient = ptScreen;
        ScreenToClient(hwndBelow, &ptClient);

        LPARAM newLParam = MAKELPARAM(ptClient.x, ptClient.y);

        // Envoyer clic gauche down + up à la fenêtre cible
        PostMessage(hwndBelow, WM_LBUTTONDOWN, wParam, newLParam);
        PostMessage(hwndBelow, WM_LBUTTONUP, 0, newLParam);
    }
}

// Fonction pour encoder une chaîne en URL-safe (utile si besoin)
std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        }
        else {
            escaped << '%' << std::uppercase << std::setw(2) << int((unsigned char)c);
        }
    }

    return escaped.str();
}

std::string MakeAPIRequest(const std::string& url, const std::string& token) {
    HINTERNET hInternet = InternetOpen(L"ShopTitansOverlay", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    std::string headers = "Authorization: Bearer " + token + "\r\n";

    HINTERNET hFile = InternetOpenUrlA(
        hInternet, url.c_str(),
        headers.c_str(), (DWORD)headers.length(),
        INTERNET_FLAG_RELOAD, 0
    );

    if (!hFile) {
        InternetCloseHandle(hInternet);
        return "";
    }

    char buffer[4096];
    DWORD bytesRead;
    std::string result;

    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead) {
        result.append(buffer, bytesRead);
    }

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);
    return result;
}

std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

void UpdateOverlayData() {
    LogToFile("UpdateOverlayData called");

    std::string token = "c54ea814-43ef-11f0-bf0c-020112c3dc7f";
    std::string playerUid = "TheLynkYT%2349851"; // Note : # doit être URL-encodé en %23
    std::string url = "https://union-titans.fr/api/player/current/" + playerUid + "/json";

    std::string json = MakeAPIRequest(url, token);

    LogToFile("Réponse JSON brute : " + json);

    if (json.empty()) {
        LogToFile("Erreur: réponse API vide");
        return;
    }

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errs;
    std::istringstream s(json);

    if (!Json::parseFromStream(builder, s, &root, &errs)) {
        OutputDebugStringA(("Erreur parsing JSON : " + errs + "\n").c_str());
        return;
    }

    // Vérifier la présence et le type des champs attendus
    if (!root.isMember("name") || !root["name"].isString() ||
        !root.isMember("gold") || !root["gold"].isInt() ||
        !root.isMember("xp") || !root["xp"].isInt()) {
        OutputDebugString(L"Erreur: structure JSON inattendue\n");
        return;
    }

    std::string nameUtf8 = root["name"].asString();
    std::wstring name = Utf8ToWstring(nameUtf8);
    int gold = root["gold"].asInt();
    int xp = root["xp"].asInt();

    overlayText = L"Joueur : " + name + L"\nOr : " + std::to_wstring(gold) + L"\nXP : " + std::to_wstring(xp);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"OverlayWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST,  // On retire WS_EX_TRANSPARENT pour capter clic droit
        CLASS_NAME,
        L"Overlay ShopTitans",
        WS_POPUP,
        100, 100, 400, 200,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 0;

    // Fond noir transparent via colorkey
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    // --- Suppression des doublons playerUid ici ---
    // Juste un appel d'update initial
    UpdateOverlayData();

    ShowWindow(hwnd, nCmdShow);

    SetTimer(hwnd, IDT_TIMER1, UPDATE_INTERVAL_MS, NULL);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

    case WM_TIMER: {
        if (wParam == IDT_TIMER1) {
            UpdateOverlayData();
            InvalidateRect(hwnd, NULL, TRUE); // Repaint la fenêtre
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Fond noir transparent (via colorkey)
        FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

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
            AppendMenu(hMenu, MF_STRING, 3, L"Repasser en mode transparent");
        else
            AppendMenu(hMenu, MF_STRING, 3, L"Activer le deplacement");

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
