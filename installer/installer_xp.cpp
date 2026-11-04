
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
#define ID_VER     170
#define ID_BAKMOV  171
#define ID_V110    174
#define ID_V117    175
#define ID_V100    176
#define ID_VRM     177
#define ID_S_OVER  120
#define ID_S_EMB   121
#define ID_INSTALL 130
#define ID_RESTORE 131
#define ID_DETECT 132
#define ID_RESTORE_ORIG 133
#define ID_RESULT 142
#define ID_L_MOD 160
#define ID_L_XP 161
#define ID_PROG    140
#define ID_STATUS  141

static HWND g_hPath, g_hStatus, g_hProg, g_hBtn, g_hMain, g_hRestoreState, g_hVer;
static WCHAR g_self[MAX_PATH];
static HANDLE g_logFile = INVALID_HANDLE_VALUE;

struct Entry { char name[260]; unsigned __int64 off; unsigned __int64 size; unsigned __int64 orig; unsigned char method; };
typedef long (__stdcall *tRtlDecompress)(unsigned short, unsigned char*, unsigned long, unsigned char*, unsigned long, unsigned long*);
static tRtlDecompress g_pDecompress = 0;
static unsigned char* g_dbuf = 0;
static unsigned long g_dbufSize = 0;

static tRtlDecompress get_decompress() {
    if (!g_pDecompress) {
        HMODULE n = LoadLibraryA("ntdll.dll");
        if (n) g_pDecompress = (tRtlDecompress)GetProcAddress(n, "RtlDecompressBuffer");
    }
    return g_pDecompress;
}
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
        unsigned char method = 0;
        ReadFile(h, &method, 1, &got, NULL);
        unsigned long long off = 0, size = 0, orig = 0;
        ReadFile(h, &off, 8, &got, NULL);
        ReadFile(h, &size, 8, &got, NULL);
        ReadFile(h, &orig, 8, &got, NULL);
        g_entries[i].off = off;
        g_entries[i].size = size;
        g_entries[i].orig = orig;
        g_entries[i].method = method;
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

static int g_detected = 0;
static int g_bakMovies = 1;
static int g_moviesRestored = 0;   // 0=不备份/不还原影片

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

