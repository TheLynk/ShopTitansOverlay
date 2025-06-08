#pragma once
#include <string>

void LogToFile(const std::string& message);
std::wstring Utf8ToWstring(const std::string& str);
std::wstring FormatNumber(int value);
