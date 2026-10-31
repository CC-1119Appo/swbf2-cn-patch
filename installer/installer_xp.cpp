
// SWBF2 汉化安装器 — 无 CRT 版本（兼容 Windows XP SP3 及以上）
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>

#pragma comment(linker, "/ENTRY:EntryPoint")
#pragma comment(linker, "/NODEFAULTLIB")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS,5.01")
#pragma comment(linker, "/GS-")

#define ID_PATH    101
#define ID_BROWSE  102
#define ID_R_AUTO  110
#define ID_R_RM    111
#define ID_R_VA    112
#define ID_S_OVER  120
#define ID_S_EMB   121
#define ID_INSTALL 130
#define ID_PROG    140
#define ID_STATUS  141

static HWND g_hPath, g_hStatus, g_hProg, g_hBtn, g_hMain;
static WCHAR g_self[MAX_PATH];
static HANDLE g_logFile = INVALID_HANDLE_VALUE;

struct Entry { char name[260]; unsigned __int64 off; unsigned __int64 size; };
static Entry* g_entries = 0;
static unsigned int g_count = 0;
static unsigned __int64 g_totalBytes = 0, g_doneBytes = 0;

/* ---------- 基础工具（不用 CRT） ---------- */
static void* mem_alloc(DWORD bytes) { return VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }
static void mem_free(void* p) { if (p) VirtualFree(p, 0, MEM_RELEASE); }

