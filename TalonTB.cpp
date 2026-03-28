#include <windows.h>
#include <commctrl.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <string>

#pragma comment(lib, "comctl32.lib")

namespace Style {
    COLORREF BG_DARK = RGB(12, 12, 17), PANEL_BG = RGB(22, 22, 28), NEON_PURP = RGB(170, 90, 255), TEXT_WHITE = RGB(215, 215, 220);
    HBRUSH hbgMain = CreateSolidBrush(BG_DARK), hbgPanel = CreateSolidBrush(PANEL_BG);
    HPEN hPenNeon = CreatePen(PS_SOLID, 2, NEON_PURP);
}

namespace Config {
    std::atomic<bool> Running{ false }, Picking{ false }, DetectingKey{ false }, ShowCrosshair{ false };
    std::atomic<int> Tolerance{ 20 }, DelayMS{ 10 };
    std::atomic<DWORD> TriggerKey{ VK_SPACE };
    std::atomic<bool> IsMouseKey{ false };
    std::string KeyName = "SPACE";
    struct { BYTE r, g, b; } Target = { 255, 0, 255 }; // purpl by default
}

HWND hTolSlider, hDelaySlider, hColorPreview, hKeyBtn, hStatus, hTolVal, hDelayVal;

void SendTrigger() {
    if (Config::IsMouseKey) {
        DWORD down = (Config::TriggerKey == VK_LBUTTON) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN;
        DWORD up = (Config::TriggerKey == VK_LBUTTON) ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP;
        mouse_event(down, 0, 0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        mouse_event(up, 0, 0, 0, 0);
    } else {
        INPUT in = { INPUT_KEYBOARD, { (WORD)Config::TriggerKey } };
        SendInput(1, &in, sizeof(INPUT));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
    }
}

void BotLoop() {
    HDC hS = GetDC(NULL), hM = CreateCompatibleDC(hS);
    HBITMAP hB = CreateCompatibleBitmap(hS, 40, 40); SelectObject(hM, hB);
    while (true) {
        if (Config::Running && !Config::Picking && !Config::DetectingKey) {
            int x = (GetSystemMetrics(SM_CXSCREEN) - 40) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - 40) / 2;
            BitBlt(hM, 0, 0, 40, 40, hS, x, y, SRCCOPY);
            BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), 40, -40, 1, 32, BI_RGB };
            DWORD px[1600]; GetDIBits(hM, hB, 0, 40, px, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
            for (int i = 0; i < 1600; i++) {
                BYTE r = (px[i] >> 16) & 0xFF, g = (px[i] >> 8) & 0xFF, b = px[i] & 0xFF;
                if (abs(r - Config::Target.r) <= Config::Tolerance && abs(g - Config::Target.g) <= Config::Tolerance && abs(b - Config::Target.b) <= Config::Tolerance) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(Config::DelayMS));
                    SendTrigger();
                    std::this_thread::sleep_for(std::chrono::milliseconds(700)); break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (GetAsyncKeyState(VK_F9)) ExitProcess(0); // set f9 to panic shutoff
    }
}

LRESULT CALLBACK GlobalHook(int n, WPARAM w, LPARAM l) {
    if (n >= 0 && Config::DetectingKey) {
        if (w == WM_LBUTTONDOWN || w == WM_RBUTTONDOWN) {
            Config::TriggerKey = (w == WM_LBUTTONDOWN) ? VK_LBUTTON : VK_RBUTTON;
            Config::KeyName = (w == WM_LBUTTONDOWN) ? "MOUSE LEFT" : "MOUSE RIGHT";
            Config::IsMouseKey = true; Config::DetectingKey = false;
            SetWindowTextA(hKeyBtn, ("KEY: " + Config::KeyName).c_str());
            SetWindowTextA(hStatus, "STATUS: IDLE"); return 1;
        }
    }
    if (n >= 0 && w == WM_LBUTTONDOWN && Config::Picking) {
        MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)l; HDC hdc = GetDC(NULL);
        COLORREF c = GetPixel(hdc, m->pt.x, m->pt.y); ReleaseDC(NULL, hdc);
        Config::Target = { GetRValue(c), GetGValue(c), GetBValue(c) };
        Config::Picking = false; SetWindowTextA(hStatus, "STATUS: IDLE");
        InvalidateRect(GetParent(hColorPreview), NULL, TRUE); return 1;
    }
    return CallNextHookEx(NULL, n, w, l);
}

