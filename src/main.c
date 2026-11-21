/*
 * Brisabella - atenuador de pantalla ultraligero para Windows 7+ (x86 y x64)
 *
 * Como funciona: pone una ventana negra semitransparente que no recibe clics
 * ("click-through") encima de TODO el escritorio (todos los monitores).
 * Al estar al 0 % la ventana se oculta y el programa no hace nada.
 *
 * Sin dependencias, sin ventana principal, 0 % de CPU en reposo.
 * Codigo fuente en UTF-8.
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

#define APP_NAME   L"Brisabella"
#define APP_VER    L"1.0.1"
#define INI_SEC    L"Brisabella"
#define RUN_KEY    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

#define WM_TRAY    (WM_APP + 1)
#define ID_TRAY    1
#define IDM_LEVEL  1000          /* 1000 + indice del nivel */
#define IDM_AUTO   2000
#define IDM_CFG    2001
#define IDM_RELOAD 2002
#define IDM_EXIT   2003

/* Niveles de oscuridad (%). 0 = Windows normal, 100 = casi negro total. */
#define NLEVELS 12
static const int LEVELS[NLEVELS] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 96, 100 };
static const WCHAR *LABELS[NLEVELS] = {
    L"0  (normal)", L"10", L"20", L"30", L"40", L"50",
    L"60", L"70", L"80", L"90", L"96  (casi negro)", L"100  (negro)"
};

static HINSTANCE g_inst;
static HWND      g_ctl;                 /* ventana oculta: bandeja, atajos, mensajes */
static HWND      g_ovl;                 /* ventana negra que atenua la pantalla */
static HICON     g_icon;
static HWINEVENTHOOK g_hook;
static UINT      g_msgTaskbar;
static int       g_idx;                 /* indice actual dentro de LEVELS */
static BOOL      g_auto = TRUE;         /* iniciar con Windows */
static BOOL      g_menuOpen;
static WCHAR     g_cfg[MAX_PATH + 16];  /* ruta del config.ini */

/* ------------------------------------------------------------ utilidades */

static void Pct(WCHAR *buf, int v)      /* "50 %" sin usar el formato %% */
{
    wsprintfW(buf, L"%d", v);
    lstrcatW(buf, L" %");
}

static int NearestIdx(int pct)
{
    int best = 0;
    for (int i = 1; i < NLEVELS; i++)
        if (abs(pct - LEVELS[i]) < abs(pct - LEVELS[best])) best = i;
    return best;
}

static BYTE AlphaFor(int pct)
{
    int a = (pct * 255 + 50) / 100;
    if (a > 252) a = 252;               /* nunca 100 % opaco: siempre queda algo visible */
    return (BYTE)a;
}

/* --------------------------------------------------------- configuracion */

static void CfgInit(void)
{
    WCHAR dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);
    if (n == 0 || n + 12 >= MAX_PATH) {
        GetModuleFileNameW(NULL, dir, MAX_PATH);
        WCHAR *s = wcsrchr(dir, L'\\');
        if (s) *s = 0;
    } else {
        lstrcatW(dir, L"\\Brisabella");
        CreateDirectoryW(dir, NULL);
    }
    wsprintfW(g_cfg, L"%s\\config.ini", dir);

    if (GetFileAttributesW(g_cfg) == INVALID_FILE_ATTRIBUTES) {
        static const char txt[] =
            "; Brisabella - configuracion\r\n"
            "; Despues de editar: guarda el archivo y elige \"Recargar atajos\" en el menu del icono.\r\n"
            "[Brisabella]\r\n"
            "Level=0\r\n"
            "Autostart=1\r\n"
            "; Modificadores: Ctrl, Alt, Shift, Win (se pueden combinar: Ctrl+Alt)\r\n"
            "Modifiers=Ctrl\r\n"
            "; Teclas: un digito, una letra o F1..F24. Vacio = desactivado.\r\n"
            "KeyDarker=1\r\n"
            "KeyLighter=2\r\n"
            "KeyReset=0\r\n";
        HANDLE h = CreateFileW(g_cfg, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD w;
            WriteFile(h, txt, (DWORD)(sizeof txt - 1), &w, NULL);
            CloseHandle(h);
        }
    }
}

static void CfgSaveInt(const WCHAR *key, int v)
{
    WCHAR b[16];
    wsprintfW(b, L"%d", v);
    WritePrivateProfileStringW(INI_SEC, key, b, g_cfg);
}

