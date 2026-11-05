// Meow Player - lightweight media player for Windows (Win32 + libmpv)
// Developed by Gr8Meow  |  MIT License
#define UNICODE
#define _UNICODE
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>
#include <winhttp.h>
#include <algorithm>
#include <string>
#include <vector>
#include <thread>
#include <cstdio>
#include <cstdlib>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")
#endif

// ---------------------------------------------------------------- config
#define APP_NAME     L"Meow Player"
#define APP_VERSION  "1.0.0"                 // bump this for every release
#define GH_REPO      L"Gr8Meow/MeowPlayer"   // <-- change to your GitHub user/repo
#define CREDIT       L"Developed by Gr8Meow"

// ---------------------------------------------------------------- libmpv (loaded at runtime, no headers needed)
struct mpv_handle;
typedef mpv_handle* (*PFN_create)(void);
typedef int  (*PFN_init)(mpv_handle*);
typedef int  (*PFN_setopt)(mpv_handle*, const char*, const char*);
typedef int  (*PFN_setprop)(mpv_handle*, const char*, const char*);
typedef int  (*PFN_command)(mpv_handle*, const char**);
typedef int  (*PFN_getprop)(mpv_handle*, const char*, int, void*);
typedef void (*PFN_destroy)(mpv_handle*);
enum { MPV_FORMAT_FLAG = 3, MPV_FORMAT_DOUBLE = 5 };

static PFN_create  p_create;
static PFN_init    p_init;
static PFN_setopt  p_setopt;
static PFN_setprop p_setprop;
static PFN_command p_command;
static PFN_getprop p_getprop;
static PFN_destroy p_destroy;
static mpv_handle* g_mpv = nullptr;

static bool LoadMpv() {
    HMODULE m = LoadLibraryW(L"libmpv-2.dll");
    if (!m) m = LoadLibraryW(L"mpv-2.dll");
    if (!m) m = LoadLibraryW(L"libmpv.dll");
    if (!m) return false;
    p_create  = (PFN_create) GetProcAddress(m, "mpv_create");
    p_init    = (PFN_init)   GetProcAddress(m, "mpv_initialize");
    p_setopt  = (PFN_setopt) GetProcAddress(m, "mpv_set_option_string");
    p_setprop = (PFN_setprop)GetProcAddress(m, "mpv_set_property_string");
    p_command = (PFN_command)GetProcAddress(m, "mpv_command");
    p_getprop = (PFN_getprop)GetProcAddress(m, "mpv_get_property");
    p_destroy = (PFN_destroy)GetProcAddress(m, "mpv_terminate_destroy");
    return p_create && p_init && p_setopt && p_setprop && p_command && p_getprop && p_destroy;
}

static std::string U8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    s.resize(n - 1);
    return s;
}

static void Cmd(const char* a, const char* b = nullptr, const char* c = nullptr, const char* d = nullptr) {
    if (!g_mpv) return;
    const char* args[5] = { a, b, c, d, nullptr };
    p_command(g_mpv, args);
}

// ---------------------------------------------------------------- state
static HWND g_hwnd, g_video;
static bool g_loaded = false, g_paused = true, g_fs = false;
static double g_pos = 0, g_dur = 0, g_vol = 100;
static bool g_updateAvail = false;
static std::wstring g_updateTag, g_title = APP_NAME;

static const int BAR_H = 80, TOP_H = 28;
static RECT rcSeek, rcOpen, rcPlay, rcStop, rcSub, rcVol, rcFull, rcBanner;
static bool g_dragSeek = false, g_dragVol = false;

static const COLORREF C_BG    = RGB(11, 16, 38);
static const COLORREF C_PANEL = RGB(22, 28, 60);
static const COLORREF C_TEXT  = RGB(230, 232, 255);
static const COLORREF C_DIM   = RGB(140, 148, 190);
static const COLORREF C_PINK  = RGB(255, 79, 163);
static const COLORREF C_BLUE  = RGB(80, 140, 255);

