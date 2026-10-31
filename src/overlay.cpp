
// overlay.cpp - 影片字幕叠加（分层透明窗口 + SRT 时间轴）
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void olog(const char* s) {
    FILE* f = fopen("overlay.log", "a");
    if (f) { fprintf(f, "%s\n", s); fclose(f); }
}

#define MAX_CUES 512
struct Cue { int start, end; wchar_t text[512]; };
static Cue g_cues[MAX_CUES];
static int g_cueCount = 0;
static volatile int g_playing = 0;
static volatile int g_pending = 0;
static volatile int g_crawlMode = 0;
static int g_haveSrt = 0;
static int g_endMs = 0;
static unsigned long long g_startTick = 0;
static int g_offsetMs = 250;
static wchar_t g_cur[512] = L"";
static HWND g_wnd = NULL;
static volatile int g_textDirty = 0;

static const char* kMovieNames[38] = {
 "hot1cam1","hot1cam2","cormon01","deamon01","felmon01","geomon01","hotmon01","kammon01",
 "kasmon01","musmon01","mygmon01","nabmon01","polmon01","sb1mon01","sb2mon01","sb3mon01",
 "sb4mon01","tanmon01","utamon01","yavmon01","cormon02","deamon02","felmon02","geomon02",
 "hotmon02","kammon02","kasmon02","musmon02","mygmon02","nabmon02","polmon02","sb1mon02",
 "sb2mon02","sb3mon02","sb4mon02","tanmon02","utamon02","yavmon02"
};

static void utf8_to_w(const char* s, wchar_t* out, int cch) {
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cch);
}

static int parse_time(const char* s) {
    int h = 0, m = 0, sec = 0, ms = 0;
    sscanf(s, "%d:%d:%d,%d", &h, &m, &sec, &ms);
    return ((h * 60 + m) * 60 + sec) * 1000 + ms;
}

