//=============================================================================
// 文件:    PlayerGUI.cpp
// 说明:    纯 Win32 图形界面的模拟播放器宿主程序（无 MFC 依赖）
//
//          插件机制：与 console 版 / MFC 版完全相同——「导出类」方式，
//          扫描 *.dll -> LoadLibrary -> GetProcAddress("CreatePlugin")
//          -> 得到基类指针 IPlugin* -> 虚函数多态调用。
//
//          界面：借鉴现代音乐播放软件的深色 UI 风格，纯 GDI 自绘实现：
//            * 无边框圆角窗口 + 自绘标题栏（含关闭按钮、可拖动）
//            * 圆角 owner-draw 按钮（主按钮强调色 + 悬停高亮）
//            * 深色播放列表 + 底部状态栏
//            * Segoe UI 现代字体
//=============================================================================

#include <windows.h>
#include <commdlg.h>     // GetOpenFileName
#include <cstdio>
#include <cstring>

#include "IPlugin.h"

// 控件 ID
#define IDC_BTN_LOAD   1001
#define IDC_BTN_OPEN   1002
#define IDC_BTN_PLAY   1003
#define IDC_BTN_PAUSE  1004
#define IDC_BTN_STOP   1005
#define IDC_LIST       1006

#define MAX_PLUGIN 32

// ---------------------------------------------------------------------------
// 配色（深色主题）
// ---------------------------------------------------------------------------
#define CLR_BG          RGB(18, 18, 18)      // 窗口背景
#define CLR_PANEL       RGB(30, 30, 30)      // 卡片背景
#define CLR_PANEL_HOVER RGB(45, 45, 45)      // 悬停/高亮
#define CLR_ACCENT      RGB(29, 185, 84)     // 强调色（播放绿）
#define CLR_ACCENT_HOV  RGB(46, 214, 102)    // 强调色悬停
#define CLR_TEXT        RGB(255, 255, 255)   // 主文字
#define CLR_TEXT_DIM    RGB(178, 178, 178)   // 次要文字
#define CLR_BORDER      RGB(48, 48, 48)      // 边框
#define CLR_CLOSE_HOV   RGB(232, 17, 35)     // 关闭按钮悬停（红）

// ---------------------------------------------------------------------------
// 布局常量（窗口固定 560 x 384）
// ---------------------------------------------------------------------------
#define WIN_W           560
#define WIN_H           384
#define TITLEBAR_H      44
#define STATUSBAR_H     40
#define PANEL_LEFT_X    16
#define PANEL_LEFT_W    184
#define PANEL_RIGHT_X   212
#define PANEL_RIGHT_W   332
#define PANEL_TOP       56
#define PANEL_BOTTOM    336

// 播放控制符号（BMP 内 Unicode 几何符号）
#define SYM_PLAY        L"\x25B6"   // ▶
#define SYM_PAUSE       L"\x23F8"   // ⏸
#define SYM_STOP        L"\x23F9"   // ⏹

// 插件登记表项：格式 -> 插件对象（基类指针）
struct PluginEntry
{
    HMODULE  hModule;
    char     format[16];
    IPlugin* pPlugin;
};

// 自绘按钮描述
struct UIButton
{
    HWND   hwnd;
    RECT   rc;
    bool   hover;
    bool   primary;      // 主按钮（强调色）
    bool   tall;         // 是否大按钮（播放键）
};

static PluginEntry g_plugins[MAX_PLUGIN];
static int         g_pluginCount = 0;
static IPlugin*    g_current     = NULL;
static HWND        g_hList       = NULL;
static HWND        g_hwnd        = NULL;
static char        g_ext[16]     = {0};
static wchar_t     g_status[256] = L"Ready - click Open File to pick a media file";

static UIButton    g_buttons[5];
static int         g_buttonCount = 0;

static HFONT g_fontTitle = NULL;   // 标题
static HFONT g_fontBtn   = NULL;   // 按钮
static HFONT g_fontText  = NULL;   // 列表 / 状态

static RECT   g_closeRect = {0};
static bool   g_closeHover = false;