static UINT ParseMods(const WCHAR *s)
{
    WCHAR b[64];
    UINT m = 0;
    lstrcpynW(b, s, 64);
    CharLowerBuffW(b, (DWORD)lstrlenW(b));
    if (wcsstr(b, L"ctrl") || wcsstr(b, L"control")) m |= MOD_CONTROL;
    if (wcsstr(b, L"alt"))   m |= MOD_ALT;
    if (wcsstr(b, L"shift")) m |= MOD_SHIFT;
    if (wcsstr(b, L"win"))   m |= MOD_WIN;
    return m;
}

static UINT ParseKey(const WCHAR *s)
{
    while (*s == L' ') s++;
    if (!*s) return 0;
    if ((s[0] == L'F' || s[0] == L'f') && s[1] >= L'1' && s[1] <= L'9') {
        int n = 0;
        for (const WCHAR *p = s + 1; *p >= L'0' && *p <= L'9'; p++) n = n * 10 + (*p - L'0');
        return (n >= 1 && n <= 24) ? (UINT)(VK_F1 + n - 1) : 0;
    }
    WCHAR c = s[0];
    if (c >= L'a' && c <= L'z') c = (WCHAR)(c - 32);
    return ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z')) ? (UINT)c : 0;
}

/* ---------------------------------------------------------- inicio con Windows */

static void SetAutostart(BOOL on)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS)
        return;
    if (on) {
        WCHAR exe[MAX_PATH], val[MAX_PATH + 4];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        wsprintfW(val, L"\"%s\"", exe);
        RegSetValueExW(k, APP_NAME, 0, REG_SZ, (const BYTE *)val, (DWORD)((lstrlenW(val) + 1) * sizeof(WCHAR)));
    } else {
        RegDeleteValueW(k, APP_NAME);
    }
    RegCloseKey(k);
}

/* ------------------------------------------------------------ bandeja */

static void TrayTip(NOTIFYICONDATAW *n)
{
    WCHAR p[16];
    Pct(p, LEVELS[g_idx]);
    lstrcpyW(n->szTip, APP_NAME);
    lstrcatW(n->szTip, L" - ");
    lstrcatW(n->szTip, p);
}

static void TrayAdd(void)
{
    NOTIFYICONDATAW n;
    ZeroMemory(&n, sizeof n);
    n.cbSize = sizeof n;
    n.hWnd = g_ctl;
    n.uID = ID_TRAY;
    n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    n.uCallbackMessage = WM_TRAY;
    n.hIcon = g_icon;
    TrayTip(&n);
    Shell_NotifyIconW(NIM_ADD, &n);
}