static void str_copyW(WCHAR* d, const WCHAR* s, int cch) {
    int i = 0;
    if (cch <= 0) return;
    while (s[i] && i < cch - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}
static void str_catW(WCHAR* d, const WCHAR* s, int cch) {
    int n = 0;
    while (d[n]) n++;
    int i = 0;
    while (s[i] && n + i < cch - 1) { d[n + i] = s[i]; i++; }
    d[n + i] = 0;
}
static int str_cmpiW(const WCHAR* a, const WCHAR* b) {
    while (*a && *b) {
        WCHAR x = *a, y = *b;
        if (x >= L'A' && x <= L'Z') x += 32;
        if (y >= L'A' && y <= L'Z') y += 32;
        if (x != y) return (int)x - (int)y;
        a++; b++;
    }
    return (int)*a - (int)*b;
}
static int str_ncmpW(const WCHAR* a, const WCHAR* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return 1;
        if (!a[i]) return 0;
    }
    return 0;
}
static int u32_to_str(unsigned int v, char* buf) {
    char tmp[16]; int n = 0;
    if (!v) { buf[0] = '0'; buf[1] = 0; return 1; }
    while (v) { tmp[n++] = (char)('0' + (int)(v % 10u)); v /= 10u; }
    for (int i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = 0;
    return n;
}

static void log_line(const char* s) {
    if (g_logFile == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(g_logFile, s, (DWORD)lstrlenA(s), &w, NULL);
    WriteFile(g_logFile, "\r\n", 2, &w, NULL);
}
static void log_lineW(const WCHAR* prefix, const WCHAR* s) {
    if (g_logFile == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    char buf[1024];
    int n = WideCharToMultiByte(CP_ACP, 0, prefix, -1, buf, 1000, NULL, NULL);
    if (n > 0) WriteFile(g_logFile, buf, (DWORD)(n - 1), &w, NULL);
    n = WideCharToMultiByte(CP_ACP, 0, s, -1, buf, 1000, NULL, NULL);
    if (n > 0) WriteFile(g_logFile, buf, (DWORD)(n - 1), &w, NULL);
    WriteFile(g_logFile, "\r\n", 2, &w, NULL);
}

/* ---------- 索引解析 ---------- */
static int load_index() {
    HANDLE h = CreateFileW(g_self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER sz; sz.QuadPart = 0;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 24) { CloseHandle(h); return 0; }
    unsigned char tail[24];
    LARGE_INTEGER pos; pos.QuadPart = sz.QuadPart - 16;
    DWORD got = 0;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    ReadFile(h, tail, 16, &got, NULL);
    if (got != 16) { CloseHandle(h); return 0; }
    unsigned __int64 idx = 0;
    for (int i = 7; i >= 0; i--) idx = (idx << 8) | tail[i];
    if (tail[8] != 'S' || tail[9] != 'W' || tail[10] != 'C' || tail[11] != 'N') { CloseHandle(h); return 0; }
    if (idx >= (unsigned __int64)sz.QuadPart) { CloseHandle(h); return 0; }
    pos.QuadPart = (LONGLONG)idx;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    unsigned int cnt = 0;
    ReadFile(h, &cnt, 4, &got, NULL);
    if (cnt == 0 || cnt > 5000) { CloseHandle(h); return 0; }
    g_entries = (Entry*)mem_alloc(sizeof(Entry) * cnt);
    if (!g_entries) { CloseHandle(h); return 0; }
    for (unsigned int i = 0; i < cnt; i++) {
        unsigned short nl = 0;
        ReadFile(h, &nl, 2, &got, NULL);
        if (nl >= 260) { CloseHandle(h); return 0; }
        ReadFile(h, g_entries[i].name, nl, &got, NULL);
        g_entries[i].name[nl] = 0;
        unsigned long long off = 0, size = 0;
        ReadFile(h, &off, 8, &got, NULL);
        ReadFile(h, &size, 8, &got, NULL);
        g_entries[i].off = off;
        g_entries[i].size = size;
    }
    g_count = cnt;
    CloseHandle(h);
    return 1;
}

static int has_prefix(const char* name, const char* p) {
    while (*p) { if (*name++ != *p++) return 0; }
    return 1;
}

static WCHAR g_tmpDir[2048];
static void make_dirs_w(const WCHAR* full) {
    WCHAR* tmp = g_tmpDir;
    str_copyW(tmp, full, 2048);
    int last = -1;
    for (int i = 0; tmp[i]; i++) if (tmp[i] == L'\\') last = i;
    if (last < 0) return;
    tmp[last] = 0;
    for (int i = 3; tmp[i]; i++) {
        if (tmp[i] == L'\\') { tmp[i] = 0; CreateDirectoryW(tmp, NULL); tmp[i] = L'\\'; }
    }
    CreateDirectoryW(tmp, NULL);
}

static int extract_entry(unsigned int i, const WCHAR* destDir, int silent) {
    const char* name = g_entries[i].name;
    const char* rel = name + 2;
    static WCHAR relW[1024];
    static WCHAR full[2048];
    MultiByteToWideChar(CP_UTF8, 0, rel, -1, relW, 1024);
    for (int k = 0; relW[k]; k++) if (relW[k] == L'/') relW[k] = L'\\';
    str_copyW(full, destDir, 2048);
    str_catW(full, L"\\", 2048);
    str_catW(full, relW, 2048);
    make_dirs_w(full);

    HANDLE src = CreateFileW(g_self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (src == INVALID_HANDLE_VALUE) return 0;
    HANDLE dst = CreateFileW(full, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dst == INVALID_HANDLE_VALUE) {
        CloseHandle(src);
        log_lineW(L"ERR dest: ", full);
        return 0;
    }
    LARGE_INTEGER pos; pos.QuadPart = (LONGLONG)g_entries[i].off;
    SetFilePointerEx(src, pos, NULL, FILE_BEGIN);
    static unsigned char buf[1 << 20];
    unsigned __int64 remain = g_entries[i].size;
    DWORD got = 0, wrote = 0;
    while (remain > 0) {
        DWORD want = remain > sizeof(buf) ? (DWORD)sizeof(buf) : (DWORD)remain;
        if (!ReadFile(src, buf, want, &got, NULL) || got == 0) break;
        if (!WriteFile(dst, buf, got, &wrote, NULL) || wrote != got) break;
        remain -= got;
        g_doneBytes += got;
        if (!silent && g_totalBytes) {
            unsigned int doneK = (unsigned int)(g_doneBytes >> 10);
            unsigned int totalK = (unsigned int)(g_totalBytes >> 10);
            int pct = totalK ? (int)((doneK * 100u) / totalK) : 0;
            SendMessageW(g_hProg, PBM_SETPOS, (WPARAM)pct, 0);
            MSG msg;
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
    }
    CloseHandle(src);
    CloseHandle(dst);
    if (remain) { log_lineW(L"ERR short read: ", full); return 0; }
    return 1;
}

static int file_exists_w(const WCHAR* p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int join_and_check(const WCHAR* dir, const WCHAR* rel) {
    static WCHAR p[2048];
    str_copyW(p, dir, 2048);
    str_catW(p, L"\\", 2048);
    str_catW(p, rel, 2048);
    return file_exists_w(p);
}

static int run_install(const WCHAR* dir, int isRemaster, int embed, int silent) {
    static WCHAR logp[2048];
    str_copyW(logp, dir, 2048);
    str_catW(logp, L"\\install_cn.log", 2048);
    g_logFile = CreateFileW(logp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    log_line(isRemaster ? "edition=remaster" : "edition=vanilla");

    g_totalBytes = 0; g_doneBytes = 0;
    const char* pre = isRemaster ? "R/" : "V/";
    for (unsigned int i = 0; i < g_count; i++) {
        const char* n = g_entries[i].name;
        if (has_prefix(n, pre) || has_prefix(n, "S/") || (embed && has_prefix(n, "M/")))
            g_totalBytes += g_entries[i].size;
    }
    int ok = 1, copied = 0;
    for (unsigned int i = 0; i < g_count && ok; i++) {
        const char* n = g_entries[i].name;
        if (!(has_prefix(n, pre) || has_prefix(n, "S/") || (embed && has_prefix(n, "M/")))) continue;
        if (!extract_entry(i, dir, silent)) { ok = 0; break; }
        copied++;
    }
    char num[24]; u32_to_str((unsigned int)copied, num);
    log_line(ok ? "done, files copied:" : "FAILED, files copied:");
    log_line(num);
    if (g_logFile != INVALID_HANDLE_VALUE) { CloseHandle(g_logFile); g_logFile = INVALID_HANDLE_VALUE; }
    return ok ? copied : -1;
}

static void do_install_gui(HWND hwnd) {
    static WCHAR dir[2048];
    dir[0] = 0;
    GetWindowTextW(g_hPath, dir, 2048);
    if (!dir[0]) { MessageBoxW(hwnd, L"请先选择游戏目录（GameData 文件夹）。", L"提示", MB_ICONWARNING); return; }
    if (!join_and_check(dir, L"data\\_lvl_pc\\core.lvl")) {
        MessageBoxW(hwnd, L"这个目录看起来不是正确的 GameData 文件夹（找不到 data\\_lvl_pc\\core.lvl）。\r\n\r\n请选择 ...\\Star Wars - Battlefront 2\\GameData", L"路径不对", MB_ICONWARNING);
        return;
    }
    int auto_mode = (SendMessageW(GetDlgItem(hwnd, ID_R_AUTO), BM_GETCHECK, 0, 0) == BST_CHECKED);
    int rm_mode = (SendMessageW(GetDlgItem(hwnd, ID_R_RM), BM_GETCHECK, 0, 0) == BST_CHECKED);
    int embed = 1;   // 只提供封装动画版本
    int isRemaster = auto_mode ? join_and_check(dir, L"addon\\Remaster") : rm_mode;

    SetWindowTextW(g_hStatus, L"正在安装，请稍候…");
    SendMessageW(g_hProg, PBM_SETPOS, 0, 0);
    int r = run_install(dir, isRemaster, embed, 0);
    if (r >= 0) {
        SendMessageW(g_hProg, PBM_SETPOS, 100, 0);
        SetWindowTextW(g_hStatus, L"安装完成");
        MessageBoxW(hwnd, L"安装完成！\r\n\r\n以后请用游戏目录下的 SWBF2_CN.exe 启动。\r\n如需还原，可用 GOG Galaxy 的「验证/修复文件」。", L"成功", MB_ICONINFORMATION);
    } else {
        SetWindowTextW(g_hStatus, L"安装失败，详见 install_cn.log");
        MessageBoxW(hwnd, L"安装过程中出错，请查看 GameData\\install_cn.log。", L"失败", MB_ICONERROR);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE h = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        CreateWindowW(L"STATIC", L"游戏目录（GameData）：", WS_CHILD | WS_VISIBLE, 18, 16, 300, 20, hwnd, NULL, h, NULL);
        g_hPath = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 18, 40, 470, 24, hwnd, (HMENU)ID_PATH, h, NULL);
        CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 496, 40, 80, 24, hwnd, (HMENU)ID_BROWSE, h, NULL);
        CreateWindowW(L"STATIC", L"游戏版本：", WS_CHILD | WS_VISIBLE, 18, 78, 200, 20, hwnd, NULL, h, NULL);
        CreateWindowW(L"BUTTON", L"自动检测（推荐）", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 18, 98, 200, 22, hwnd, (HMENU)ID_R_AUTO, h, NULL);
        CreateWindowW(L"BUTTON", L"Remaster 1.5r2 版", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 18, 120, 200, 22, hwnd, (HMENU)ID_R_RM, h, NULL);
        CreateWindowW(L"BUTTON", L"原版 1.1 版", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 18, 142, 200, 22, hwnd, (HMENU)ID_R_VA, h, NULL);
        CreateWindowW(L"STATIC", L"过场与开场动画字幕：已烧录进影片（无需额外设置）", WS_CHILD | WS_VISIBLE, 250, 98, 330, 44, hwnd, NULL, h, NULL);
        g_hBtn = CreateWindowW(L"BUTTON", L"开始安装", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 18, 180, 120, 32, hwnd, (HMENU)ID_INSTALL, h, NULL);
        g_hStatus = CreateWindowW(L"STATIC", L"请选择游戏目录后点击安装", WS_CHILD | WS_VISIBLE, 150, 186, 420, 20, hwnd, (HMENU)ID_STATUS, h, NULL);
        g_hProg = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE, 18, 224, 558, 18, hwnd, (HMENU)ID_PROG, h, NULL);
        CreateWindowW(L"STATIC", L"v1.1.3  ·  界面/字体/过场字幕/开场动画  ·  兼容 Windows XP SP3+", WS_CHILD | WS_VISIBLE, 18, 250, 560, 20, hwnd, NULL, h, NULL);
        {
            HWND c = GetWindow(hwnd, GW_CHILD);
            while (c) { SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE); c = GetWindow(c, GW_HWNDNEXT); }
        }
        CheckRadioButton(hwnd, ID_R_AUTO, ID_R_VA, ID_R_AUTO);
        const WCHAR* guesses[] = {
            L"E:\\GOG Games\\Star Wars - Battlefront 2\\GameData",
            L"C:\\GOG Games\\Star Wars - Battlefront 2\\GameData",
            L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Star Wars Battlefront II Classic\\GameData",
        };
        for (int i = 0; i < 3; i++) {
            if (join_and_check(guesses[i], L"data\\_lvl_pc\\core.lvl")) { SetWindowTextW(g_hPath, guesses[i]); break; }
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_BROWSE) {
            BROWSEINFOW bi;
            bi.hwndOwner = hwnd; bi.pidlRoot = NULL; bi.pszDisplayName = NULL;
            bi.lpszTitle = L"选择 星球大战：前线 II 的 GameData 文件夹";
            bi.ulFlags = BIF_RETURNONLYFSDIRS; bi.lpfn = NULL; bi.lParam = 0; bi.iImage = 0;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                WCHAR p[MAX_PATH];
                p[0] = 0;
                SHGetPathFromIDListW(pidl, p);
                CoTaskMemFree(pidl);
                if (p[0]) SetWindowTextW(g_hPath, p);
            }
        } else if (LOWORD(wp) == ID_INSTALL) {
            EnableWindow(g_hBtn, FALSE);
            do_install_gui(hwnd);
            EnableWindow(g_hBtn, TRUE);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void __stdcall EntryPoint() {
    GetModuleFileNameW(NULL, g_self, MAX_PATH);

    /* 命令行（shell32 CommandLineToArgvW，XP 可用） */
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        const WCHAR* sdir = NULL; int srm = 1, semb = 1, silent = 0;
        for (int i = 1; argv && i < argc; i++) {
            if (str_cmpiW(argv[i], L"--silent") == 0) silent = 1;
            else if (str_ncmpW(argv[i], L"--dir=", 6) == 0) sdir = argv[i] + 6;
            else if (str_ncmpW(argv[i], L"--edition=", 10) == 0) srm = (str_cmpiW(argv[i] + 10, L"remaster") == 0);
            else if (str_ncmpW(argv[i], L"--mode=", 7) == 0) semb = 1;
        }
        if (silent && sdir) {
            if (!load_index()) ExitProcess(2);
            int n = run_install(sdir, srm, semb, 1);
            ExitProcess(n >= 0 ? 0 : 3);
        }
    }

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    if (!load_index()) {
        MessageBoxW(NULL, L"安装器数据损坏（找不到内嵌文件）。\r\n请重新下载完整的安装程序。", L"错误", MB_ICONERROR);
        ExitProcess(1);
    }
    WNDCLASSEXW wc;
    wc.cbSize = sizeof(wc);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"SWBF2CNInstallerXP";
    wc.hIconSm = NULL;
    RegisterClassExW(&wc);
    g_hMain = CreateWindowExW(0, L"SWBF2CNInstallerXP", L"星球大战：前线 II 简体中文补丁 安装器 v1.1.3",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 620, 320, NULL, NULL, wc.hInstance, NULL);
    if (!g_hMain) ExitProcess(1);
    ShowWindow(g_hMain, SW_SHOW);
    UpdateWindow(g_hMain);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    ExitProcess(0);
}