static int write_payload(unsigned int i, HANDLE src, HANDLE dst) {
    DWORD got = 0;
    if (g_entries[i].method == 0) {
        static unsigned char buf[1 << 20];
        unsigned __int64 remain = g_entries[i].size;
        DWORD wrote = 0;
        while (remain > 0) {
            DWORD want = remain > sizeof(buf) ? (DWORD)sizeof(buf) : (DWORD)remain;
            if (!ReadFile(src, buf, want, &got, NULL) || got == 0) break;
            if (!WriteFile(dst, buf, got, &wrote, NULL) || wrote != got) break;
            remain -= got;
        }
        return remain == 0;
    }
    tRtlDecompress fn = get_decompress();
    if (!fn) return 0;
    if (g_entries[i].method == 1) {
        unsigned long csz = (unsigned long)g_entries[i].size;
        unsigned long osz = (unsigned long)g_entries[i].orig;
        unsigned char* inb = (unsigned char*)mem_alloc(csz);
        unsigned char* outb = (unsigned char*)mem_alloc(osz);
        int ok = 0;
        if (inb && outb) {
            DWORD rd = 0;
            if (ReadFile(src, inb, csz, &rd, NULL) && rd == csz) {
                unsigned long outLen = 0;
                if (fn(2, outb, osz, inb, csz, &outLen) == 0 && outLen == osz) {
                    DWORD wr = 0;
                    if (WriteFile(dst, outb, osz, &wr, NULL) && wr == osz) ok = 1;
                }
            }
        }
        if (inb) mem_free(inb);
        if (outb) mem_free(outb);
        return ok;
    }
    // method 2：分块
    unsigned long nCh = 0;
    if (!ReadFile(src, &nCh, 4, &got, NULL) || got != 4 || nCh == 0 || nCh > 8192) return 0;
    for (unsigned long ci = 0; ci < nCh; ci++) {
        unsigned long csz = 0, osz = 0;
        if (!ReadFile(src, &csz, 4, &got, NULL) || got != 4) return 0;
        if (!ReadFile(src, &osz, 4, &got, NULL) || got != 4) return 0;
        unsigned char* inb = (unsigned char*)mem_alloc(csz);
        unsigned char* outb = (unsigned char*)mem_alloc(osz);
        int ok = 0;
        if (inb && outb) {
            DWORD rd = 0;
            if (ReadFile(src, inb, csz, &rd, NULL) && rd == csz) {
                unsigned long outLen = 0;
                if (fn(2, outb, osz, inb, csz, &outLen) == 0 && outLen == osz) {
                    DWORD wr = 0;
                    if (WriteFile(dst, outb, osz, &wr, NULL) && wr == osz) ok = 1;
                }
            }
        }
        if (inb) mem_free(inb);
        if (outb) mem_free(outb);
        if (!ok) return 0;
    }
    return 1;
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
    if (!has_prefix(name, "M/") || g_bakMovies) backup_existing(full, rel, destDir);

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
    int okAll = write_payload(i, src, dst);
    CloseHandle(src);
    CloseHandle(dst);
    if (!okAll) { log_lineW(L"ERR write: ", full); return 0; }
    g_doneBytes += g_entries[i].orig;
    if (!silent && g_totalBytes && g_hProg) {
        unsigned int doneK = (unsigned int)(g_doneBytes >> 10);
        unsigned int totalK = (unsigned int)(g_totalBytes >> 10);
        int pct = totalK ? (int)((doneK * 100u) / totalK) : 0;
        SendMessageW(g_hProg, PBM_SETPOS, (WPARAM)pct, 0);
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
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

static int g_family = 0;   // 0=modern 1=old 2=cd10
static void detect_version(const WCHAR* dir);
static const WCHAR* os_name();
static int os_is_xp();

static int g_xpMode = -1;  // 1=XP 版, 0=Vista+ 版, -1=按系统自动

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

    if (g_xpMode < 0) { os_name(); g_xpMode = os_is_xp() ? 1 : 0; }
    {   // 清掉与当前系统不匹配的另一份 DLL，避免残留
        static WCHAR junk[2048];
        join2(dir, g_xpMode == 1 ? L"cn_hook.dll" : L"cn_hook_xp.dll", junk);
        DeleteFileW(junk);
    }
    g_totalBytes = 0; g_doneBytes = 0;
    detect_version(dir);
    const char* pre = isRemaster ? "R/" : (g_family == 2 ? "C/" : "V/");
    for (unsigned int i = 0; i < g_count; i++) {
        const char* n = g_entries[i].name;
        if (has_prefix(n, pre) || has_prefix(n, "S/") || (embed && has_prefix(n, "M/")))
            g_totalBytes += g_entries[i].size;
    }
    int ok = 1, copied = 0;
    for (unsigned int i = 0; i < g_count && ok; i++) {
        const char* n = g_entries[i].name;
        if (!(has_prefix(n, pre) || has_prefix(n, "S/") || (embed && has_prefix(n, "M/")))) continue;
        // 只装与系统匹配的字体模块 DLL
        if (has_prefix(n, "S/")) {
            int isXpDll = (lstrcmpA(n, "S/cn_hook_xp.dll") == 0);
            int isModDll = (lstrcmpA(n, "S/cn_hook.dll") == 0);
            if (g_xpMode == 1 && isModDll) { log_line("skip modern dll"); continue; }
            if (g_xpMode != 1 && isXpDll) { log_line("skip xp dll"); continue; }
        }
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
            const char* txt = isRemaster ? "v1.4.0 remaster" : "v1.4.0 vanilla";
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

// 各版本补丁安装后的期望文件大小（用于判断汉化是否完整）
#define SZ_V_CORE   25065520u
#define SZ_V_INGAME 15041988u
#define SZ_C_CORE   25037332u
#define SZ_R_CORE   19655772u
#define SZ_R_FONT_D 19818060u
#define SZ_R_FONT_2K 44983812u
#define SZ_R_FONT_4K 44983812u
#define SZ_M_CRAWL  114149376u
#define SZ_M_INGAME 762167296u

#define LS_CORE   1
#define LS_INGAME 2
#define LS_FONTS  4
#define LS_MOVIES 8
#define LS_ALL    15

static unsigned int check_localization(const WCHAR* dir, int remaster) {
    static WCHAR p[2048];
    unsigned int have = 0;
    join2(dir, L"data\\_lvl_pc\\core.lvl", p);
    unsigned int core = file_size_w(p);
    int isCd = (g_family == 2);
    if ((!remaster && (core == SZ_V_CORE || core == SZ_C_CORE)) || (remaster && core == SZ_R_CORE)) have |= LS_CORE;

    if (!remaster) {
        join2(dir, L"data\\_lvl_pc\\ingame.lvl", p);
        if (file_size_w(p) == SZ_V_INGAME) have |= LS_INGAME;
    } else {
        join2(dir, L"addon\\Remaster\\Fonts\\arialblack_default.lvl", p);
        unsigned int f1 = file_size_w(p);
        join2(dir, L"addon\\Remaster\\Fonts\\arialblack_2k.lvl", p);
        unsigned int f2 = file_size_w(p);
        join2(dir, L"addon\\Remaster\\Fonts\\arialblack_4k.lvl", p);
        unsigned int f3 = file_size_w(p);
        if (f1 == SZ_R_FONT_D && f2 == SZ_R_FONT_2K && f3 == SZ_R_FONT_4K) have |= LS_FONTS;
    }

    join2(dir, L"data\\_lvl_pc\\movies\\crawl.mvs", p);
    unsigned int m1 = file_size_w(p);
    join2(dir, L"data\\_lvl_pc\\movies\\ingame.mvs", p);
    unsigned int m2 = file_size_w(p);
    if (m1 == SZ_M_CRAWL && m2 == SZ_M_INGAME) have |= LS_MOVIES;

    (void)isCd;
    return have;
}

static unsigned int need_mask(int remaster) {
    return remaster ? (LS_CORE | LS_FONTS) : (LS_CORE | LS_INGAME);
}

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
    else if (exe == 7790592u) { g_platform = 4; g_family = 2; }   // CD 1.0（2005 原始盘）
    else {
        join2(dir, L"steam_api.dll", p);
        if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) { g_platform = 3; g_family = 0; }
        else { g_platform = 0; g_family = 0; }
    }
}

// 从内嵌的 O/<family>/ 与 O/fonts/ 还原原版文件
static int run_restore_original(const WCHAR* dir, int family) {
    const char* pref = (family == 2) ? "O/cd10/" : (family ? "O/old/" : "O/modern/");
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
        int okOne = write_payload(i, src, dst);
        CloseHandle(src); CloseHandle(dst);
        if (okOne) n++;
    }
    // 从安装时备份恢复原版影片（如果有）
    g_moviesRestored = 0;
    if (g_bakMovies) {
        static const WCHAR* mvs[] = { L"crawl.mvs", L"ingame.mvs" };
        for (int mi = 0; mi < 2; mi++) {
            static WCHAR bakM[2048], dstM[2048];
            str_copyW(bakM, dir, 2048);
            str_catW(bakM, L"\\_cn_backup\\data\\_lvl_pc\\movies\\", 2048);
            str_catW(bakM, mvs[mi], 2048);
            if (!file_exists_w(bakM)) continue;
            str_copyW(dstM, dir, 2048);
            str_catW(dstM, L"\\data\\_lvl_pc\\movies\\", 2048);
            str_catW(dstM, mvs[mi], 2048);
            make_dirs_w(dstM);
            if (CopyFileW(bakM, dstM, FALSE)) g_moviesRestored++;
        }
    }
    return n;
}

