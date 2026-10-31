
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
#define ID_RESTORE 131
#define ID_DETECT 132
#define ID_RESTORE_ORIG 133
#define ID_RESULT 142
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

static int file_exists_w(const WCHAR* p);

// 备份到 <GameData>\_cn_backup\<相对路径>（首次备份为准；影片不备份，体积太大）
static void backup_existing(const WCHAR* full, const char* rel, const WCHAR* destDir) {
    if (!file_exists_w(full)) return;
    static WCHAR bak[2048];
    str_copyW(bak, destDir, 2048);
    str_catW(bak, L"\\_cn_backup\\", 2048);
    static WCHAR relW[1024];
    MultiByteToWideChar(CP_UTF8, 0, rel, -1, relW, 1024);
    for (int k = 0; relW[k]; k++) if (relW[k] == L'/') relW[k] = L'\\';
    str_catW(bak, relW, 2048);
    if (file_exists_w(bak)) return;                 // 已有备份，保留最早的原件
    make_dirs_w(bak);
    CopyFileW(full, bak, FALSE);
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
    if (!has_prefix(name, "M/")) backup_existing(full, rel, destDir);

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

static void cleanup_subs(const WCHAR* dir);

static unsigned int file_size_w(const WCHAR* p);
static void join2(const WCHAR* dir, const WCHAR* rel, WCHAR* out);
// 删除旧版外挂字幕残留（cn_subs\*.srt）
static void cleanup_subs(const WCHAR* dir) {
    static WCHAR pat[2048];
    str_copyW(pat, dir, 2048);
    str_catW(pat, L"\\cn_subs\\*.srt", 2048);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            static WCHAR full[2048];
            str_copyW(full, dir, 2048);
            str_catW(full, L"\\cn_subs\\", 2048);
            str_catW(full, fd.cFileName, 2048);
            DeleteFileW(full);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    static WCHAR sd[2048];
    str_copyW(sd, dir, 2048);
    str_catW(sd, L"\\cn_subs", 2048);
    RemoveDirectoryW(sd);   // 空目录才会成功
}

static int run_install(const WCHAR* dir, int isRemaster, int embed, int silent) {
    static WCHAR logp[2048];
    str_copyW(logp, dir, 2048);
    str_catW(logp, L"\\install_cn.log", 2048);
    g_logFile = CreateFileW(logp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    log_line(isRemaster ? "edition=remaster" : "edition=vanilla");
    log_line(embed ? "embed=1" : "embed=0");
    {
        char nb[16]; int k = 0;
        nb[k++]='e'; nb[k++]='n'; nb[k++]='t'; nb[k++]='r'; nb[k++]='i'; nb[k++]='e'; nb[k++]='s'; nb[k++]='=';
        k += u32_to_str((unsigned int)g_count, nb + k);
        nb[k] = 0;
        log_line(nb);
    }

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
        // Remaster 自带的 ingame.lvl：仅当仍是未打补丁的原文件时才替换（装了 Extension 的保持原样）
        if (isRemaster && lstrcmpA(n, "R/data/_lvl_pc/ingame.lvl") == 0) {
            static WCHAR probe[2048];
            str_copyW(probe, dir, 2048);
            str_catW(probe, L"\\data\\_lvl_pc\\ingame.lvl", 2048);
            unsigned int sz = file_size_w(probe);
            if (sz != 14555244u) { log_line("skip ingame (extension)"); continue; }
        }
        if (!extract_entry(i, dir, silent)) { log_line("FAILED entry"); ok = 0; break; }
        copied++;
        if (has_prefix(n, "M/")) log_line("copied movie");
    }
    cleanup_subs(dir);
    if (ok) {
        static WCHAR mk[2048];
        join2(dir, L"cn_patch.txt", mk);
        HANDLE mh = CreateFileW(mk, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (mh != INVALID_HANDLE_VALUE) {
            const char* txt = isRemaster ? "v1.2.0 remaster" : "v1.2.0 vanilla";
            DWORD w = 0;
            WriteFile(mh, txt, (DWORD)lstrlenA(txt), &w, NULL);
            CloseHandle(mh);
        }
    }
    char num[24]; u32_to_str((unsigned int)copied, num);
    log_line(ok ? "done, files copied:" : "FAILED, files copied:");
    log_line(num);
    if (g_logFile != INVALID_HANDLE_VALUE) { CloseHandle(g_logFile); g_logFile = INVALID_HANDLE_VALUE; }
    return ok ? copied : -1;
}

static WCHAR g_bakRoot[2048], g_bakDst[2048], g_bakPath[2048], g_relW2[1024];
static int run_restore(const WCHAR* dir) {
    WCHAR* bakRoot = g_bakRoot;
    str_copyW(bakRoot, dir, 2048);
    str_catW(bakRoot, L"\\_cn_backup", 2048);
    if (GetFileAttributesW(bakRoot) == INVALID_FILE_ATTRIBUTES) return 0;
    int n = 0;
    for (unsigned int i = 0; i < g_count; i++) {
        const char* name = g_entries[i].name;
        if (has_prefix(name, "M/")) continue;
        const char* rel = name + 2;
        WCHAR* bak = g_bakPath; WCHAR* dst = g_bakDst;
        str_copyW(bak, bakRoot, 2048);
        WCHAR* relW = g_relW2;
        MultiByteToWideChar(CP_UTF8, 0, rel, -1, relW, 1024);
        for (int k = 0; relW[k]; k++) if (relW[k] == L'/') relW[k] = L'\\';
        str_catW(bak, L"\\", 2048);
        str_catW(bak, relW, 2048);
        if (!file_exists_w(bak)) continue;
        str_copyW(dst, dir, 2048);
        str_catW(dst, L"\\", 2048);
        str_catW(dst, relW, 2048);
        make_dirs_w(dst);
        if (CopyFileW(bak, dst, FALSE)) n++;
    }
    return n;
}

static int g_platform = 0;   // 1=GOG2018 2=GOG经典 3=Steam
static int g_family = 0;     // 0=modern 1=old
static int g_remaster = 0;

static unsigned int file_size_w(const WCHAR* p) {
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER sz; sz.QuadPart = 0;
    GetFileSizeEx(h, &sz);
    CloseHandle(h);
    return (unsigned int)sz.QuadPart;
}

static void join2(const WCHAR* dir, const WCHAR* rel, WCHAR* out) {
    str_copyW(out, dir, 2048);
    str_catW(out, L"\\", 2048);
    str_catW(out, rel, 2048);
}

static int dir_exists_w(const WCHAR* p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

static void detect_version(const WCHAR* dir) {
    static WCHAR p[2048];
    g_platform = 0; g_family = 0; g_remaster = 0;
    join2(dir, L"addon\\Remaster", p);
    g_remaster = (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
    join2(dir, L"BattlefrontII.exe", p);
    unsigned int exe = file_size_w(p);
    if (exe == 4366848u) { g_platform = 1; g_family = 0; }        // GOG 2018
    else if (exe == 4608000u) { g_platform = 2; g_family = 1; }   // GOG 经典
    else if (exe == 4357632u) { g_platform = 3; g_family = 0; }   // Steam
    else {
        join2(dir, L"steam_api.dll", p);
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) { g_platform = 3; g_family = 0; }
        else { g_platform = 0; g_family = 0; }
    }
}

// 从内嵌的 O/<family>/ 与 O/fonts/ 还原原版文件
static int run_restore_original(const WCHAR* dir, int family) {
    const char* pref = family ? "O/old/" : "O/modern/";
    int n = 0;
    for (unsigned int i = 0; i < g_count; i++) {
        const char* name = g_entries[i].name;
        int use = has_prefix(name, pref) || has_prefix(name, "O/fonts/");
        if (!use) continue;
        const char* rel = name + (has_prefix(name, pref) ? lstrlenA(pref) : lstrlenA("O/"));
        static WCHAR full[2048];
        static WCHAR relW[1024];
        MultiByteToWideChar(CP_UTF8, 0, rel, -1, relW, 1024);
        for (int k = 0; relW[k]; k++) if (relW[k] == L'/') relW[k] = L'\\';
        if (has_prefix(name, "O/fonts/")) {
            str_copyW(full, dir, 2048);
            str_catW(full, L"\\addon\\Remaster\\Fonts\\", 2048);
            str_catW(full, relW, 2048);
        } else {
            str_copyW(full, dir, 2048);
            str_catW(full, L"\\", 2048);
            str_catW(full, relW, 2048);
        }
        make_dirs_w(full);
        HANDLE src = CreateFileW(g_self, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (src == INVALID_HANDLE_VALUE) continue;
        LARGE_INTEGER pos; pos.QuadPart = (LONGLONG)g_entries[i].off;
        SetFilePointerEx(src, pos, NULL, FILE_BEGIN);
        HANDLE dst = CreateFileW(full, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (dst == INVALID_HANDLE_VALUE) { CloseHandle(src); continue; }
        static unsigned char buf[1 << 20];
        unsigned __int64 remain = g_entries[i].size;
        DWORD got = 0, wrote = 0;
        while (remain > 0) {
            DWORD want = remain > sizeof(buf) ? (DWORD)sizeof(buf) : (DWORD)remain;
            if (!ReadFile(src, buf, want, &got, NULL) || got == 0) break;
            if (!WriteFile(dst, buf, got, &wrote, NULL) || wrote != got) break;
            remain -= got;
        }
        CloseHandle(src); CloseHandle(dst);
        if (remain == 0) n++;
    }
    return n;
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
    int rm_mode = (SendMessageW(GetDlgItem(hwnd, ID_R_RM), BM_GETCHECK, 0, 0) == BST_CHECKED);
    int embed = 1;
    int isRemaster = rm_mode;

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
        CreateWindowW(L"STATIC", L"游戏目录（GameData）：", WS_CHILD | WS_VISIBLE, 18, 14, 300, 18, hwnd, NULL, h, NULL);
        g_hPath = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 18, 34, 420, 24, hwnd, (HMENU)ID_PATH, h, NULL);
        CreateWindowW(L"BUTTON", L"浏览…", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 446, 34, 70, 24, hwnd, (HMENU)ID_BROWSE, h, NULL);
        CreateWindowW(L"BUTTON", L"检测版本", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 522, 34, 96, 24, hwnd, (HMENU)ID_DETECT, h, NULL);

        CreateWindowW(L"STATIC", L"检测结果：", WS_CHILD | WS_VISIBLE, 18, 68, 200, 18, hwnd, NULL, h, NULL);
        CreateWindowW(L"EDIT", L"（点击右侧「检测版本」查看平台与版本）", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY,
                      18, 88, 600, 74, hwnd, (HMENU)ID_RESULT, h, NULL);

        CreateWindowW(L"STATIC", L"安装版本：", WS_CHILD | WS_VISIBLE, 18, 172, 200, 18, hwnd, NULL, h, NULL);
        CreateWindowW(L"BUTTON", L"Remaster 1.5r2 版", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 18, 192, 220, 20, hwnd, (HMENU)ID_R_RM, h, NULL);
        CreateWindowW(L"BUTTON", L"原版 1.1 版", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 18, 214, 220, 20, hwnd, (HMENU)ID_R_VA, h, NULL);
        CreateWindowW(L"STATIC", L"过场与开场字幕已烧录进影片，无需设置", WS_CHILD | WS_VISIBLE, 250, 192, 370, 40, hwnd, NULL, h, NULL);

        g_hBtn = CreateWindowW(L"BUTTON", L"开始安装", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 18, 268, 110, 30, hwnd, (HMENU)ID_INSTALL, h, NULL);
        CreateWindowW(L"BUTTON", L"还原原版", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 138, 268, 110, 30, hwnd, (HMENU)ID_RESTORE_ORIG, h, NULL);
        CreateWindowW(L"BUTTON", L"还原备份", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 258, 268, 110, 30, hwnd, (HMENU)ID_RESTORE, h, NULL);

        g_hStatus = CreateWindowW(L"STATIC", L"请选择游戏目录后点击「检测版本」", WS_CHILD | WS_VISIBLE, 380, 274, 250, 20, hwnd, (HMENU)ID_STATUS, h, NULL);
        g_hProg = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE, 18, 306, 600, 16, hwnd, (HMENU)ID_PROG, h, NULL);
        CreateWindowW(L"STATIC", L"v1.2.0  ·  界面/字体/过场字幕/开场动画  ·  Windows XP SP3+", WS_CHILD | WS_VISIBLE, 18, 330, 600, 18, hwnd, NULL, h, NULL);

        {
            HWND ch = GetWindow(hwnd, GW_CHILD);
            while (ch) { SendMessageW(ch, WM_SETFONT, (WPARAM)font, TRUE); ch = GetWindow(ch, GW_HWNDNEXT); }
        }
        CheckRadioButton(hwnd, ID_R_RM, ID_R_VA, ID_R_VA);
        {
            const WCHAR* guesses[] = {
                L"E:\\GOG Games\\Star Wars - Battlefront 2\\GameData",
                L"C:\\GOG Games\\Star Wars - Battlefront 2\\GameData",
                L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Star Wars Battlefront II Classic\\GameData",
            };
            for (int i = 0; i < 3; i++) {
                if (join_and_check(guesses[i], L"data\\_lvl_pc\\core.lvl")) { SetWindowTextW(g_hPath, guesses[i]); break; }
            }
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
        } else if (LOWORD(wp) == ID_DETECT) {
            static WCHAR dir[2048];
            dir[0] = 0;
            GetWindowTextW(g_hPath, dir, 2048);
            static WCHAR out[1024];
            out[0] = 0;
            if (!join_and_check(dir, L"data\\_lvl_pc\\core.lvl")) {
                wsprintfW(out, L"路径无效：找不到 data\\_lvl_pc\\core.lvl\r\n请选择 ...\\Star Wars - Battlefront 2\\GameData");
            } else {
                detect_version(dir);
                static WCHAR exePath[2048];
                join2(dir, L"BattlefrontII.exe", exePath);
                unsigned int exe = file_size_w(exePath);
                const WCHAR* plat = L"未知版本";
                if (g_platform == 1) plat = L"GOG 版（2018 修订）";
                else if (g_platform == 2) plat = L"GOG 版（经典版 / 2017 前）";
                else if (g_platform == 3) plat = L"Steam 版";
                static WCHAR mk[2048];
                join2(dir, L"cn_patch.txt", mk);
                WCHAR cnState[96];
                if (file_exists_w(mk)) {
                    HANDLE mh = CreateFileW(mk, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                    static char buf[64];
                    buf[0] = 0;
                    DWORD rd = 0;
                    if (mh != INVALID_HANDLE_VALUE) { ReadFile(mh, buf, 60, &rd, NULL); CloseHandle(mh); }
                    WCHAR ver[64];
                    MultiByteToWideChar(CP_UTF8, 0, buf, -1, ver, 64);
                    wsprintfW(cnState, L"已安装（%s）", ver);
                } else {
                    static WCHAR ck[2048];
                    join2(dir, L"cn_hook.dll", ck);
                    if (file_exists_w(ck)) wsprintfW(cnState, L"已安装（无版本标记）");
                    else wsprintfW(cnState, L"未安装");
                }
                wsprintfW(out, L"平台：%s\r\n主程序：%u 字节\r\nRemaster 1.5r2：%s\r\n汉化补丁：%s\r\n建议安装：%s",
                          plat, exe,
                          g_remaster ? L"已安装" : L"未安装",
                          cnState,
                          g_remaster ? L"Remaster 版" : L"原版 1.1 版");
            }
            SetWindowTextW(GetDlgItem(hwnd, ID_RESULT), out);
            CheckRadioButton(hwnd, ID_R_RM, ID_R_VA, g_remaster ? ID_R_RM : ID_R_VA);
            SetWindowTextW(g_hStatus, g_remaster ? L"已选：Remaster 版" : L"已选：原版 1.1 版");
        } else if (LOWORD(wp) == ID_RESTORE_ORIG) {
            static WCHAR dir[2048];
            dir[0] = 0;
            GetWindowTextW(g_hPath, dir, 2048);
            if (!join_and_check(dir, L"data\\_lvl_pc\\core.lvl")) {
                MessageBoxW(hwnd, L"请先选择正确的 GameData 目录。", L"提示", MB_ICONWARNING);
                return 0;
            }
            detect_version(dir);
            int n = run_restore_original(dir, g_family);
            static WCHAR extra[2048];
            join2(dir, L"cn_hook.dll", extra); DeleteFileW(extra);
            join2(dir, L"SWBF2_CN.exe", extra); DeleteFileW(extra);
            join2(dir, L"cn_patch.txt", extra); DeleteFileW(extra);
            static WCHAR msg[512];
            wsprintfW(msg, L"已还原 %d 个原版文件（core/ingame/shell 及字体）。\r\n\r\n启动器与 SRT 已移除；影片请用 Steam/GOG 的「验证文件」恢复。", n);
            MessageBoxW(hwnd, msg, L"还原原版完成", MB_ICONINFORMATION);
        } else if (LOWORD(wp) == ID_INSTALL) {
            EnableWindow(g_hBtn, FALSE);
            do_install_gui(hwnd);
            EnableWindow(g_hBtn, TRUE);
        } else if (LOWORD(wp) == ID_RESTORE) {
            static WCHAR dir[2048];
            dir[0] = 0;
            GetWindowTextW(g_hPath, dir, 2048);
            if (!join_and_check(dir, L"data\\_lvl_pc\\core.lvl")) {
                MessageBoxW(hwnd, L"请先选择正确的 GameData 目录。", L"提示", MB_ICONWARNING);
                return 0;
            }
            int n = run_restore(dir);
            if (n > 0) {
                static WCHAR msg[256];
                wsprintfW(msg, L"已还原 %d 个文件。\r\n\r\n影片请用 Steam/GOG 的「验证文件」恢复。", n);
                MessageBoxW(hwnd, msg, L"还原完成", MB_ICONINFORMATION);
            } else {
                MessageBoxW(hwnd, L"没有找到安装时生成的备份（GameData\\_cn_backup）。", L"提示", MB_ICONWARNING);
            }
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
            else if (str_cmpiW(argv[i], L"--restore") == 0) silent |= 0x100;
            else if (str_cmpiW(argv[i], L"--detect") == 0) silent |= 0x200;
            else if (str_cmpiW(argv[i], L"--restore-orig") == 0) silent |= 0x400;
        }
        if ((silent & 0x400) && sdir) {
            detect_version(sdir);
            int n = run_restore_original(sdir, g_family);
            static WCHAR rm[2048];
            join2(sdir, L"cn_hook.dll", rm);   DeleteFileW(rm);
            join2(sdir, L"SWBF2_CN.exe", rm);  DeleteFileW(rm);
            join2(sdir, L"cn_patch.txt", rm);  DeleteFileW(rm);
            ExitProcess(n >= 0 ? 0 : 3);
        }
        if ((silent & 0x200) && sdir) {
            detect_version(sdir);
            static WCHAR outp[2048];
            join2(sdir, L"detect.txt", outp);
            HANDLE h = CreateFileW(outp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                char b[256]; int k = 0;
                const char* nm = g_platform == 1 ? "gog2018" : (g_platform == 2 ? "gogold" : (g_platform == 3 ? "steam" : "unknown"));
                while (*nm) b[k++] = *nm++;
                b[k++] = ' '; b[k++] = 'f'; b[k++] = 'a'; b[k++] = 'm'; b[k++] = '=';
                k += u32_to_str((unsigned int)g_family, b + k);
                b[k++] = ' '; b[k++] = 'r'; b[k++] = 'e'; b[k++] = 'm'; b[k++] = '=';
                k += u32_to_str((unsigned int)g_remaster, b + k);
                b[k++] = ' '; b[k++] = 'c'; b[k++] = 'n'; b[k++] = '=';
                {
                    static WCHAR ck[2048];
                    join2(sdir, L"cn_patch.txt", ck);
                    int has = file_exists_w(ck) ? 1 : 0;
                    if (!has) { join2(sdir, L"cn_hook.dll", ck); has = file_exists_w(ck) ? 2 : 0; }
                    k += u32_to_str((unsigned int)has, b + k);
                }
                b[k] = 0;
                DWORD w = 0; WriteFile(h, b, (DWORD)k, &w, NULL); CloseHandle(h);
            }
            ExitProcess(0);
        }
        if ((silent & 0x100) && sdir) {
            if (!load_index()) ExitProcess(2);
            int rn = run_restore(sdir);
            ExitProcess(rn >= 0 ? 0 : 3);
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
    g_hMain = CreateWindowExW(0, L"SWBF2CNInstallerXP", L"星球大战：前线 II 简体中文补丁 安装器 v1.2.0",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 650, 395, NULL, NULL, wc.hInstance, NULL);
    if (!g_hMain) ExitProcess(1);
    ShowWindow(g_hMain, SW_SHOW);
    UpdateWindow(g_hMain);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    ExitProcess(0);
}