static int BarH() { return g_fs ? 0 : BAR_H; }
static int TopH() { return (g_updateAvail && !g_fs) ? TOP_H : 0; }

static void DoLayout() {
    RECT rc; GetClientRect(g_hwnd, &rc);
    int w = rc.right, h = rc.bottom, bt = h - BarH();
    MoveWindow(g_video, 0, TopH(), w, std::max(0, bt - TopH()), TRUE);
    rcBanner = { 0, 0, w, TopH() };
    rcSeek = { 16, bt + 12, w - 16, bt + 22 };
    int y = bt + 32, bh = 28;
    rcOpen = { 16, y, 80, y + bh };
    rcPlay = { 88, y, 152, y + bh };
    rcStop = { 160, y, 224, y + bh };
    rcSub  = { 232, y, 296, y + bh };
    rcFull = { w - 80, y, w - 16, y + bh };
    rcVol  = { w - 240, y + 10, w - 100, y + 18 };
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------- actions
static void SetTitleFor(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    g_title = (p == std::wstring::npos) ? path : path.substr(p + 1);
    SetWindowTextW(g_hwnd, (g_title + L" - " + APP_NAME).c_str());
}

static void OpenPath(const std::wstring& path) {
    if (!g_mpv) return;
    std::string u = U8(path);
    Cmd("loadfile", u.c_str(), "replace");
    p_setprop(g_mpv, "pause", "no");
    g_loaded = true; g_paused = false;
    SetTitleFor(path);
}

static void DoOpen() {
    wchar_t buf[MAX_PATH * 4] = {};
    OPENFILENAMEW o = { sizeof(o) };
    o.hwndOwner = g_hwnd; o.lpstrFile = buf; o.nMaxFile = MAX_PATH * 4;
    o.lpstrFilter = L"Media files\0*.*\0All files\0*.*\0";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&o)) OpenPath(buf);
}

static void DoSubs() {
    wchar_t buf[MAX_PATH * 4] = {};
    OPENFILENAMEW o = { sizeof(o) };
    o.hwndOwner = g_hwnd; o.lpstrFile = buf; o.nMaxFile = MAX_PATH * 4;
    o.lpstrFilter = L"Subtitles\0*.srt;*.ass;*.ssa;*.sub;*.vtt;*.txt\0All files\0*.*\0";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (GetOpenFileNameW(&o)) { std::string u = U8(buf); Cmd("sub-add", u.c_str(), "select"); }
}