static unsigned int g_osMajor = 0, g_osMinor = 0, g_osBuild = 0;

// RtlGetVersion 不受 manifest 影响，能取到真实版本（XP 起就存在）
typedef struct tagOSVI {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
} OSVI;

static void os_version() {
    if (g_osMajor) return;
    OSVI vi;
    vi.dwOSVersionInfoSize = sizeof(vi);
    typedef LONG (WINAPI *tRtlGetVersion)(OSVI*);
    tRtlGetVersion fn = (tRtlGetVersion)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (fn && fn(&vi) == 0) {
        g_osMajor = vi.dwMajorVersion;
        g_osMinor = vi.dwMinorVersion;
        g_osBuild = vi.dwBuildNumber;
        return;
    }
    OSVERSIONINFOW vo;
    vo.dwOSVersionInfoSize = sizeof(vo);
    if (GetVersionExW(&vo)) {
        g_osMajor = vo.dwMajorVersion;
        g_osMinor = vo.dwMinorVersion;
        g_osBuild = vo.dwBuildNumber;
    }
}

static const WCHAR* os_name() {
    os_version();
    unsigned int M = g_osMajor, m = g_osMinor, B = g_osBuild;
    if (M == 5 && m == 1) return L"Windows XP";
    if (M == 5 && m == 2) return L"Windows XP x64 / Server 2003";
    if (M == 6 && m == 0) return L"Windows Vista";
    if (M == 6 && m == 1) return L"Windows 7";
    if (M == 6 && m == 2) return L"Windows 8";
    if (M == 6 && m == 3) return L"Windows 8.1";
    if (M == 10 && B >= 22000) return L"Windows 11";
    if (M == 10) return L"Windows 10";
    if (M > 10) return L"Windows（新版本）";
    return L"Windows（其他版本）";
}
static int os_is_xp() { return g_osMajor == 5; }

