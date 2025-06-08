#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <list>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Easy.hpp>

#include <json/json.h> // JsonCpp

#define IDT_TIMER1 1
#define UPDATE_INTERVAL_MS 60000

// Globals
HWND g_hOverlayWnd = NULL;
HWND g_hOptionsWnd = NULL;
HWND g_hCheckboxMove = NULL;
HWND g_hCheckboxSquare = NULL;

bool isInteractive = false;
bool showSquare = true;

std::wstring overlayText = L"Chargement...";

void LogToFile(const std::string& message) {
    std::ofstream logFile("overlay_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
}

std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string MakeAPIRequestCurlpp(const std::string& url, const std::string& token) {
    try {
        curlpp::Cleanup cleanup;
        curlpp::Easy request;

        std::ostringstream responseStream;

        request.setOpt(curlpp::options::Url(url));

        std::list<std::string> headers;
        headers.push_back("Authorization: Bearer " + token);
        request.setOpt(curlpp::options::HttpHeader(headers));

        request.setOpt(curlpp::options::WriteStream(&responseStream));

        LogToFile("URL utilisée : " + url);
        LogToFile("Headers envoyés : Authorization: Bearer " + token);

        request.perform();

        std::string response = responseStream.str();

        LogToFile("Réponse JSON brute : " + response);

        return response;
    }
    catch (curlpp::RuntimeError& e) {
        std::string err = std::string("RuntimeError curlpp: ") + e.what();
        LogToFile(err);
    }
    catch (curlpp::LogicError& e) {
        std::string err = std::string("LogicError curlpp: ") + e.what();
        LogToFile(err);
    }
    return "";
}

void UpdateOverlayData() {
    std::string token = "c54ea814-43ef-11f0-bf0c-020112c3dc7f";
    std::string itemUid = "platinumgolem";
    std::string url = "https://union-titans.fr/api/items/detail/" + itemUid + "/json";

    std::string json = MakeAPIRequestCurlpp(url, token);
    if (json.empty()) {
        OutputDebugString(L"Erreur: réponse API vide\n");
        return;
    }

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errs;
    std::istringstream s(json);

    if (!Json::parseFromStream(builder, s, &root, &errs)) {
        std::string err = "Erreur parsing JSON : " + errs + "\n";
        OutputDebugStringA(err.c_str());
        LogToFile(err);
        return;
    }

    if (root.isMember("error")) {
        std::string errorMsg = root["error"].asString();
        std::string err = "API error: " + errorMsg;
        OutputDebugStringA(err.c_str());
        LogToFile(err);
        return;
    }

    if (!root.isMember("data")) {
        OutputDebugString(L"Erreur: structure JSON inattendue (pas de 'data')\n");
        LogToFile("Erreur: structure JSON inattendue (pas de 'data')");
        return;
    }

    const Json::Value& data = root["data"];

    if (!data.isMember("label") || !data["label"].isString()) {
        OutputDebugString(L"Erreur: structure JSON inattendue (label manquant)\n");
        LogToFile("Erreur: structure JSON inattendue (label manquant)");
        return;
    }

    std::string labelUtf8 = data["label"].asString();
    std::wstring label = Utf8ToWstring(labelUtf8);

    const Json::Value* marketEntry = nullptr;
    if (data.isMember("last_market_data") && data["last_market_data"].isArray()) {
        for (const auto& entry : data["last_market_data"]) {
            if (entry.isMember("quality") && entry["quality"].asString() == "common") {
                marketEntry = &entry;
                break;
            }
        }
    }

    if (!marketEntry) {
        overlayText = label + L"\nPas de donnees marcher pour qualite 'common'.";
        return;
    }

    int prixOffre = -1;
    int prixDemande = -1;

    if (marketEntry->isMember("gold_price") && (*marketEntry)["gold_price"].isInt()) {
        int prixBase = (*marketEntry)["gold_price"].asInt();
        prixOffre = static_cast<int>(prixBase * 0.9);
        prixDemande = static_cast<int>(prixBase * 1.1);
    }

    auto formatNombre = [](int value) -> std::wstring {
        std::wstring str = std::to_wstring(value);
        int pos = (int)str.size() - 3;
        while (pos > 0) {
            str.insert(pos, L" ");
            pos -= 3;
        }
        return str;
        };

    std::wstring offreStr = (prixOffre >= 0) ? formatNombre(prixOffre) : L"inconnu";
    std::wstring demandeStr = (prixDemande >= 0) ? formatNombre(prixDemande) : L"inconnu";

    std::wstring resultat;
    if (prixOffre >= 0 && prixDemande >= 0) {
        int diff = prixOffre - prixDemande;
        resultat = (diff > 0)
            ? L"Benefice : +" + formatNombre(diff) + L" gold"
            : L"A perte";
    }
    else {
        resultat = L"Donnees insuffisantes";
    }

    overlayText = L"Objet : " + label + L"\n"
        + L"Prix offre (common) : " + offreStr + L"\n"
        + L"Prix demande (common) : " + demandeStr + L"\n"
        + resultat;
}

// Fenêtre Options
LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        g_hCheckboxMove = CreateWindowEx(0, L"BUTTON", L"Déplacer l'overlay (clic gauche)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 20, 250, 25,
            hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL);

        SendMessage(g_hCheckboxMove, BM_SETCHECK, isInteractive ? BST_CHECKED : BST_UNCHECKED, 0);

        g_hCheckboxSquare = CreateWindowEx(0, L"BUTTON", L"Cacher le carré blanc",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 60, 250, 25,
            hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL);

        SendMessage(g_hCheckboxSquare, BM_SETCHECK, showSquare ? BST_UNCHECKED : BST_CHECKED, 0);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == BN_CLICKED) {
            LRESULT state = SendMessage(g_hCheckboxMove, BM_GETCHECK, 0, 0);
            isInteractive = (state == BST_CHECKED);
            InvalidateRect(g_hOverlayWnd, NULL, TRUE);
        }
        else if (LOWORD(wParam) == 1002 && HIWORD(wParam) == BN_CLICKED) {
            LRESULT state = SendMessage(g_hCheckboxSquare, BM_GETCHECK, 0, 0);
            showSquare = (state == BST_UNCHECKED);
            InvalidateRect(g_hOverlayWnd, NULL, TRUE);
        }
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

