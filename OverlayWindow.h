#pragma once
#include <windows.h>

void UpdateOverlayData();
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern bool isInteractive;