LRESULT CALLBACK WindowProc(HWND hw, UINT u, WPARAM w, LPARAM l) {
    switch (u) {
    case WM_CTLCOLORSTATIC: { SetTextColor((HDC)w, Style::TEXT_WHITE); SetBkColor((HDC)w, Style::PANEL_BG); return (LRESULT)Style::hbgPanel; }
    case WM_ERASEBKGND: {
        HDC h = (HDC)w; RECT r; GetClientRect(hw, &r); FillRect(h, &r, Style::hbgMain);
        SelectObject(h, Style::hPenNeon); SelectObject(h, Style::hbgPanel);
        RoundRect(h, 10, 10, 125, r.bottom - 10, 12, 12); RoundRect(h, 135, 45, r.right - 10, r.bottom - 10, 12, 12);
        SetBkMode(h, TRANSPARENT); SetTextColor(h, Style::NEON_PURP); TextOutA(h, 145, 15, "TALON PRO", 9);
        SetTextColor(h, Style::TEXT_WHITE); TextOutA(h, 20, 60, "Triggerbot", 10); return 1;
    }
    case WM_KEYDOWN: {
        if (Config::DetectingKey) {
            Config::TriggerKey = (DWORD)w; Config::IsMouseKey = false;
            char k[32]; GetKeyNameTextA((LONG)l, k, 32); Config::KeyName = k;
            Config::DetectingKey = false; SetWindowTextA(hKeyBtn, ("KEY: " + Config::KeyName).c_str());
            SetWindowTextA(hStatus, "STATUS: IDLE");
        }
        break;
    }
    case WM_CREATE: {
        CreateWindowA("button", "START", WS_VISIBLE | WS_CHILD | BS_FLAT, 150, 65, 80, 30, hw, (HMENU)1, NULL, NULL);
        CreateWindowA("button", "STOP", WS_VISIBLE | WS_CHILD | BS_FLAT, 235, 65, 80, 30, hw, (HMENU)2, NULL, NULL);
        hStatus = CreateWindowA("static", "STATUS: IDLE", WS_VISIBLE | WS_CHILD, 325, 72, 120, 20, hw, NULL, NULL, NULL);
        hKeyBtn = CreateWindowA("button", "KEY: SPACE", WS_VISIBLE | WS_CHILD | BS_FLAT, 150, 110, 150, 30, hw, (HMENU)4, NULL, NULL);

        CreateWindowA("static", "TOLERANCE:", WS_VISIBLE | WS_CHILD, 150, 155, 90, 20, hw, NULL, NULL, NULL);
        hTolSlider = CreateWindowA(TRACKBAR_CLASSA, "", WS_VISIBLE | WS_CHILD, 240, 150, 180, 30, hw, NULL, NULL, NULL);
        SendMessage(hTolSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100)); SendMessage(hTolSlider, TBM_SETPOS, TRUE, Config::Tolerance);
        hTolVal = CreateWindowA("static", "20", WS_VISIBLE | WS_CHILD, 430, 155, 30, 20, hw, NULL, NULL, NULL);

        CreateWindowA("static", "DELAY (MS):", WS_VISIBLE | WS_CHILD, 150, 195, 90, 20, hw, NULL, NULL, NULL);
        hDelaySlider = CreateWindowA(TRACKBAR_CLASSA, "", WS_VISIBLE | WS_CHILD, 240, 190, 180, 30, hw, NULL, NULL, NULL);
        SendMessage(hDelaySlider, TBM_SETRANGE, TRUE, MAKELONG(0, 500)); SendMessage(hDelaySlider, TBM_SETPOS, TRUE, Config::DelayMS);
        hDelayVal = CreateWindowA("static", "10ms", WS_VISIBLE | WS_CHILD, 430, 195, 50, 20, hw, NULL, NULL, NULL);

        CreateWindowA("button", "DROPPER", WS_VISIBLE | WS_CHILD | BS_FLAT, 150, 240, 80, 30, hw, (HMENU)3, NULL, NULL);
        hColorPreview = CreateWindowA("static", "", WS_VISIBLE | WS_CHILD | SS_OWNERDRAW | WS_BORDER, 235, 242, 40, 25, hw, NULL, NULL, NULL);
        return 0;
    }
    case WM_HSCROLL: {
        Config::Tolerance = (int)SendMessage(hTolSlider, TBM_GETPOS, 0, 0);
        Config::DelayMS = (int)SendMessage(hDelaySlider, TBM_GETPOS, 0, 0);
        SetWindowTextA(hTolVal, std::to_string(Config::Tolerance).c_str());
        SetWindowTextA(hDelayVal, (std::to_string(Config::DelayMS) + "ms").c_str());
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT p = (LPDRAWITEMSTRUCT)l; HBRUSH hB = CreateSolidBrush(RGB(Config::Target.r, Config::Target.g, Config::Target.b));
        FillRect(p->hDC, &p->rcItem, hB); DeleteObject(hB); return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(w) == 1) { Config::Running = true; SetWindowTextA(hStatus, "STATUS: ACTIVE"); }
        if (LOWORD(w) == 2) { Config::Running = false; SetWindowTextA(hStatus, "STATUS: IDLE"); }
        if (LOWORD(w) == 3) { Config::Picking = true; SetWindowTextA(hStatus, "PICKING..."); }
        if (LOWORD(w) == 4) { Config::DetectingKey = true; SetWindowTextA(hStatus, "PRESS KEY/CLICK"); SetFocus(hw); }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hw, u, w, l);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nS) {
    Beep(800, 150); std::thread(BotLoop).detach();
    HHOOK h = SetWindowsHookEx(WH_MOUSE_LL, GlobalHook, hI, 0);
    WNDCLASSA wc = { 0 }; wc.lpfnWndProc = WindowProc; wc.hInstance = hI; wc.lpszClassName = "TalonPro";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); RegisterClassA(&wc);
    HWND hw = CreateWindowExA(WS_EX_TOPMOST, "TalonPro", "Talon Pro", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 360, NULL, NULL, hI, NULL);
    ShowWindow(hw, nS); MSG m;
    while (GetMessage(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
    UnhookWindowsHookEx(h); return 0;
}