POINT dragOffset;
bool isDragging = false;

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        SetTimer(hwnd, IDT_TIMER1, UPDATE_INTERVAL_MS, NULL);
        UpdateOverlayData();
        return 0;

    case WM_TIMER:
        if (wParam == IDT_TIMER1) {
            UpdateOverlayData();
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        // Fond magenta utilisé comme couleur transparente
        HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 255));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        // Texte overlay
        DrawText(hdc, overlayText.c_str(), -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);

        // Petit carré blanc si activé
        if (showSquare) {
            RECT sq = { 10, 150, 30, 170 };
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &sq, whiteBrush);
            DeleteObject(whiteBrush);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
        if (isInteractive) {
            isDragging = true;
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            dragOffset = pt;
            SetCapture(hwnd);
        }
        else {
            // Clic gauche passe à la fenêtre en dessous (clic "translucide")
            POINT ptScreen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &ptScreen);

            HWND hwndBelow = WindowFromPoint(ptScreen);

            if (hwndBelow && hwndBelow != hwnd) {
                POINT ptClient = ptScreen;
                ScreenToClient(hwndBelow, &ptClient);
                LPARAM lParamNew = MAKELPARAM(ptClient.x, ptClient.y);

                PostMessage(hwndBelow, WM_LBUTTONDOWN, wParam, lParamNew);
                PostMessage(hwndBelow, WM_LBUTTONUP, 0, lParamNew);
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (isDragging) {
            POINT ptScreen;
            GetCursorPos(&ptScreen);
            SetWindowPos(hwnd, NULL,
                ptScreen.x - dragOffset.x,
                ptScreen.y - dragOffset.y,
                0, 0,
                SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;

    case WM_LBUTTONUP:
        if (isDragging) {
            isDragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (g_hOptionsWnd) {
            ShowWindow(g_hOptionsWnd, SW_SHOW);
            SetForegroundWindow(g_hOptionsWnd);
        }
        return 0;

    case WM_NCHITTEST:
        if (isInteractive) {
            return HTCAPTION;
        }
        else {
            return HTCLIENT;  // important pour capter clics droit
        }

    case WM_DESTROY:
        KillTimer(hwnd, IDT_TIMER1);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Register Overlay Window Class
    WNDCLASS wcOverlay = {};
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.lpszClassName = L"OverlayWindowClass";
    wcOverlay.hbrBackground = NULL; // pas de fond par défaut

    RegisterClass(&wcOverlay);

    // Register Options Window Class
    WNDCLASS wcOptions = {};
    wcOptions.lpfnWndProc = OptionsWndProc;
    wcOptions.hInstance = hInstance;
    wcOptions.lpszClassName = L"OptionsWindowClass";
    wcOptions.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    RegisterClass(&wcOptions);

    // Create Options Window (hidden by default)
    g_hOptionsWnd = CreateWindowEx(
        0,
        wcOptions.lpszClassName,
        L"Options",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 150,
        NULL, NULL, hInstance, NULL);

    ShowWindow(g_hOptionsWnd, SW_HIDE);

    // Create Overlay Window
    g_hOverlayWnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_APPWINDOW, // plus de WS_EX_TRANSPARENT !
        wcOverlay.lpszClassName,
        L"Overlay",
        WS_POPUP,
        100, 100, 400, 300,
        NULL, NULL, hInstance, NULL);

    if (!g_hOverlayWnd) {
        MessageBox(NULL, L"Erreur création fenêtre overlay", L"Erreur", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Définir la couleur magenta (255,0,255) comme transparente
    SetLayeredWindowAttributes(g_hOverlayWnd, RGB(255, 0, 255), 0, LWA_COLORKEY);

    ShowWindow(g_hOverlayWnd, nCmdShow);
    UpdateWindow(g_hOverlayWnd);

    // Boucle de messages
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