static int del_tree(const WCHAR* path) {
    static WCHAR pat[2048];
    str_copyW(pat, path, 2048);
    str_catW(pat, L"\\*", 2048);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!lstrcmpW(fd.cFileName, L".") || !lstrcmpW(fd.cFileName, L"..")) continue;
            static WCHAR child[2048];
            str_copyW(child, path, 2048);
            str_catW(child, L"\\", 2048);
            str_catW(child, fd.cFileName, 2048);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                del_tree(child);
            } else {
                SetFileAttributesW(child, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(child);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    return RemoveDirectoryW(path) ? 1 : 0;
}

static void update_restore_state(HWND hwnd, const WCHAR* dir);

static void select_version(HWND hwnd, int id) {
    int ids[4] = { ID_VRM, ID_V110, ID_V117, ID_V100 };
    for (int i = 0; i < 4; i++) {
        HWND b = GetDlgItem(hwnd, ids[i]);
        if (b) SendMessageW(b, BM_SETCHECK, (WPARAM)(ids[i] == id ? BST_CHECKED : BST_UNCHECKED), 0);
    }
}

static void update_buttons(HWND hwnd, const WCHAR* dir) {
    if (g_hBtn) EnableWindow(g_hBtn, g_detected ? TRUE : FALSE);
    HWND ro = GetDlgItem(hwnd, ID_RESTORE_ORIG);
    if (ro) EnableWindow(ro, g_detected ? TRUE : FALSE);
    update_restore_state(hwnd, dir);
}

static void update_restore_state(HWND hwnd, const WCHAR* dir) {
    if (!g_hRestoreState) return;
    static WCHAR bk[2048];
    join2(dir, L"_cn_backup", bk);
    BOOL ok = (GetFileAttributesW(bk) != INVALID_FILE_ATTRIBUTES);
    if (!ok) {
        // 没有备份目录但确实装过补丁（旧版本安装），也允许点击（点击后会提示无备份）
        static WCHAR f1[2048];
        join2(dir, L"cn_patch.txt", f1);
        if (!file_exists_w(f1)) {
            join2(dir, L"cn_hook.dll", f1);
            if (!file_exists_w(f1)) {
                join2(dir, L"cn_hook_xp.dll", f1);
                if (file_exists_w(f1)) ok = TRUE;
            } else ok = TRUE;
        } else ok = TRUE;
    }
    EnableWindow(g_hRestoreState, ok);
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
    g_bakMovies = (SendMessageW(GetDlgItem(hwnd, ID_BAKMOV), BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    int isRemaster = (SendMessageW(GetDlgItem(hwnd, ID_VRM), BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    int embed = 1;
    if (isRemaster && !g_remaster) {
        if (MessageBoxW(hwnd,
                L"当前目录未检测到 Remaster 1.5r2 补丁。\r\n\r\n仍要按 Remaster 版安装吗？\r\n（一般应选择「原版 1.1」对应项）",
                L"未检测到 Remaster", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    }
    if (isRemaster) {
        g_family = (g_remaster ? g_family : 0);
    } else if (SendMessageW(GetDlgItem(hwnd, ID_V100), BM_GETCHECK, 0, 0) == BST_CHECKED) {
        g_family = 2;   // CD 1.0
    } else if (SendMessageW(GetDlgItem(hwnd, ID_V117), BM_GETCHECK, 0, 0) == BST_CHECKED) {
        g_family = 1;   // GOG 2017 / 零售 1.1
    } else {
        g_family = 0;   // Steam / GOG 2018
    }

    SetWindowTextW(g_hStatus, L"正在安装，请稍候…");
    SendMessageW(g_hProg, PBM_SETPOS, 0, 0);
    int r = run_install(dir, isRemaster, embed, 0);
    if (r >= 0) {
        SendMessageW(g_hProg, PBM_SETPOS, 100, 0);
        SetWindowTextW(g_hStatus, L"安装完成");
        update_buttons(hwnd, dir);
        PostMessageW(hwnd, WM_COMMAND, (WPARAM)ID_DETECT, 0);   // 装完自动刷新检测结果
        MessageBoxW(hwnd, L"安装完成！\r\n\r\n本版不含启动器，直接运行游戏目录下的 BattlefrontII.exe 即可。\r\n如需还原，请用安装器的「还原原版」或 Steam/GOG 的「验证文件」。", L"成功", MB_ICONINFORMATION);
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
        CreateWindowW(L"BUTTON", L"Remaster 1.5r2 版", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                      18, 192, 200, 20, hwnd, (HMENU)ID_VRM, h, NULL);
        CreateWindowW(L"BUTTON", L"原版 1.1（Steam / GOG 2018）", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                      18, 216, 205, 20, hwnd, (HMENU)ID_V110, h, NULL);
        CreateWindowW(L"BUTTON", L"原版 1.1（GOG 2017 / 零售）", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                      228, 216, 195, 20, hwnd, (HMENU)ID_V117, h, NULL);
        CreateWindowW(L"BUTTON", L"原版 1.0（2005 初版 CD）", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                      428, 216, 210, 20, hwnd, (HMENU)ID_V100, h, NULL);
        select_version(hwnd, ID_V110);

        CreateWindowW(L"STATIC", L"开始安装：写入中文文本、字体与中文字幕影片；已存在的文件会先备份到 _cn_backup", WS_CHILD | WS_VISIBLE, 18, 248, 620, 18, hwnd, NULL, h, NULL);
        CreateWindowW(L"STATIC", L"还原原版：用内置官方文件覆盖，警告：Remaster 补丁会一并清除", WS_CHILD | WS_VISIBLE, 18, 268, 620, 18, hwnd, NULL, h, NULL);
        CreateWindowW(L"STATIC", L"恢复先前游戏状态：还原安装本补丁之前的文件（没有备份时不可点）", WS_CHILD | WS_VISIBLE, 18, 292, 620, 18, hwnd, NULL, h, NULL);
        CreateWindowW(L"STATIC", L"安装完成后直接运行游戏目录下的 BattlefrontII.exe", WS_CHILD | WS_VISIBLE, 18, 316, 620, 18, hwnd, NULL, h, NULL);

        CreateWindowW(L"BUTTON", L"备份原版影片用于还原（约 +600~900 MB 磁盘）",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 336, 520, 20, hwnd, (HMENU)ID_BAKMOV, h, NULL);
        CheckDlgButton(hwnd, ID_BAKMOV, BST_CHECKED);

        g_hBtn = CreateWindowW(L"BUTTON", L"开始安装", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 18, 366, 110, 30, hwnd, (HMENU)ID_INSTALL, h, NULL);
        CreateWindowW(L"BUTTON", L"还原原版", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 138, 366, 110, 30, hwnd, (HMENU)ID_RESTORE_ORIG, h, NULL);
        g_hRestoreState = CreateWindowW(L"BUTTON", L"恢复先前游戏状态", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 258, 366, 140, 30, hwnd, (HMENU)ID_RESTORE, h, NULL);

        g_hStatus = CreateWindowW(L"STATIC", L"请选择游戏目录后点击「检测版本」", WS_CHILD | WS_VISIBLE, 408, 372, 230, 20, hwnd, (HMENU)ID_STATUS, h, NULL);
        g_hProg = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE, 18, 404, 600, 16, hwnd, (HMENU)ID_PROG, h, NULL);
        CreateWindowW(L"STATIC", L"v1.4.0  ·  界面/字体/过场字幕/开场动画  ·  Windows XP SP3+", WS_CHILD | WS_VISIBLE, 18, 428, 600, 18, hwnd, NULL, h, NULL);

        {
            HWND ch = GetWindow(hwnd, GW_CHILD);
            while (ch) { SendMessageW(ch, WM_SETFONT, (WPARAM)font, TRUE); ch = GetWindow(ch, GW_HWNDNEXT); }
        }
        { static WCHAR g0[2048]; GetWindowTextW(g_hPath, g0, 2048); g_detected = 0; update_buttons(hwnd, g0); }
        os_name();
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
        if (LOWORD(wp) == ID_PATH && HIWORD(wp) == EN_CHANGE) {
            static WCHAR gc[2048];
            GetWindowTextW(g_hPath, gc, 2048);
            g_detected = 0;
            update_buttons(hwnd, gc);
            return 0;
        }
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
                if (p[0]) { SetWindowTextW(g_hPath, p); g_detected = 0; update_buttons(hwnd, p); }
            }
        } else if (LOWORD(wp) == ID_DETECT) {
            static WCHAR dir[2048];
            dir[0] = 0;
            GetWindowTextW(g_hPath, dir, 2048);
            static WCHAR out[1024];
            out[0] = 0;
            if (!join_and_check(dir, L"data\\_lvl_pc\\core.lvl")) {
                wsprintfW(out, L"路径无效：找不到 data\\_lvl_pc\\core.lvl\r\n请选择 ...\\Star Wars - Battlefront 2\\GameData");
                g_detected = 0;
            } else {
                g_detected = 1;
                detect_version(dir);
                static WCHAR exePath[2048];
                join2(dir, L"BattlefrontII.exe", exePath);
                unsigned int exe = file_size_w(exePath);
                const WCHAR* plat = L"未知版本";
                if (g_platform == 1) plat = L"GOG 版（2018 修订）";
                else if (g_platform == 2) plat = L"GOG 版（经典版 / 2017 前）";
                else if (g_platform == 3) plat = L"Steam 版";
                else if (g_platform == 4) plat = L"CD 版 1.0（2005 原始盘）";
                static WCHAR mk[2048];
                join2(dir, L"cn_patch.txt", mk);
                WCHAR ver[64];
                ver[0] = 0;
                if (file_exists_w(mk)) {
                    HANDLE mh = CreateFileW(mk, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                    static char buf[64];
                    buf[0] = 0;
                    DWORD rd = 0;
                    if (mh != INVALID_HANDLE_VALUE) { ReadFile(mh, buf, 60, &rd, NULL); CloseHandle(mh); }
                    MultiByteToWideChar(CP_UTF8, 0, buf, -1, ver, 64);
                }
                unsigned int have = check_localization(dir, g_remaster);
                unsigned int need = need_mask(g_remaster);
                WCHAR cnState[192];
                int hasDll = 0;
                {
                    static WCHAR dk[2048];
                    join2(dir, L"cn_hook.dll", dk);
                    if (file_exists_w(dk)) hasDll = 1;
                    else { join2(dir, L"cn_hook_xp.dll", dk); if (file_exists_w(dk)) hasDll = 1; }
                }
                int installed = (ver[0] != 0) || hasDll || ((have & LS_CORE) != 0);
                if (!installed) {
                    lstrcpyW(cnState, L"未安装汉化包");
                } else if (g_remaster && !(have & LS_CORE)) {
                    lstrcpyW(cnState, L"Remaster 汉化未安装，请点击「开始安装」以补全汉化");
                } else if ((have & need) != need) {
                    lstrcpyW(cnState, L"汉化安装未完成，请点击「开始安装」以补全");
                } else if (ver[0]) {
                    wsprintfW(cnState, L"已安装（完整 · %s）", ver);
                } else {
                    lstrcpyW(cnState, L"已安装（完整）");
                }
                os_version();
                static WCHAR osFull[96];
                wsprintfW(osFull, L"%s  (%u.%u.%u)", os_name(), g_osMajor, g_osMinor, g_osBuild);
                wsprintfW(out, L"操作系统：%s\r\n平台：%s\r\n主程序：%u 字节\r\nRemaster 1.5r2：%s\r\n汉化补丁：%s\r\n建议安装：%s",
                          osFull, plat, exe,
                          g_remaster ? L"已安装" : L"未安装",
                          cnState,
                          g_remaster ? L"Remaster 版" : L"原版 1.1 版");
            }
            SetWindowTextW(GetDlgItem(hwnd, ID_RESULT), out);
            update_restore_state(hwnd, dir);
            if (g_detected) {
                int pick = ID_V110;
                if (g_remaster) pick = ID_VRM;
                else if (g_family == 2) pick = ID_V100;
                else if (g_family == 1) pick = ID_V117;
                else if (g_platform == 3) pick = ID_V110;
                select_version(hwnd, pick);
            }
            update_buttons(hwnd, dir);
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
            static WCHAR rmDir[2048];
            join2(dir, L"addon\\Remaster", rmDir);
            int hasRm = (GetFileAttributesW(rmDir) != INVALID_FILE_ATTRIBUTES);
            const WCHAR* packName = L"GOG 2018 / Steam";
            if (g_family == 2) packName = L"CD 1.0（2005 原始盘）";
            else if (g_family == 1) packName = L"GOG 经典版（2017 前）";
            static WCHAR qmsg[768];
            wsprintfW(qmsg,
                L"将使用内置的【%s】官方原始文件覆盖：\n"
                L"  data\\_lvl_pc\\core.lvl\n"
                L"  data\\_lvl_pc\\ingame.lvl\n"
                L"  data\\_lvl_pc\\shell.lvl\n\n"
                L"警告：若检测到 Remaster 补丁（addon\\Remaster），将一并清除。\n\n"
                L"确定还原为英文原版吗？", packName);
            if (MessageBoxW(hwnd, qmsg, L"还原原版游戏", MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
            int n = run_restore_original(dir, g_family);
            int removedRm = 0;
            if (hasRm) removedRm = del_tree(rmDir);
            static WCHAR extra[2048];
            join2(dir, L"cn_hook.dll", extra); DeleteFileW(extra);
            join2(dir, L"SWBF2_CN.exe", extra); DeleteFileW(extra);
            join2(dir, L"SWBF2_CN_XP.exe", extra); DeleteFileW(extra);
            join2(dir, L"cn_patch.txt", extra); DeleteFileW(extra);
            static WCHAR msg[512];
            wsprintfW(msg, L"已用【%s】原版文件还原 %d 个文件。%s%s\r\n\r\n影片请用 Steam/GOG 的「验证文件」恢复。",
                      packName, n,
                      removedRm ? L"\r\nRemaster 补丁已清除。" : L"",
                      g_moviesRestored > 0 ? L"\r\n已从安装时备份恢复原版影片。" : L"");
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
            if (MessageBoxW(hwnd,
                    L"将把游戏恢复为安装本补丁之前的状态\r\n（使用 GameData\\_cn_backup 中的备份文件）。\r\n\r\n确定继续吗？",
                    L"恢复先前游戏状态", MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
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
            else if (str_ncmpW(argv[i], L"--launcher=", 11) == 0) g_xpMode = (str_cmpiW(argv[i] + 11, L"xp") == 0) ? 1 : 0;
        }
        if ((silent & 0x400) && sdir) {
            if (!load_index()) ExitProcess(2);
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
                const char* nm = g_platform == 1 ? "gog2018" : (g_platform == 2 ? "gogold" : (g_platform == 3 ? "steam" : (g_platform == 4 ? "cd10" : "unknown")));
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
                {
                    unsigned int hv = check_localization(sdir, g_remaster);
                    unsigned int nd = need_mask(g_remaster);
                    b[k++] = ' '; b[k++] = 'l'; b[k++] = 's'; b[k++] = '=';
                    k += u32_to_str(hv, b + k);
                    b[k++] = '/'; k += u32_to_str(nd, b + k);
                }
                os_version();
                b[k++] = ' '; b[k++] = 'o'; b[k++] = 's'; b[k++] = '=';
                k += u32_to_str(g_osMajor, b + k);
                b[k++] = '.'; k += u32_to_str(g_osMinor, b + k);
                b[k++] = '.'; k += u32_to_str(g_osBuild, b + k);
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
            os_name();
            if (g_xpMode < 0) g_xpMode = os_is_xp() ? 1 : 0;
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
    g_hMain = CreateWindowExW(0, L"SWBF2CNInstallerXP", L"星球大战：前线2 简体中文包 安装器",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 700, 492, NULL, NULL, wc.hInstance, NULL);
    if (!g_hMain) ExitProcess(1);
    ShowWindow(g_hMain, SW_SHOW);
    UpdateWindow(g_hMain);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    ExitProcess(0);
}