static void load_srt(const wchar_t* path) {
    g_cueCount = 0;
    FILE* f = _wfopen(path, L"rb");
    if (!f) { olog("srt not found"); return; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    fread(buf, 1, sz, f); buf[sz] = 0; fclose(f);
    if ((unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB) memmove(buf, buf + 3, sz - 2);
    char* p = buf;
    while (*p && g_cueCount < MAX_CUES) {
        while (*p == '\r' || *p == '\n') p++;
        if (!*p) break;
        char* nl = strstr(p, "-->");
        if (!nl) break;
        char* lineStart = nl;
        while (lineStart > p && lineStart[-1] != '\n') lineStart--;
        Cue* c = &g_cues[g_cueCount];
        c->start = parse_time(lineStart);
        c->end = parse_time(nl + 3);
        char* line = strstr(nl, "\n");
        if (!line) break;
        line++;
        c->text[0] = 0;
        for (;;) {
            if (*line == '\r' || *line == '\n' || !*line) break;   // 空行 = 该条结束
            char* e = line;
            while (*e && *e != '\r' && *e != '\n') e++;
            char tmp[512] = {0};
            int n = (int)(e - line); if (n > 500) n = 500;
            memcpy(tmp, line, n);
            wchar_t w[512] = {0};
            utf8_to_w(tmp, w, 512);
            if (c->text[0]) wcscat(c->text, L"\n");
            wcscat(c->text, w);
            line = e;
            if (*line == '\r') line++;
            if (*line == '\n') line++;
        }
        g_cueCount++;
        p = line;
        while (*p && *p != '\r' && *p != '\n') p++;
    }
    free(buf);
    char msg[128];
    sprintf(msg, "srt loaded: cues=%d", g_cueCount);
    olog(msg);
}

static const char* kCrawlNames[2][2] = { {"crawl1", "crawl_1"}, {"crawl2", "crawl_2"} };

extern "C" void subs_on_movie(const char* name) {
    // 开场动画
    for (int i = 0; i < 2; i++) {
        if (_stricmp(name, kCrawlNames[i][0]) == 0) {
            wchar_t p2[MAX_PATH];
            _snwprintf(p2, MAX_PATH, L"cn_subs\\%S.srt", kCrawlNames[i][1]);
            olog("crawl movie -> srt");
            g_crawlMode = 1;
            load_srt(p2);
            g_endMs = 0;
            for (int k = 0; k < g_cueCount; k++) if (g_cues[k].end > g_endMs) g_endMs = g_cues[k].end;
            g_haveSrt = g_cueCount > 0;
            g_playing = 0;
            g_pending = g_haveSrt;
            return;
        }
    }
    int idx = 0;
    for (int i = 0; i < 38; i++) {
        if (_stricmp(name, kMovieNames[i]) == 0) { idx = i + 1; break; }
    }
    char msg[256];
    if (!idx) {
        sprintf(msg, "movie %s -> no mapping (clear subs)", name);
        olog(msg);
        g_playing = 0; g_pending = 0; g_cur[0] = 0;
        return;
    }
    g_crawlMode = 0;
    sprintf(msg, "movie %s -> ingame_%d", name, idx);
    olog(msg);
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"cn_subs\\ingame_%d.srt", idx);
    load_srt(path);
    g_endMs = 0;
    for (int i = 0; i < g_cueCount; i++) if (g_cues[i].end > g_endMs) g_endMs = g_cues[i].end;
    g_haveSrt = g_cueCount > 0;
    g_playing = 0;
    g_pending = g_haveSrt;
}

static void render_layered(HWND hwnd, int w, int h, const wchar_t* text) {
    HDC screen = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old = SelectObject(mem, bmp);
    memset(bits, 0, w * h * 4);

    HDC tmp = CreateCompatibleDC(screen);
    BITMAPINFO bi2 = bi;
    unsigned char* srcbits = NULL;
    HBITMAP b2 = CreateDIBSection(tmp, &bi2, DIB_RGB_COLORS, (void**)&srcbits, NULL, 0);
    HGDIOBJ old2 = SelectObject(tmp, b2);
    memset(srcbits, 0, w * h * 4);
    SetBkMode(tmp, TRANSPARENT);
    SetTextColor(tmp, RGB(255, 255, 255));
    HFONT f = CreateFontW(-26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    HGDIOBJ of = SelectObject(tmp, f);
    RECT rc = {40, h - 170, w - 40, h - 20};
    DrawTextW(tmp, text, -1, &rc, DT_CENTER | DT_BOTTOM | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(tmp, of);
    DeleteObject(f);

    unsigned char* dst = (unsigned char*)bits;
    for (int i = 0; i < w * h; i++) {
        unsigned char b = srcbits[i*4+0], g = srcbits[i*4+1], r = srcbits[i*4+2];
        unsigned char a = (unsigned char)((r + g + b) / 3);
        dst[i*4+0] = (unsigned char)(b * a / 255);
        dst[i*4+1] = (unsigned char)(g * a / 255);
        dst[i*4+2] = (unsigned char)(r * a / 255);
        dst[i*4+3] = a;
    }
    RECT wr; GetWindowRect(hwnd, &wr);
    POINT ptSrc = {0, 0};
    SIZE sz = {w, h};
    POINT ptDst = {0, 0};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd, screen, &ptDst, &sz, mem, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(tmp, old2); DeleteObject(b2); DeleteDC(tmp);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    ReleaseDC(NULL, screen);
}

static DWORD WINAPI overlay_thread(LPVOID) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SWBF2SubOverlay";
    RegisterClassExW(&wc);
    g_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                            L"SWBF2SubOverlay", L"", WS_POPUP, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    if (!g_wnd) { olog("window failed"); return 0; }
    olog("overlay window created");
    { static int once = 0; }
    for (;;) {
        Sleep(50);
        HWND target = NULL;
        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            wchar_t t[256] = {0};
            GetWindowTextW(h, t, 255);
            if (wcsstr(t, L"STAR WARS") || wcsstr(t, L"Star Wars") || wcsstr(t, L"Battlefront")) { *(HWND*)lp = h; return FALSE; }
            return TRUE;
        }, (LPARAM)&target);
        if (!target || IsIconic(target)) { ShowWindow(g_wnd, SW_HIDE); continue; }

        // 真实播放状态：[0x1d77bf8] 为影片对象指针
        // 复刻引擎 ScriptCB_IsMoviePlaying 的判定
        int isPlaying = 0;
        __try {
            if (g_crawlMode) {
                // 开场：用引擎 IsMoviePlaying 的完整判定（跳过能及时识别）
                int state = *(int*)0x1d77c18;
                if (state == 1 || state == 2) isPlaying = 1;
                else if (*(unsigned char*)0x7f5e54) {
                    void* obj = *(void**)0x1d77bf8;
                    if (obj && *(void**)((char*)obj + 0x38) && (*(unsigned char*)((char*)obj + 0x438) & 1))
                        isPlaying = 1;
                }
            } else {
                // 过场：只要影片对象还在就算播放（原本验证可用的判据）
                isPlaying = (*(void**)0x1d77bf8) != 0;
            }
        } __except (1) { isPlaying = 0; }
        if (g_pending && !g_playing && isPlaying) {
            g_startTick = GetTickCount64();
            g_playing = 1;
            olog("movie actually started -> timer start");
        }
        if (g_playing) {
            static unsigned long long lastSeen = 0;
            if (isPlaying) lastSeen = GetTickCount64();
            if (lastSeen && GetTickCount64() - lastSeen > 100) {
                olog("movie stopped (skipped/ended)");
                g_playing = 0; g_pending = 0; lastSeen = 0;
            }
        }
        if (g_playing) {
            unsigned long long el2 = GetTickCount64() - g_startTick;
            if ((int)el2 > g_endMs + 3000) { olog("subtitle run finished"); g_playing = 0; g_pending = 0; }
        }
        static unsigned long long lastLog = 0;
        unsigned long long now = GetTickCount64();
        if (g_playing && now - lastLog > 1000) {
            lastLog = now;
            char m[160];
            sprintf(m, "tick ms=%d playing=%d cues=%d endMs=%d target=%p", (int)(now - g_startTick), g_playing, g_cueCount, g_endMs, (void*)target);
            olog(m);
        }
        wchar_t cur[512] = L"";
        if (g_playing) {
            unsigned long long el = GetTickCount64() - g_startTick;
            int ms = (int)el - g_offsetMs;
            for (int i = 0; i < g_cueCount; i++) {
                if (ms >= g_cues[i].start && ms <= g_cues[i].end) { wcscpy(cur, g_cues[i].text); break; }
            }

        }
        if (!cur[0]) { ShowWindow(g_wnd, SW_HIDE); g_cur[0] = 0; continue; }

        RECT rc; GetClientRect(target, &rc);
        POINT tl = {0, 0}; ClientToScreen(target, &tl);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) continue;
        SetWindowPos(g_wnd, HWND_TOPMOST, tl.x, tl.y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        if (wcscmp(cur, g_cur) != 0) {
            wcscpy(g_cur, cur);
            render_layered(g_wnd, w, h, cur);
            static int shown = 0;
            if (shown < 5) { shown++; char msg[128]; sprintf(msg, "showing cue at %dms", (int)(GetTickCount64() - g_startTick)); olog(msg); }
        }
    }
    return 0;
}

extern "C" void overlay_start() {
    CreateThread(NULL, 0, overlay_thread, NULL, 0, NULL);
}