#include "Utils.h"
#include <fstream>
#include <windows.h>

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

std::wstring FormatNumber(int value) {
    std::wstring str = std::to_wstring(value);
    int pos = (int)str.size() - 3;
    while (pos > 0) {
        str.insert(pos, L" ");
        pos -= 3;
    }
    return str;
}
