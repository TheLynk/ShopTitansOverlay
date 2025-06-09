#pragma comment(lib, "comctl32.lib")

#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
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

// Fréquence update min/max en ms
#define UPDATE_INTERVAL_MIN_MS 10000
#define UPDATE_INTERVAL_MAX_MS 60000

// Globals
HWND g_hOverlayWnd = NULL;
HWND g_hOptionsWnd = NULL;
HWND g_hCheckboxMove = NULL;
HWND g_hCheckboxSquare = NULL;
HWND g_hEditToken = NULL;
HWND g_hEditUID = NULL;
HWND g_hComboQualities = NULL;
HWND g_hComboUpdateInterval = NULL;

bool isInteractive = false;
int updateIntervalMs = 60000; // Par défaut 60s

std::wstring overlayText = L"Chargement...";

// Valeurs par défaut
std::string g_token = "c54ea814-43ef-11f0-bf0c-020112c3dc7f";
std::string g_uid = "platinumgolem";
std::string g_quality = "normal";

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

std::string WstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
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
    std::string url = "https://union-titans.fr/api/items/detail/" + g_uid + "/json";

    std::string json = MakeAPIRequestCurlpp(url, g_token);
    if (json.empty()) {
        OutputDebugString(L"Erreur: réponse API vide\n");
        overlayText = L"Erreur: réponse API vide";
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
        overlayText = L"Erreur parsing JSON";
        return;
    }

    if (root.isMember("error")) {
        std::string errorMsg = root["error"].asString();
        std::string err = "API error: " + errorMsg;
        OutputDebugStringA(err.c_str());
        LogToFile(err);
        overlayText = Utf8ToWstring("Erreur API: " + errorMsg);
        return;
    }

    if (!root.isMember("data")) {
        OutputDebugString(L"Erreur: structure JSON inattendue (pas de 'data')\n");
        LogToFile("Erreur: structure JSON inattendue (pas de 'data')");
        overlayText = L"Erreur: JSON inattendu (pas de data)";
        return;
    }

    const Json::Value& data = root["data"];

    if (!data.isMember("label") || !data["label"].isString()) {
        OutputDebugString(L"Erreur: structure JSON inattendue (label manquant)\n");
        LogToFile("Erreur: structure JSON inattendue (label manquant)");
        overlayText = L"Erreur: JSON inattendu (label manquant)";
        return;
    }

    std::string labelUtf8 = data["label"].asString();
    std::wstring label = Utf8ToWstring(labelUtf8);

    // Recherche des données du marché pour la qualité choisie
    const Json::Value* marketEntry = NULL;
    if (data.isMember("last_market_data") && data["last_market_data"].isArray()) {
        const Json::Value& marketArray = data["last_market_data"];
        for (Json::ArrayIndex i = 0; i < marketArray.size(); ++i) {
            const Json::Value& entry = marketArray[i];
            if (entry.isMember("quality") && entry["quality"].isString()) {
                std::string q = entry["quality"].asString();
                if (q == g_quality) {
                    marketEntry = &entry;
                    break;
                }
            }
        }
    }

    if (!marketEntry) {
        overlayText = label + L"\nPas de données marché pour qualite '" + Utf8ToWstring(g_quality) + L"'.";
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
            ? L"Bénéfice : +" + formatNombre(diff) + L" gold"
            : L"a perte";
    }
    else {
        resultat = L"Données insuffisantes";
    }

    overlayText = L"Objet : " + label + L"\n"
        + L"Qualite : " + Utf8ToWstring(g_quality) + L"\n"
        + L"Prix offre (" + Utf8ToWstring(g_quality) + L") : " + offreStr + L"\n"
        + L"Prix demande (" + Utf8ToWstring(g_quality) + L") : " + demandeStr + L"\n"
        + resultat;
}

// Chargement / sauvegarde paramètres
struct Settings {
    std::string token;
    std::string uid;
    std::string quality;
    int updateIntervalMs;
    bool interactive;

    Settings() : token("c54ea814-43ef-11f0-bf0c-020112c3dc7f"), uid("platinumgolem"),
        quality("normal"), updateIntervalMs(60000), interactive(false) {
    }
};

Settings g_settings;