static void TrayUpdate(void)
{
    NOTIFYICONDATAW n;
    ZeroMemory(&n, sizeof n);
    n.cbSize = sizeof n;
    n.hWnd = g_ctl;
    n.uID = ID_TRAY;
    n.uFlags = NIF_TIP;
    TrayTip(&n);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

static void TrayRemove(void)
{
    NOTIFYICONDATAW n;
    ZeroMemory(&n, sizeof n);
    n.cbSize = sizeof n;
    n.hWnd = g_ctl;
    n.uID = ID_TRAY;
    Shell_NotifyIconW(NIM_DELETE, &n);
}

static void Balloon(const WCHAR *title, const WCHAR *text)
{
    NOTIFYICONDATAW n;
    ZeroMemory(&n, sizeof n);
    n.cbSize = sizeof n;
    n.hWnd = g_ctl;
    n.uID = ID_TRAY;
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = NIIF_WARNING;
    lstrcpynW(n.szInfoTitle, title, 64);
    lstrcpynW(n.szInfo, text, 256);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

/* ------------------------------------------------------------ overlay */

static void RaiseOverlay(void)
{
    WCHAR cls[80];
    HWND fg;
    if (g_menuOpen || LEVELS[g_idx] == 0) return;

    /* No tapar el menu de iconos ocultos de la barra (la flecha ^) */
    fg = GetForegroundWindow();
    if (fg && GetClassNameW(fg, cls, 80) &&
        (!lstrcmpW(cls, L"NotifyIconOverflowWindow") ||
         !lstrcmpW(cls, L"TopLevelWindowForOverflowXamlIsland")))
        return;

    SetWindowPos(g_ovl, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

#define MARGIN 8                        /* sobra unos pixeles por cada borde: sin franjas claras */

static void Reposition(void)           /* cubre el escritorio virtual completo */
{
    UINT f = SWP_NOACTIVATE | SWP_SHOWWINDOW;
    if (g_menuOpen) f |= SWP_NOZORDER;  /* no pasar por encima del menu abierto */
    SetWindowPos(g_ovl, HWND_TOPMOST,
                 GetSystemMetrics(SM_XVIRTUALSCREEN) - MARGIN, GetSystemMetrics(SM_YVIRTUALSCREEN) - MARGIN,
                 GetSystemMetrics(SM_CXVIRTUALSCREEN) + 2 * MARGIN, GetSystemMetrics(SM_CYVIRTUALSCREEN) + 2 * MARGIN, f);
}

/* Asegura medidas en pixeles reales (laptops con escala 125 %/150 %), aunque el manifiesto no se aplique. */
static void EnableDpiAwareness(void)
{
    typedef BOOL (WINAPI *SetCtxFn)(HANDLE);
    HMODULE u = GetModuleHandleW(L"user32.dll");
    SetCtxFn f = u ? (SetCtxFn)(void *)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if (f) f((HANDLE)(INT_PTR)-4);      /* PER_MONITOR_AWARE_V2; falla sin problema si ya estaba activo */
    else   SetProcessDPIAware();
}

static void CALLBACK OnForeground(HWINEVENTHOOK h, DWORD ev, HWND w, LONG o, LONG c, DWORD t, DWORD ms)
{
    (void)h; (void)ev; (void)w; (void)o; (void)c; (void)t; (void)ms;
    RaiseOverlay();
}

static void ApplyLevel(void)
{
    int pct = LEVELS[g_idx];
    if (pct <= 0) {
        ShowWindow(g_ovl, SW_HIDE);
        if (g_hook) { UnhookWinEvent(g_hook); g_hook = NULL; }
    } else {
        SetLayeredWindowAttributes(g_ovl, 0, AlphaFor(pct), LWA_ALPHA);
        Reposition();
        if (!g_hook)
            g_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL,
                                     OnForeground, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }
    TrayUpdate();
}

static void SetIdx(int i)
{
    if (i < 0) i = 0;
    if (i > NLEVELS - 1) i = NLEVELS - 1;
    if (i == g_idx) return;
    g_idx = i;
    ApplyLevel();
    CfgSaveInt(L"Level", LEVELS[g_idx]);
}

/* ------------------------------------------------------------ atajos */

static void HotkeysRegister(void)
{
    static const WCHAR *names[3] = { L"KeyDarker", L"KeyLighter", L"KeyReset" };
    static const WCHAR *defs[3]  = { L"1", L"2", L"0" };
    WCHAR b[64], bad[96], msg[256];
    UINT mods;
    bad[0] = 0;

    GetPrivateProfileStringW(INI_SEC, L"Modifiers", L"Ctrl", b, 64, g_cfg);
    mods = ParseMods(b);
    if (!mods) mods = MOD_CONTROL;      /* nunca registrar teclas sueltas */

    for (int i = 0; i < 3; i++) {
        UnregisterHotKey(g_ctl, i + 1);
        GetPrivateProfileStringW(INI_SEC, names[i], defs[i], b, 64, g_cfg);
        UINT vk = ParseKey(b);
        if (!vk) continue;
        if (!RegisterHotKey(g_ctl, i + 1, mods | MOD_NOREPEAT, vk)) {
            if (bad[0]) lstrcatW(bad, L", ");
            lstrcatW(bad, b);
        }
    }
    if (bad[0]) {
        lstrcpyW(msg, L"Otra aplicacion ya usa estas teclas: ");
        lstrcatW(msg, bad);
        lstrcatW(msg, L". Cambialas en config.ini (menu del icono > Editar atajos).");
        Balloon(APP_NAME, msg);
    }
}

/* ------------------------------------------------------------ menu */

static void ShowMenu(void)
{
    HMENU m = CreatePopupMenu();
    POINT pt;
    UINT cmd;
    WCHAR head[64];

    lstrcpyW(head, APP_NAME);
    lstrcatW(head, L"  v");
    lstrcatW(head, APP_VER);
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, head);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    for (int i = 0; i < NLEVELS; i++)
        AppendMenuW(m, MF_STRING, IDM_LEVEL + i, LABELS[i]);
    CheckMenuRadioItem(m, IDM_LEVEL, IDM_LEVEL + NLEVELS - 1, IDM_LEVEL + g_idx, MF_BYCOMMAND);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (g_auto ? MF_CHECKED : 0), IDM_AUTO, L"Iniciar con Windows");
    AppendMenuW(m, MF_STRING, IDM_CFG, L"Editar atajos...");
    AppendMenuW(m, MF_STRING, IDM_RELOAD, L"Recargar atajos");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, L"Salir");

    GetCursorPos(&pt);
    g_menuOpen = TRUE;                  /* mientras el menu esta abierto no se tapa con el overlay */
    SetForegroundWindow(g_ctl);
    cmd = (UINT)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                               pt.x, pt.y, 0, g_ctl, NULL);
    PostMessageW(g_ctl, WM_NULL, 0, 0);
    g_menuOpen = FALSE;
    DestroyMenu(m);

    if (cmd >= IDM_LEVEL && cmd < IDM_LEVEL + NLEVELS) {
        SetIdx((int)cmd - IDM_LEVEL);
    } else if (cmd == IDM_AUTO) {
        g_auto = !g_auto;
        SetAutostart(g_auto);
        CfgSaveInt(L"Autostart", g_auto);
    } else if (cmd == IDM_CFG) {
        WCHAR q[MAX_PATH + 24];
        wsprintfW(q, L"\"%s\"", g_cfg);
        ShellExecuteW(NULL, L"open", L"notepad.exe", q, NULL, SW_SHOWNORMAL);
    } else if (cmd == IDM_RELOAD) {
        HotkeysRegister();
    } else if (cmd == IDM_EXIT) {
        DestroyWindow(g_ctl);
        return;
    }
    RaiseOverlay();
}