// ---------------------------------------------------------------------------
// 提取扩展名（不含点、转小写）
// ---------------------------------------------------------------------------
static void GetExt(const char* fileName, char* ext, int extSize)
{
    ext[0] = '\0';
    if (fileName == NULL) return;
    const char* dot = strrchr(fileName, '.');
    if (dot == NULL) return;
    int i = 0;
    for (const char* p = dot + 1; *p && i + 1 < extSize; ++p, ++i)
    {
        char c = *p;
        ext[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    ext[i] = '\0';
}

// ---------------------------------------------------------------------------
// 扫描并加载当前目录下的播放器插件（导出类方式）
// ---------------------------------------------------------------------------
static void LoadAllPlugins(HWND hList)
{
    SendMessage(hList, LB_RESETCONTENT, 0, 0);

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile("*.dll", &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        HMODULE hModule = LoadLibrary(fd.cFileName);
        if (hModule == NULL) continue;

        // 导出类方式：只取工厂函数 CreatePlugin
        CREATE_PLUGIN_FUNC pfnCreate =
            (CREATE_PLUGIN_FUNC)GetProcAddress(hModule, "CreatePlugin");
        if (pfnCreate == NULL) { FreeLibrary(hModule); continue; }

        IPlugin* pPlugin = pfnCreate();
        if (pPlugin == NULL) { FreeLibrary(hModule); continue; }

        const char* fmt = pPlugin->GetSupportedFormat();   // 虚函数
        if (fmt == NULL || g_pluginCount >= MAX_PLUGIN)
        {
            delete pPlugin; FreeLibrary(hModule); continue;
        }

        g_plugins[g_pluginCount].hModule = hModule;
        strncpy(g_plugins[g_pluginCount].format, fmt, sizeof(g_plugins[0].format) - 1);
        g_plugins[g_pluginCount].format[sizeof(g_plugins[0].format) - 1] = '\0';
        g_plugins[g_pluginCount].pPlugin = pPlugin;
        ++g_pluginCount;

        wchar_t item[256];
        _snwprintf(item, 256, L"  %hs  -  %hs", fmt, fd.cFileName);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)item);
    } while (FindNextFile(hFind, &fd));

    FindClose(hFind);
}

// ---------------------------------------------------------------------------
// 释放所有插件
// ---------------------------------------------------------------------------
static void UnloadAllPlugins()
{
    for (int i = 0; i < g_pluginCount; ++i)
    {
        if (g_plugins[i].pPlugin != NULL)
        {
            delete g_plugins[i].pPlugin;   // 虚析构
            g_plugins[i].pPlugin = NULL;
        }
        if (g_plugins[i].hModule != NULL)
        {
            FreeLibrary(g_plugins[i].hModule);
            g_plugins[i].hModule = NULL;
        }
    }
    g_pluginCount = 0;
    g_current = NULL;
}

// ---------------------------------------------------------------------------
// 按扩展名查找插件，返回基类指针
// ---------------------------------------------------------------------------
static IPlugin* FindPlugin(const char* ext)
{
    for (int i = 0; i < g_pluginCount; ++i)
        if (strcmp(g_plugins[i].format, ext) == 0)
            return g_plugins[i].pPlugin;
    return NULL;
}

// ---------------------------------------------------------------------------
// 绘制圆角填充矩形
// ---------------------------------------------------------------------------
static void FillRoundRect(HDC hdc, RECT& rc, int radius, COLORREF color)
{
    HBRUSH br = CreateSolidBrush(color);
    HGDIOBJ old = SelectObject(hdc, br);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    SelectObject(hdc, old);
    DeleteObject(br);
}