void LoadSettings() {
    std::ifstream file("config.ini");
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("token=") == 0) g_settings.token = line.substr(6);
        else if (line.find("uid=") == 0) g_settings.uid = line.substr(4);
        else if (line.find("quality=") == 0) g_settings.quality = line.substr(8);
        else if (line.find("updateIntervalMs=") == 0) g_settings.updateIntervalMs = atoi(line.substr(17).c_str());
        else if (line.find("interactive=") == 0) g_settings.interactive = (line.substr(12) == "1");
    }

    file.close();

    // Appliquer dans variables globales
    g_token = g_settings.token;
    g_uid = g_settings.uid;
    g_quality = g_settings.quality;
    updateIntervalMs = g_settings.updateIntervalMs;
    if (updateIntervalMs < UPDATE_INTERVAL_MIN_MS) updateIntervalMs = UPDATE_INTERVAL_MIN_MS;
    if (updateIntervalMs > UPDATE_INTERVAL_MAX_MS) updateIntervalMs = UPDATE_INTERVAL_MAX_MS;
    isInteractive = g_settings.interactive;
}

void SaveSettings() {
    std::ofstream file("config.ini");
    if (!file.is_open()) return;
    file << "token=" << g_token << "\n";
    file << "uid=" << g_uid << "\n";
    file << "quality=" << g_quality << "\n";
    file << "updateIntervalMs=" << updateIntervalMs << "\n";
    file << "interactive=" << (isInteractive ? "1" : "0") << "\n";
    file.close();
}

// UI helpers
HWND CreateLabel(HWND parent, int x, int y, int width, int height, const wchar_t* text) {
    return CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, width, height, parent, NULL, GetModuleHandle(NULL), NULL);
}

HWND CreateEdit(HWND parent, int x, int y, int width, int height, const wchar_t* text, bool readOnly = false) {
    HWND hEdit = CreateWindowW(L"EDIT", text, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        x, y, width, height, parent, NULL, GetModuleHandle(NULL), NULL);
    if (readOnly) {
        EnableWindow(hEdit, FALSE);
    }
    return hEdit;
}

HWND CreateComboBox(HWND parent, int x, int y, int width, int height) {
    return CreateWindowW(WC_COMBOBOX, NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | CBS_DROPDOWNLIST,
        x, y, width, height, parent, NULL, GetModuleHandle(NULL), NULL);
}