/* ------------------------------------------------------------ ventana de control */

static LRESULT CALLBACK CtlProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == g_msgTaskbar && g_msgTaskbar) {      /* el Explorador se reinicio */
        TrayAdd();
        return 0;
    }
    switch (msg) {
    case WM_TRAY:
        if (lp == WM_LBUTTONUP || lp == WM_RBUTTONUP) ShowMenu();
        return 0;
    case WM_HOTKEY:
        if (wp == 1)      SetIdx(g_idx + 1);        /* Ctrl+1: mas oscuro */
        else if (wp == 2) SetIdx(g_idx - 1);        /* Ctrl+2: mas claro  */
        else if (wp == 3) SetIdx(0);                /* Ctrl+0: normal     */
        return 0;
    case WM_SETTINGCHANGE:                          /* cambio de escala / ajustes de pantalla */
    case WM_DISPLAYCHANGE:                          /* cambio de resolucion / monitores */
        if (LEVELS[g_idx] > 0) Reposition();
        return 0;
    case WM_DESTROY:
        if (g_hook) { UnhookWinEvent(g_hook); g_hook = NULL; }
        TrayRemove();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    WNDCLASSW wc;
    MSG m;
    HANDLE mtx;
    (void)hp; (void)cmd; (void)show;

    mtx = CreateMutexW(NULL, FALSE, L"Local\\Brisabella.SingleInstance");
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"Brisabella ya esta en ejecucion.\nBusca su icono junto al reloj (flecha ^).",
                    APP_NAME, MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    g_inst = hi;
    EnableDpiAwareness();
    g_msgTaskbar = RegisterWindowMessageW(L"TaskbarCreated");
    CfgInit();
    g_idx  = NearestIdx(GetPrivateProfileIntW(INI_SEC, L"Level", 0, g_cfg));
    g_auto = GetPrivateProfileIntW(INI_SEC, L"Autostart", 1, g_cfg) != 0;

    ZeroMemory(&wc, sizeof wc);                     /* overlay: solo pinta negro */
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = hi;
    wc.lpszClassName = L"BrisabellaOverlay";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    g_ovl = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                            L"BrisabellaOverlay", L"", WS_POPUP, 0, 0, 0, 0, NULL, NULL, hi, NULL);

    ZeroMemory(&wc, sizeof wc);                     /* ventana oculta de control */
    wc.lpfnWndProc   = CtlProc;
    wc.hInstance     = hi;
    wc.lpszClassName = L"BrisabellaCtl";
    RegisterClassW(&wc);
    g_ctl = CreateWindowExW(WS_EX_TOOLWINDOW, L"BrisabellaCtl", APP_NAME, WS_POPUP,
                            0, 0, 0, 0, NULL, NULL, hi, NULL);

    if (!g_ovl || !g_ctl) {
        MessageBoxW(NULL, L"No se pudo iniciar Brisabella.", APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_icon = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (!g_icon) g_icon = LoadIconW(NULL, IDI_APPLICATION);

    TrayAdd();
    HotkeysRegister();
    SetAutostart(g_auto);                           /* se corrige solo si moviste el .exe */
    ApplyLevel();                                   /* restaura el ultimo nivel usado */
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    DestroyWindow(g_ovl);
    if (mtx) CloseHandle(mtx);
    return (int)m.wParam;
}