static void TogglePause() {
    if (!g_loaded) return;
    g_paused = !g_paused;
    p_setprop(g_mpv, "pause", g_paused ? "yes" : "no");
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void DoStop() {
    Cmd("stop"); g_loaded = false; g_paused = true; g_pos = g_dur = 0;
    SetWindowTextW(g_hwnd, APP_NAME); InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void SeekTo(double sec) {
    if (!g_loaded || g_dur <= 0) return;
    sec = std::max(0.0, std::min(sec, g_dur));
    char b[64]; snprintf(b, sizeof b, "%.3f", sec);
    Cmd("seek", b, "absolute");
    g_pos = sec;
}

static void SeekRel(double d) {
    if (!g_loaded) return;
    char b[64]; snprintf(b, sizeof b, "%.1f", d);
    Cmd("seek", b, "relative");
}

static void SetVol(double v) {
    g_vol = std::max(0.0, std::min(100.0, v));
    char b[32]; snprintf(b, sizeof b, "%d", (int)g_vol);
    if (g_mpv) p_setprop(g_mpv, "volume", b);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

static void ToggleFS() {
    static WINDOWPLACEMENT wp = { sizeof(wp) };
    if (!g_fs) {
        GetWindowPlacement(g_hwnd, &wp);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN);
        SetWindowPlacement(g_hwnd, &wp);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    g_fs = !g_fs;
    DoLayout();
}

// ---------------------------------------------------------------- update checker (GitHub releases)
static std::vector<int> ParseVer(const std::string& s) {
    std::vector<int> v; int cur = 0; bool in = false;
    for (char c : s) {
        if (c >= '0' && c <= '9') { cur = cur * 10 + (c - '0'); in = true; }
        else { if (in) { v.push_back(cur); cur = 0; in = false; } if (c == '-' ) break; }
    }
    if (in) v.push_back(cur);
    return v;
}

static bool IsNewer(const std::string& remote, const std::string& local) {
    auto a = ParseVer(remote), b = ParseVer(local);
    size_t n = std::max(a.size(), b.size());
    a.resize(n, 0); b.resize(n, 0);
    for (size_t i = 0; i < n; i++) { if (a[i] != b[i]) return a[i] > b[i]; }
    return false;
}

static void UpdateCheckThread() {
    std::string body;
    HINTERNET s = WinHttpOpen(L"MeowPlayer-UpdateCheck", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!s) return;
    HINTERNET c = WinHttpConnect(s, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", (std::wstring(L"/repos/") + GH_REPO + L"/releases/latest").c_str(),
                                         nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (r && WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(r, nullptr)) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(r, &avail) && avail > 0 && body.size() < 200000) {
            std::string chunk(avail, 0); DWORD got = 0;
            if (!WinHttpReadData(r, &chunk[0], avail, &got) || !got) break;
            body.append(chunk, 0, got);
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);

    size_t k = body.find("\"tag_name\"");
    if (k == std::string::npos) return;
    size_t q1 = body.find('"', body.find(':', k) + 1);
    size_t q2 = body.find('"', q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos) return;
    std::string tag = body.substr(q1 + 1, q2 - q1 - 1);
    if (IsNewer(tag, APP_VERSION)) {
        auto* w = new std::wstring(tag.begin(), tag.end());
        PostMessageW(g_hwnd, WM_APP + 1, 0, (LPARAM)w);
    }
}

// ---------------------------------------------------------------- painting
static std::wstring FmtTime(double t) {
    int s = (int)t; wchar_t b[32];
    if (s >= 3600) swprintf(b, 32, L"%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    else swprintf(b, 32, L"%d:%02d", s / 60, s % 60);
    return b;
}

static void FillSolid(HDC dc, const RECT& r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b);
}

static void FillGrad(HDC dc, const RECT& r, COLORREF a, COLORREF b) {
    if (r.right <= r.left) return;
    TRIVERTEX v[2] = {
        { r.left,  r.top,    (COLOR16)(GetRValue(a) << 8), (COLOR16)(GetGValue(a) << 8), (COLOR16)(GetBValue(a) << 8), 0 },
        { r.right, r.bottom, (COLOR16)(GetRValue(b) << 8), (COLOR16)(GetGValue(b) << 8), (COLOR16)(GetBValue(b) << 8), 0 } };
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(dc, v, 2, &gr, 1, GRADIENT_FILL_RECT_H);
}

static void Button(HDC dc, const RECT& r, const wchar_t* t) {
    RECT rr = r;
    HBRUSH br = CreateSolidBrush(C_PANEL); HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 70, 130));
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 10, 10);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(pen);
    SetTextColor(dc, C_TEXT);
    DrawTextW(dc, t, -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void Track(HDC dc, const RECT& r, double frac) {
    RECT bg = r; FillSolid(dc, bg, RGB(40, 48, 95));
    RECT f = r; f.right = r.left + (LONG)((r.right - r.left) * std::max(0.0, std::min(1.0, frac)));
    FillGrad(dc, f, C_PINK, C_BLUE);
    int cx = f.right, cy = (r.top + r.bottom) / 2;
    HBRUSH b = CreateSolidBrush(C_TEXT); HGDIOBJ ob = SelectObject(dc, b);
    Ellipse(dc, cx - 7, cy - 7, cx + 7, cy + 7);
    SelectObject(dc, ob); DeleteObject(b);
}

static void Paint(HWND h) {
    PAINTSTRUCT ps; HDC wdc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(dc, bmp);
    FillSolid(dc, rc, C_BG);
    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ of = SelectObject(dc, font);

    if (TopH() > 0) {
        FillGrad(dc, rcBanner, RGB(120, 40, 160), RGB(40, 80, 200));
        SetTextColor(dc, RGB(255, 255, 255));
        std::wstring t = L"Update available: " + g_updateTag + L"  -  click to download";
        RECT r = rcBanner; DrawTextW(dc, t.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (BarH() > 0) {
        RECT bar = { 0, rc.bottom - BAR_H, rc.right, rc.bottom };
        FillSolid(dc, bar, C_PANEL);
        Track(dc, rcSeek, g_dur > 0 ? g_pos / g_dur : 0);
        Button(dc, rcOpen, L"Open");
        Button(dc, rcPlay, (g_loaded && !g_paused) ? L"Pause" : L"Play");
        Button(dc, rcStop, L"Stop");
        Button(dc, rcSub,  L"Subs");
        Button(dc, rcFull, L"Full");
        Track(dc, rcVol, g_vol / 100.0);
        SetTextColor(dc, C_TEXT);
        std::wstring tm = FmtTime(g_pos) + L" / " + FmtTime(g_dur);
        RECT tr = { 310, rcPlay.top, rcVol.left - 20, rcPlay.bottom };
        DrawTextW(dc, tm.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, C_DIM);
        RECT cr = { 16, rc.bottom - 22, rc.right - 16, rc.bottom - 4 };
        DrawTextW(dc, CREDIT, -1, &cr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(dc, of); DeleteObject(font);
    BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc);
    EndPaint(h, &ps);
}

// ---------------------------------------------------------------- window procs
static bool In(const RECT& r, int x, int y) { POINT p = { x, y }; return PtInRect(&r, p); }

static void SeekFromX(int x) {
    double f = (double)(x - rcSeek.left) / (rcSeek.right - rcSeek.left);
    SeekTo(f * g_dur);
}
static void VolFromX(int x) {
    SetVol(100.0 * (x - rcVol.left) / (rcVol.right - rcVol.left));
}

static LRESULT CALLBACK VideoProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_LBUTTONDOWN: TogglePause(); return 0;
    case WM_LBUTTONDBLCLK: ToggleFS(); return 0;
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, (HBRUSH)GetStockObject(BLACK_BRUSH)); return 1; }
    }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE: if (g_video) DoLayout(); return 0;
    case WM_GETMINMAXINFO: { auto* mm = (MINMAXINFO*)l; mm->ptMinTrackSize = { 640, 360 }; return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: Paint(h); return 0;
    case WM_TIMER: {
        if (g_mpv && g_loaded) {
            double v = 0; int flag = 0;
            if (!g_dragSeek && p_getprop(g_mpv, "time-pos", MPV_FORMAT_DOUBLE, &v) >= 0) g_pos = v;
            if (p_getprop(g_mpv, "duration", MPV_FORMAT_DOUBLE, &v) >= 0) g_dur = v;
            if (p_getprop(g_mpv, "pause", MPV_FORMAT_FLAG, &flag) >= 0) g_paused = flag != 0;
        }
        if (!g_fs) { RECT r; GetClientRect(h, &r); r.top = r.bottom - BAR_H; InvalidateRect(h, &r, FALSE); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (In(rcBanner, x, y) && TopH() > 0) { ShellExecuteW(h, L"open", (std::wstring(L"https://github.com/") + GH_REPO + L"/releases/latest").c_str(), 0, 0, SW_SHOW); return 0; }
        RECT sk = rcSeek; InflateRect(&sk, 0, 8); RECT vk = rcVol; InflateRect(&vk, 0, 8);
        if (In(sk, x, y)) { g_dragSeek = true; SetCapture(h); SeekFromX(x); }
        else if (In(vk, x, y)) { g_dragVol = true; SetCapture(h); VolFromX(x); }
        else if (In(rcOpen, x, y)) DoOpen();
        else if (In(rcPlay, x, y)) TogglePause();
        else if (In(rcStop, x, y)) DoStop();
        else if (In(rcSub, x, y)) DoSubs();
        else if (In(rcFull, x, y)) ToggleFS();
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(l);
        if (g_dragSeek) { SeekFromX(x); InvalidateRect(h, nullptr, FALSE); }
        if (g_dragVol) VolFromX(x);
        return 0;
    }
    case WM_LBUTTONUP: g_dragSeek = g_dragVol = false; ReleaseCapture(); return 0;
    case WM_MOUSEWHEEL: SetVol(g_vol + (GET_WHEEL_DELTA_WPARAM(w) > 0 ? 5 : -5)); return 0;
    case WM_KEYDOWN:
        switch (w) {
        case VK_SPACE: TogglePause(); break;
        case VK_LEFT:  SeekRel(-5); break;
        case VK_RIGHT: SeekRel(5); break;
        case VK_UP:    SetVol(g_vol + 5); break;
        case VK_DOWN:  SetVol(g_vol - 5); break;
        case 'F': case VK_F11: ToggleFS(); break;
        case VK_ESCAPE: if (g_fs) ToggleFS(); break;
        case 'O': DoOpen(); break;
        case 'S': DoSubs(); break;
        }
        return 0;
    case WM_DROPFILES: {
        wchar_t path[MAX_PATH * 2];
        if (DragQueryFileW((HDROP)w, 0, path, MAX_PATH * 2)) OpenPath(path);
        DragFinish((HDROP)w);
        return 0;
    }
    case WM_APP + 1: {
        auto* t = (std::wstring*)l; g_updateTag = *t; delete t;
        g_updateAvail = true; DoLayout(); return 0;
    }
    case WM_DESTROY:
        KillTimer(h, 1);
        if (g_mpv) { p_destroy(g_mpv); g_mpv = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---------------------------------------------------------------- entry
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR cmdline, int show) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"MeowPlayerMain";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(1));
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);
    WNDCLASSW vc = {};
    vc.style = CS_DBLCLKS; vc.lpfnWndProc = VideoProc; vc.hInstance = hi; vc.lpszClassName = L"MeowPlayerVideo";
    vc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&vc);

    g_hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, L"MeowPlayerMain", APP_NAME, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1100, 680, nullptr, nullptr, hi, nullptr);
    g_video = CreateWindowExW(0, L"MeowPlayerVideo", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              0, 0, 100, 100, g_hwnd, nullptr, hi, nullptr);

    if (!LoadMpv()) {
        MessageBoxW(nullptr, L"libmpv-2.dll was not found next to MeowPlayer.exe.\nSee README.md for where to get it.",
                    APP_NAME, MB_ICONERROR);
        return 1;
    }
    g_mpv = p_create();
    char wid[32]; snprintf(wid, sizeof wid, "%lld", (long long)(intptr_t)g_video);
    p_setopt(g_mpv, "wid", wid);
    p_setopt(g_mpv, "hwdec", "auto-safe");          // GPU decoding when available
    p_setopt(g_mpv, "vo", "gpu");
    p_setopt(g_mpv, "osc", "no");
    p_setopt(g_mpv, "input-default-bindings", "no");
    p_setopt(g_mpv, "input-vo-keyboard", "no");
    p_setopt(g_mpv, "keep-open", "yes");
    p_setopt(g_mpv, "sub-auto", "fuzzy");           // auto-load matching subtitle files
    p_setopt(g_mpv, "cache", "yes");
    p_setopt(g_mpv, "force-window", "yes");
    if (p_init(g_mpv) < 0) { MessageBoxW(nullptr, L"Failed to start the mpv engine.", APP_NAME, MB_ICONERROR); return 1; }

    DoLayout();
    ShowWindow(g_hwnd, show); UpdateWindow(g_hwnd);
    SetTimer(g_hwnd, 1, 250, nullptr);
    std::thread(UpdateCheckThread).detach();

    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) OpenPath(argv[1]);
    if (argv) LocalFree(argv);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