// Gère le message WM_COMMAND pour les contrôles de la fenêtre options
void OnCommandOptions(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    int id = LOWORD(wParam);

    if ((HWND)lParam == g_hComboQualities && HIWORD(wParam) == CBN_SELCHANGE) {
        int sel = (int)SendMessage(g_hComboQualities, CB_GETCURSEL, 0, 0);
        if (sel >= 0) {
            wchar_t buf[256];
            SendMessage(g_hComboQualities, CB_GETLBTEXT, sel, (LPARAM)buf);
            g_quality = WstringToUtf8(buf);
        }
    }
    else if ((HWND)lParam == g_hComboUpdateInterval && HIWORD(wParam) == CBN_SELCHANGE) {
        int sel = (int)SendMessage(g_hComboUpdateInterval, CB_GETCURSEL, 0, 0);
        if (sel >= 0) {
            // Les intervalles (en secondes) en combo
            int intervals[] = { 20, 30, 40, 50, 60 };
            updateIntervalMs = intervals[sel] * 1000;
        }
    }
    else if ((HWND)lParam == g_hCheckboxMove && HIWORD(wParam) == BN_CLICKED) {
        isInteractive = (SendMessage(g_hCheckboxMove, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    else if ((HWND)lParam == g_hEditToken && HIWORD(wParam) == EN_CHANGE) {
        wchar_t buf[256];
        GetWindowTextW(g_hEditToken, buf, 256);
        g_token = WstringToUtf8(buf);
    }
    else if ((HWND)lParam == g_hEditUID && HIWORD(wParam) == EN_CHANGE) {
        wchar_t buf[256];
        GetWindowTextW(g_hEditUID, buf, 256);
        g_uid = WstringToUtf8(buf);
    }
}

// Fenêtre options
LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
    {
        CreateLabel(hwnd, 10, 10, 300, 20, L"Token d'acces API");
        g_hEditToken = CreateEdit(hwnd, 10, 30, 350, 25, Utf8ToWstring(g_token).c_str());

        CreateLabel(hwnd, 10, 65, 100, 20, L"UID Objet");
        g_hEditUID = CreateEdit(hwnd, 10, 85, 350, 25, Utf8ToWstring(g_uid).c_str());

        CreateLabel(hwnd, 10, 120, 100, 20, L"Qualite");
        g_hComboQualities = CreateComboBox(hwnd, 10, 140, 150, 100);

        // Ajout des qualités dans combo
        const char* qualities[] = { "normal", "uncommon", "rare", "epic", "legendary" };
        for (int i = 0; i < 5; i++) {
            std::wstring wq = Utf8ToWstring(qualities[i]);
            SendMessage(g_hComboQualities, CB_ADDSTRING, 0, (LPARAM)wq.c_str());
            if (g_quality == qualities[i]) {
                SendMessage(g_hComboQualities, CB_SETCURSEL, i, 0);
            }
        }

        CreateLabel(hwnd, 200, 120, 140, 20, L"Intervalle update (sec)");
        g_hComboUpdateInterval = CreateComboBox(hwnd, 200, 140, 160, 100);

        // Ajout des intervalles possibles
        int intervals[] = { 20, 30, 40, 50, 60 };
        int selectedIndex = 0;
        for (int i = 0; i < 5; ++i) {
            std::wstring ws = std::to_wstring(intervals[i]);
            SendMessage(g_hComboUpdateInterval, CB_ADDSTRING, 0, (LPARAM)ws.c_str());
            if (updateIntervalMs == intervals[i] * 1000) selectedIndex = i;
        }
        SendMessage(g_hComboUpdateInterval, CB_SETCURSEL, selectedIndex, 0);

        g_hCheckboxMove = CreateWindowW(L"BUTTON", L"Interactive (deplacement fenetre)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, 180, 300, 25, hwnd, NULL, GetModuleHandle(NULL), NULL);

        if (isInteractive) SendMessage(g_hCheckboxMove, BM_SETCHECK, BST_CHECKED, 0);


    }
    case WM_COMMAND:
        OnCommandOptions(hwnd, wParam, lParam);
        break;

    case WM_CLOSE:
        SaveSettings();
        DestroyWindow(hwnd);
        g_hOptionsWnd = NULL;
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Fenêtre overlay (semi-transparente)
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static POINT dragStart = { 0,0 };
    static RECT wndRect;

    switch (msg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Fond transparent
        SetBkMode(hdc, TRANSPARENT);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Fond semi-transparent noir
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        // Texte blanc
        SetTextColor(hdc, RGB(255, 255, 255));

        DrawTextW(hdc, overlayText.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK);


        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        // Commencer le déplacement de la fenêtre proprement
        if (isInteractive)
        {
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        break;

    case WM_LBUTTONUP:
        if (isInteractive) {
            ReleaseCapture();
        }
        break;

    case WM_RBUTTONDOWN:
        // Ouvre la fenêtre options à droite de l'overlay
        if (!g_hOptionsWnd) {
            RECT r;
            GetWindowRect(hwnd, &r);
            int x = r.right + 10;
            int y = r.top;
            g_hOptionsWnd = CreateWindowW(L"OptionsWndClass", L"Options Overlay",
                WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
                x, y, 400, 280, NULL, NULL, GetModuleHandle(NULL), NULL);
            if (!g_hOptionsWnd) {
                MessageBoxW(NULL, L"Erreur création fenêtre options", L"Erreur", MB_ICONERROR);
            }
            ShowWindow(g_hOptionsWnd, SW_SHOW);
            UpdateWindow(g_hOptionsWnd);
        }
        break;

    case WM_TIMER:
        if (wParam == IDT_TIMER1) {
            UpdateOverlayData();
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Init common controls (pour combo box)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    LoadSettings();

    // Window classes
    WNDCLASSW wcOverlay = { 0 };
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.lpszClassName = L"OverlayWndClass";
    wcOverlay.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wcOverlay.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wcOverlay);

    WNDCLASSW wcOptions = { 0 };
    wcOptions.lpfnWndProc = OptionsWndProc;
    wcOptions.hInstance = hInstance;
    wcOptions.lpszClassName = L"OptionsWndClass";
    wcOptions.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcOptions.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wcOptions);

    // Création fenêtre overlay (transparent)
    g_hOverlayWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_APPWINDOW,
        L"OverlayWndClass",
        L"Overlay",
        WS_POPUP,
        100, 100, 400, 150,
        NULL, NULL, hInstance, NULL);

    if (!g_hOverlayWnd) {
        MessageBoxW(NULL, L"Erreur création fenêtre overlay", L"Erreur", MB_ICONERROR);
        return -1;
    }

    // Rendre la fenêtre transparente (permet clics sous-jacents)
    SetLayeredWindowAttributes(g_hOverlayWnd, 0, 200, LWA_ALPHA);

    ShowWindow(g_hOverlayWnd, nCmdShow);
    UpdateWindow(g_hOverlayWnd);

    // Timer pour update
    SetTimer(g_hOverlayWnd, IDT_TIMER1, updateIntervalMs, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        // Met à jour le timer si l'intervalle a changé (modifié dans options)
        static int lastInterval = updateIntervalMs;
        if (lastInterval != updateIntervalMs) {
            KillTimer(g_hOverlayWnd, IDT_TIMER1);
            SetTimer(g_hOverlayWnd, IDT_TIMER1, updateIntervalMs, NULL);
            lastInterval = updateIntervalMs;
        }
    }

    SaveSettings();
    return 0;
}