// ---------------------------------------------------------------------------
// 绘制标题栏
// ---------------------------------------------------------------------------
static void DrawTitleBar(HDC hdc, int width)
{
    RECT rc = {0, 0, width, TITLEBAR_H};
    FillRoundRect(hdc, rc, 0, CLR_BG);

    // 标题
    HFONT oldFont = (HFONT)SelectObject(hdc, g_fontTitle);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT);
    RECT titleRc = {20, 0, width - 60, TITLEBAR_H};
    DrawTextW(hdc, L"\x266A  Music Player", -1, &titleRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // 关闭按钮
    COLORREF cb = g_closeHover ? CLR_CLOSE_HOV : CLR_PANEL;
    FillRoundRect(hdc, g_closeRect, 8, cb);

    // 画 ×
    HPEN pen = CreatePen(PS_SOLID, 2, CLR_TEXT);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    int cx = (g_closeRect.left + g_closeRect.right) / 2;
    int cy = (g_closeRect.top + g_closeRect.bottom) / 2;
    MoveToEx(hdc, cx - 5, cy - 5, NULL); LineTo(hdc, cx + 5, cy + 5);
    MoveToEx(hdc, cx + 5, cy - 5, NULL); LineTo(hdc, cx - 5, cy + 5);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

// ---------------------------------------------------------------------------
// 绘制状态栏
// ---------------------------------------------------------------------------
static void DrawStatusBar(HDC hdc, int width)
{
    RECT rc = {0, WIN_H - STATUSBAR_H, width, WIN_H};
    FillRoundRect(hdc, rc, 0, CLR_PANEL);

    HFONT oldFont = (HFONT)SelectObject(hdc, g_fontText);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT_DIM);

    // 分隔线
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, 0, WIN_H - STATUSBAR_H, NULL);
    LineTo(hdc, width, WIN_H - STATUSBAR_H);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    RECT trc = {20, WIN_H - STATUSBAR_H + 2, width - 40, WIN_H - 4};
    DrawTextW(hdc, g_status, -1, &trc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
}

