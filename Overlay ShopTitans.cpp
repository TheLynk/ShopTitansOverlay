#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <list>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Easy.hpp>

#include <json/json.h> // JsonCpp

// Timer ID et intervalle mise à jour (1 min)
#define IDT_TIMER1 1
#define UPDATE_INTERVAL_MS 60000

// Fonction pour logger dans un fichier texte
void LogToFile(const std::string& message) {
    std::ofstream logFile("overlay_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
}

// UTF-8 -> std::wstring (Unicode)
std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Fonction d'appel API avec curlpp
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

// Texte affiché dans l'overlay
std::wstring overlayText = L"Chargement...";

POINT clickPos;
bool isDragging = false;
bool isInteractive = false;

// Mise à jour des données affichées dans l'overlay
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

    if (!root.isMember("data")) {
        LogToFile("Erreur: pas de champ 'data' dans JSON");
        return;
    }

    Json::Value data = root["data"];

    std::string nameUtf8 = data.get("label", "N/A").asString();
    std::wstring name = Utf8ToWstring(nameUtf8);

    int value = data.get("value", 0).asInt();
    int atk = data.get("atk", 0).asInt();
    int def = data.get("def", 0).asInt();
    int hp = data.get("hp", 0).asInt();

    std::wostringstream woss;
    woss << L"Objet : " << name << L"\n"
        << L"Valeur : " << value << L"\n"
        << L"ATK : " << atk << L"\n"
        << L"DEF : " << def << L"\n"
        << L"HP : " << hp;

    overlayText = woss.str();
}


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

    // Vider le fichier log au démarrage
    {
        std::ofstream logFile("overlay_log.txt", std::ios::trunc);
    }

    const wchar_t CLASS_NAME[] = L"OverlayWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        switch (uMsg) {
        case WM_TIMER:
            if (wParam == IDT_TIMER1) {
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
            if (!isInteractive) {
                return 0;
            }
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
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        };
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

    SetTimer(hwnd, IDT_TIMER1, UPDATE_INTERVAL_MS, NULL);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
