#include "OverlayWindow.h"
#include "APIClient.h"
#include "Utils.h"
#include "OptionsWindow.h"
#include <json/json.h>
#include <string>

static std::wstring overlayText = L"Chargement...";
static bool isDragging = false;
static POINT clickPos;
bool isInteractive = false;

void UpdateOverlayData() {
    std::string token = "c54ea814-43ef-11f0-bf0c-020112c3dc7f";
    std::string itemUid = "platinumgolem";
    std::string url = "https://union-titans.fr/api/items/detail/" + itemUid + "/json";

    std::string json = MakeAPIRequestCurlpp(url, token);
    if (json.empty()) return;

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errs;
    std::istringstream s(json);

    if (!Json::parseFromStream(builder, s, &root, &errs)) {
        LogToFile("Erreur parsing JSON : " + errs);
        return;
    }

    if (root.isMember("error")) {
        LogToFile("API error: " + root["error"].asString());
        return;
    }

    if (!root.isMember("data")) {
        LogToFile("Erreur: structure JSON inattendue (pas de 'data')");
        return;
    }

    const Json::Value& data = root["data"];

    if (!data.isMember("label") || !data["label"].isString()) {
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

    auto formatNombre = FormatNumber;

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

static void PassThroughClick(HWND hwnd, WPARAM wParam, LPARAM lParam) {
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

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TIMER:
        if (wParam == 1) { // IDT_TIMER1
            UpdateOverlayData();
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 255, 0));
        DrawText(hdc, overlayText.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_WORDBREAK);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
        if (!isInteractive) {
            PassThroughClick(hwnd, wParam, lParam);
            return 0;
        }
        isDragging = true;
        GetCursorPos(&clickPos);
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE:
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

    case WM_LBUTTONUP:
        if (!isInteractive) return 0;
        isDragging = false;
        ReleaseCapture();
        return 0;

    case WM_RBUTTONDOWN: {
        HMENU hMenu = CreatePopupMenu();
        AppendMenu(hMenu, MF_STRING, 1, L"Minimiser");
        AppendMenu(hMenu, MF_STRING, 2, L"Fermer");
        if (isInteractive)
            AppendMenu(hMenu, MF_STRING, 3, L"Repasser en mode transparent");
        else
            AppendMenu(hMenu, MF_STRING, 3, L"Activer le déplacement");
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, 4, L"Options...");

        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);

        if (cmd == 1) ShowWindow(hwnd, SW_MINIMIZE);
        else if (cmd == 2) PostQuitMessage(0);
        else if (cmd == 3) {
            isInteractive = !isInteractive;
            if (isInteractive) {
                SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
                SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else {
                SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOPMOST);
                SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
            }
        }
        else if (cmd == 4) {
            const wchar_t OPT_CLASS[] = L"OptionsWindowClass";

            static bool registered = false;
            if (!registered) {
                WNDCLASS wcOpt = {};
                wcOpt.lpfnWndProc = OptionsWndProc;
                wcOpt.hInstance = GetModuleHandle(NULL);
                wcOpt.lpszClassName = OPT_CLASS;
                RegisterClass(&wcOpt);
                registered = true;
            }

            HWND hOptWnd = CreateWindowEx(0, OPT_CLASS, L"Options",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
                hwnd, NULL, GetModuleHandle(NULL), NULL);

            if (!hOptWnd) {
                MessageBox(hwnd, L"Erreur création fenêtre options", L"Erreur", MB_ICONERROR);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