// ---------------------------------------------------------------------------
// 绘制窗口背景（双缓冲）
// ---------------------------------------------------------------------------
static void PaintBackground(HDC hdc, int width, int height)
{
    // 整体背景
    RECT bg = {0, 0, width, height};
    FillRoundRect(hdc, bg, 0, CLR_BG);

    // 左侧控制面板卡片
    RECT left = {PANEL_LEFT_X, PANEL_TOP, PANEL_LEFT_X + PANEL_LEFT_W, PANEL_BOTTOM};
    FillRoundRect(hdc, left, 12, CLR_PANEL);

    // 右侧列表卡片
    RECT right = {PANEL_RIGHT_X, PANEL_TOP, PANEL_RIGHT_X + PANEL_RIGHT_W, PANEL_BOTTOM};
    FillRoundRect(hdc, right, 12, CLR_PANEL);

    // 面板标题
    HFONT oldFont = (HFONT)SelectObject(hdc, g_fontBtn);
    SetBkMode(hdc, TRANSPARENT);

    SetTextColor(hdc, CLR_TEXT_DIM);
    RECT lt = {PANEL_LEFT_X + 16, PANEL_TOP + 12, PANEL_LEFT_X + PANEL_LEFT_W, PANEL_TOP + 34};
    DrawTextW(hdc, L"CONTROLS", -1, &lt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT rt = {PANEL_RIGHT_X + 16, PANEL_TOP + 12, PANEL_RIGHT_X + PANEL_RIGHT_W, PANEL_TOP + 34};
    DrawTextW(hdc, L"NOW PLAYING", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

// ---------------------------------------------------------------------------
// 更新按钮悬停状态
// ---------------------------------------------------------------------------
static void UpdateHover(int x, int y)
{
    for (int i = 0; i < g_buttonCount; ++i)
    {
        bool inside = (x >= g_buttons[i].rc.left  && x < g_buttons[i].rc.right &&
                       y >= g_buttons[i].rc.top   && y < g_buttons[i].rc.bottom);
        if (inside != g_buttons[i].hover)
        {
            g_buttons[i].hover = inside;
            InvalidateRect(g_buttons[i].hwnd, NULL, TRUE);
        }
    }
}

// ---------------------------------------------------------------------------
// 窗口过程
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hwnd = hwnd;

        // 关闭按钮矩形（提前初始化，供 WM_NCHITTEST 使用）
        g_closeRect.left   = WIN_W - 40;
        g_closeRect.top    = 8;
        g_closeRect.right  = WIN_W - 16;
        g_closeRect.bottom = 32;

        // 创建字体
        g_fontTitle = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontBtn   = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontText  = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        // 创建 owner-draw 按钮（左侧控制面板内）
        int bx = PANEL_LEFT_X + 16;
        int bw = PANEL_LEFT_W - 32;

        g_buttonCount = 0;
        struct { int id; int y; int h; bool primary; bool tall; const wchar_t* text; }
        defs[5] = {
            { IDC_BTN_PLAY,  PANEL_TOP + 46, 44, true,  true,  SYM_PLAY  L"  Play"  },
            { IDC_BTN_PAUSE, PANEL_TOP + 98, 34, false, false, SYM_PAUSE L"  Pause" },
            { IDC_BTN_STOP,  PANEL_TOP + 140, 34, false, false, SYM_STOP  L"  Stop"  },
            { IDC_BTN_OPEN,  PANEL_TOP + 190, 34, false, false, L"Open File"      },
            { IDC_BTN_LOAD,  PANEL_TOP + 232, 34, false, false, L"Load Plugins"   },
        };

        for (int i = 0; i < 5; ++i)
        {
            g_buttons[i].hwnd = CreateWindowExW(0, L"BUTTON", defs[i].text,
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                bx, defs[i].y, bw, defs[i].h, hwnd,
                (HMENU)(INT_PTR)defs[i].id, NULL, NULL);
            g_buttons[i].primary = defs[i].primary;
            g_buttons[i].tall    = defs[i].tall;
            g_buttons[i].hover   = false;
            g_buttons[i].rc.left   = bx;
            g_buttons[i].rc.top    = defs[i].y;
            g_buttons[i].rc.right  = bx + bw;
            g_buttons[i].rc.bottom = defs[i].y + defs[i].h;
            ++g_buttonCount;
        }

        // 播放列表（右侧卡片内）
        g_hList = CreateWindowExW(0, L"LISTBOX", NULL,
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            PANEL_RIGHT_X + 16, PANEL_TOP + 40,
            PANEL_RIGHT_W - 32, PANEL_BOTTOM - PANEL_TOP - 56,
            hwnd, (HMENU)(INT_PTR)IDC_LIST, NULL, NULL);
        SendMessage(g_hList, WM_SETFONT, (WPARAM)g_fontText, TRUE);

        LoadAllPlugins(g_hList);
        return 0;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        int id = dis->CtlID;

        UIButton* btn = NULL;
        for (int i = 0; i < g_buttonCount; ++i)
            if (GetDlgCtrlID(g_buttons[i].hwnd) == id) { btn = &g_buttons[i]; break; }
        if (btn == NULL) return TRUE;

        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF bg;
        if (btn->primary)
            bg = pressed ? CLR_ACCENT : (btn->hover ? CLR_ACCENT_HOV : CLR_ACCENT);
        else
            bg = pressed ? CLR_PANEL_HOVER : (btn->hover ? CLR_PANEL_HOVER : CLR_PANEL);

        FillRoundRect(dis->hDC, dis->rcItem, 8, bg);

        wchar_t text[64];
        GetWindowTextW(dis->hwndItem, text, 64);
        HFONT oldFont = (HFONT)SelectObject(dis->hDC, g_fontBtn);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, btn->primary ? RGB(0, 0, 0) : CLR_TEXT);
        DrawTextW(dis->hDC, text, -1, &dis->rcItem,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, oldFont);
        return TRUE;
    }

    case WM_CTLCOLORLISTBOX:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_TEXT);
        SetBkColor(hdc, CLR_PANEL);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        switch (id)
        {
        case IDC_BTN_LOAD:
            LoadAllPlugins(g_hList);
            _snwprintf(g_status, 256, L"Loaded %d plugin(s)", g_pluginCount);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;

        case IDC_BTN_OPEN:
        {
            OPENFILENAMEW ofn;
            wchar_t file[MAX_PATH] = {0};
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFilter = L"Media Files (*.mp3;*.wma;*.flac)\0*.mp3;*.wma;*.flac\0All Files (*.*)\0*.*\0\0";
            ofn.lpstrFile   = file;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW(&ofn))
            {
                wchar_t* name = wcsrchr(file, L'\\');
                name = (name != NULL) ? (name + 1) : file;

                char nameA[MAX_PATH];
                WideCharToMultiByte(CP_ACP, 0, name, -1, nameA, MAX_PATH, NULL, NULL);
                GetExt(nameA, g_ext, sizeof(g_ext));

                SendMessageW(g_hList, LB_ADDSTRING, 0, (LPARAM)name);

                g_current = FindPlugin(g_ext);
                if (g_current != NULL)
                {
                    _snwprintf(g_status, 256, L"Playing: %s  (%hs plugin)", name, g_ext);
                    g_current->LoadFile(nameA);   // 虚函数 -> 多态
                }
                else
                {
                    _snwprintf(g_status, 256, L"Unsupported format: .%hs", g_ext);
                    MessageBoxW(hwnd, L"Unsupported format!", L"Music Player",
                                MB_OK | MB_ICONWARNING);
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case IDC_BTN_PLAY:
            if (g_current != NULL) g_current->Play();   // 虚函数 -> 多态
            else MessageBoxW(hwnd, L"Please open a file first.", L"Music Player", MB_OK);
            return 0;

        case IDC_BTN_PAUSE:
            if (g_current != NULL) g_current->Pause();
            else MessageBoxW(hwnd, L"Please open a file first.", L"Music Player", MB_OK);
            return 0;

        case IDC_BTN_STOP:
            if (g_current != NULL) g_current->Stop();
            else MessageBoxW(hwnd, L"Please open a file first.", L"Music Player", MB_OK);
            return 0;
        }
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // 双缓冲
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

        PaintBackground(mem, rc.right, rc.bottom);
        DrawTitleBar(mem, rc.right);
        DrawStatusBar(mem, rc.right);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        UpdateHover(x, y);

        bool ch = (x >= g_closeRect.left && x < g_closeRect.right &&
                   y >= g_closeRect.top  && y < g_closeRect.bottom);
        if (ch != g_closeHover)
        {
            g_closeHover = ch;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
    {
        for (int i = 0; i < g_buttonCount; ++i)
        {
            if (g_buttons[i].hover)
            {
                g_buttons[i].hover = false;
                InvalidateRect(g_buttons[i].hwnd, NULL, TRUE);
            }
        }
        if (g_closeHover)
        {
            g_closeHover = false;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        if (x >= g_closeRect.left && x < g_closeRect.right &&
            y >= g_closeRect.top  && y < g_closeRect.bottom)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_NCHITTEST:
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        // 关闭按钮区域交给客户区处理（走 WM_LBUTTONDOWN），其余标题栏可拖动
        if (pt.x >= g_closeRect.left && pt.x < g_closeRect.right &&
            pt.y >= g_closeRect.top  && pt.y < g_closeRect.bottom)
            return HTCLIENT;
        if (pt.y <= TITLEBAR_H)
            return HTCAPTION;
        return HTCLIENT;
    }

    case WM_DESTROY:
        UnloadAllPlugins();
        if (g_fontTitle) DeleteObject(g_fontTitle);
        if (g_fontBtn)   DeleteObject(g_fontBtn);
        if (g_fontText)  DeleteObject(g_fontText);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// WinMain：程序入口
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // 当前目录切换到 exe 所在目录，便于扫描插件 DLL
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* slash = strrchr(exePath, '\\');
    if (slash != NULL) *slash = '\0';
    SetCurrentDirectoryA(exePath);

    // 注册窗口类
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"PlayerPluginWinClass";
    if (!RegisterClassW(&wc)) return 1;

    // 创建无边框窗口
    HWND hwnd = CreateWindowExW(
        0, L"PlayerPluginWinClass",
        L"Music Player",
        WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN) - WIN_W) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - WIN_H) / 2,
        WIN_W, WIN_H,
        NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) return 1;

    // 圆角窗口
    HRGN rgn = CreateRoundRectRgn(0, 0, WIN_W + 1, WIN_H + 1, 14, 14);
    SetWindowRgn(hwnd, rgn, TRUE);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0))
    {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return (int)m.wParam;
}
