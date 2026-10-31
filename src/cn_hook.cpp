
// cn_hook.cpp - SWBF2(GOG+Remaster) 文本定位 hook（内联 hook 版）
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned int (__thiscall *fn_lookup_t)(void* self, unsigned int code);

static FILE* g_log = NULL;
static unsigned int g_tramp = 0;         // 原始函数前 8 字节 + jmp 回去
static unsigned int g_lookupAddr = 0;
static volatile int g_mode = 0;          // 0=none 1=载入页 2=单位数量
static unsigned char g_tmp[24];
static unsigned char g_zero[24];
static int g_shift = 0;
static int g_unitShift = 41;
static int g_hits = 0, g_mods = 0;
extern "C" void overlay_start();
extern "C" void subs_on_movie(const char* name);
static void plog(const char* fmt, ...);
static unsigned char g_modeFh[16]; static int g_nModeFh = 0;
static void note_mode_fh(unsigned char fh) {
    for (int i = 0; i < g_nModeFh; i++) if (g_modeFh[i] == fh) return;
    if (g_nModeFh < 16) { g_modeFh[g_nModeFh++] = fh; plog("mode1 fh=%u seen", fh); }
}

static void lookup_thunk(void);

static void plog(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

static int g_dbgCjk = 0;
static unsigned int c_lookup(void* self, unsigned int code) {
    unsigned int ch = code & 0xFFFF;

    if (ch == 0x0301) { g_mode = 1; g_hits++; return (unsigned int)g_zero; }
    if (ch == 0x0302) { g_mode = 0; return (unsigned int)g_zero; }
    if (ch == 0x0305) { g_mode = 2; g_hits++; return (unsigned int)g_zero; }
    if (ch == 0x0306) { g_mode = 0; return (unsigned int)g_zero; }

    unsigned int r;
    __try { r = ((fn_lookup_t)g_tramp)(self, code); }
    __except (1) { return 0; }
    if (g_mode == 0 || !r) return r;

    unsigned char fh = *(unsigned char*)((char*)self + 0x0E);
    if (g_mode == 1) note_mode_fh(fh);
    int scaleW = 100;
    int scaleH = 100, shift = 0;
    if (g_mode == 2) { scaleH = 100; shift = -g_unitShift; }
    else if (fh == 22 || fh == 23 || fh == 27 || fh == 33) { scaleH = 100; scaleW = 100; shift = g_shift; }
    else { scaleH = 100; scaleW = 100; shift = g_shift - 10; }

    __try {
        memcpy(g_tmp, (void*)r, 24);
        int l = g_tmp[4], rr = g_tmp[5], t = g_tmp[6], b = g_tmp[7];
        int ww = rr - l;
        int hh = b - t;
        int nh = hh * scaleH / 100; if (nh < 1) nh = 1;
        int nt = t + shift; if (nt < 0) nt = 0;
        int nb = nt + nh; if (nb > 255) nb = 255; if (nb <= nt) nb = nt + 1;
        int nw = ww * scaleW / 100; if (nw < 1) nw = 1;
        int adv = g_tmp[3];
        adv = adv * scaleW / 100; if (adv > 255) adv = 255;
        g_tmp[3] = (unsigned char)adv;
        g_tmp[4] = (unsigned char)l;
        int nr = l + nw; if (nr > 255) nr = 255; if (nr <= l) nr = l + 1;
        g_tmp[5] = (unsigned char)nr;
        g_tmp[6] = (unsigned char)nt;
        g_tmp[7] = (unsigned char)nb;
        g_mods++;
        return (unsigned int)g_tmp;
    } __except (1) { return r; }
}

__declspec(naked) static void lookup_thunk(void) {
    __asm {
        push ebp
        mov  ebp, esp
        push dword ptr [ebp+8]
        push ecx
        call c_lookup
        add  esp, 8
        mov  esp, ebp
        pop  ebp
        ret  4
    }
}

// ---- 影片请求钩子 ----
static unsigned int g_movieTramp = 0;
static void movie_thunk(void);
typedef const char* (__cdecl *tLuaStr)(void* L, int idx, int def);
static void install_movie_hook() {
    unsigned int addr = 0x426A60;
    unsigned char* p = (unsigned char*)addr;
    // 特征校验：51 (push ecx) + E8 rel32 (call ...)，且 call 目标应为 0x630C60
    __try {
        if (p[0] != 0x51 || p[1] != 0xE8) { plog("movie hook: prologue mismatch, skipped"); return; }
        int rel = *(int*)(p + 2);
        unsigned int target = (addr + 6) + rel;
        if (target != 0x630C60) { plog("movie hook: call target %p != 0x630C60, skipped", (void*)target); return; }
    } __except (1) { plog("movie hook: read failed, skipped"); return; }
    unsigned char* t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return;
    // 原函数前 6 字节: 51 (push ecx) + e8 rel32 (call 0x630c60)
    // 跳板里重建，避免相对寻址错位
    t[0] = 0x51;
    t[1] = 0xE8;
    *(int*)(t + 2) = (int)(0x630C60 - ((unsigned int)t + 1 + 5));
    t[6] = 0xE9;
    *(int*)(t + 7) = (int)((addr + 6) - ((unsigned int)t + 6 + 5));
    DWORD old;
    if (!VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) return;
    p[0] = 0xE9;
    *(int*)(p + 1) = (int)((unsigned int)movie_thunk - (addr + 5));
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
    g_movieTramp = (unsigned int)t;
    plog("movie hook installed at %p tramp %p", (void*)addr, (void*)t);
}
static void log_movie_arg() {
    __try {
        void* L = *(void**)0x1d77f68;
        if (!L) { plog("movie request: (no lua state)"); return; }
        const char* name = ((tLuaStr)0x70dd00)(L, 2, 0);
        plog("movie request: %s", name ? name : "(null)");
        if (name) subs_on_movie(name);
    } __except (1) { plog("movie arg read failed"); }
}
__declspec(naked) static void movie_thunk(void) {
    __asm {
        pushad
        call log_movie_arg
        popad
        mov  eax, [g_movieTramp]
        jmp  eax
    }
}

// ---- iftext diag: IFText_SetString / SetTextBox 探针 ----
static unsigned int g_setstrTramp = 0;
static unsigned int g_setboxTramp = 0;
static void setstr_thunk(void);
static void setbox_thunk(void);
static int g_logCount = 0;

static void* lua_base_of(void* L) { return *(void**)((char*)L + 12); }

static void diag_setstring() {
    __try {
        void* L = *(void**)0x1d77f68;
        if (!L || g_logCount > 400) return;
        int top = ((int (__cdecl*)(void*))0x70e000)(L);
        if (top < 1) return;
        void* el = ((void* (__cdecl*)(void*, int))0x70e630)(L, 1);
        const char* s = (const char*)((const char* (__cdecl*)(void*, int, int))0x70dd00)(L, 2, 0);
        if (!s) return;
        static unsigned int seen[256]; static int nSeen = 0;
        unsigned int key = 2166136261u;
        for (int i = 0; i < 16 && s[i]; i++) { key ^= (unsigned char)s[i]; key *= 16777619u; }
        for (int i = 0; i < nSeen; i++) if (seen[i] == key) return;
        if (nSeen < 256) seen[nSeen++] = key;
        char hex[80] = {0};
        for (int i = 0; i < 20 && s[i]; i++) sprintf(hex + i * 3, "%02x ", (unsigned char)s[i]);
        g_logCount++;
        plog("iftext str el=%p hex=%s", el, hex);
    } __except (1) { plog("diag_setstring fault"); }
}

static void diag_settextbox() {
    __try {
        void* L = *(void**)0x1d77f68;
        if (!L || g_logCount > 400) return;
        void* base = lua_base_of(L);
        if (!base) return;
        void* el = ((void* (__cdecl*)(void*, int))0x70e630)(L, 1);
        if (!el) return;
        int top = ((int (__cdecl*)(void*))0x70e000)(L);
        char buf[220]; int n = sprintf(buf, "iftext box el=%p top=%d", el, top);
        for (int a = 2; a <= 5 && a <= top; a++) {
            unsigned char* tv = (unsigned char*)base + (a - 1) * 16;
            int tt = *(int*)(tv + 8);
            double d = *(double*)tv;
            n += sprintf(buf + n, " a%d(tt=%d,%.4f)", a, tt, d);
        }
        g_logCount++;
        plog("%s", buf);
    } __except (1) { plog("diag_settextbox fault"); }
}

__declspec(naked) static void setstr_thunk(void) {
    __asm {
        pushad
        call diag_setstring
        popad
        mov  eax, [g_setstrTramp]
        jmp  eax
    }
}
__declspec(naked) static void setbox_thunk(void) {
    __asm {
        pushad
        call diag_settextbox
        popad
        mov  eax, [g_setboxTramp]
        jmp  eax
    }
}

static unsigned int make_hook(unsigned int addr, void* thunk) {
    unsigned char* p = (unsigned char*)addr;
    unsigned char* t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return 0;
    memcpy(t, p, 5);
    t[5] = 0xE9;
    *(int*)(t + 6) = (int)((addr + 5) - ((unsigned int)t + 5 + 5));
    DWORD old;
    if (!VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) return 0;
    p[0] = 0xE9;
    *(int*)(p + 1) = (int)((unsigned int)thunk - (addr + 5));
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
    return (unsigned int)t;
}

static void install_iftext_diag() {
    plog("iftext diag: begin");
    g_setstrTramp = make_hook(0x422300, (void*)setstr_thunk);
    plog("iftext: setstr hooked tramp=%p", (void*)g_setstrTramp);
    g_setboxTramp = make_hook(0x422F40, (void*)setbox_thunk);
    plog("iftext: setbox hooked tramp=%p", (void*)g_setboxTramp);
}

// ---- ScriptCB_PlayMovie (0x426740) 探针 ----
static unsigned int g_playmovieTramp = 0;
static void playmovie_thunk(void);
static void log_playmovie_arg() {
    __try {
        void* L = *(void**)0x1d77f68;
        if (!L) { plog("playmovie: (no lua state)"); return; }
        const char* name = ((const char* (__cdecl*)(void*, int, int))0x70dd00)(L, 1, 0);
        if (name && name[0]) {
            plog("playmovie request: %s", name);
            subs_on_movie(name);
        }
    } __except (1) { plog("playmovie arg read failed"); }
}
static void install_playmovie_hook() {
    unsigned int addr = 0x426740;
    unsigned char* p = (unsigned char*)addr;
    __try {
        if (p[0] != 0xA1) { plog("playmovie hook: prologue mismatch, skipped"); return; }
    } __except (1) { plog("playmovie hook: read failed"); return; }
    unsigned char* t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return;
    memcpy(t, p, 5);
    t[5] = 0xE9;
    *(int*)(t + 6) = (int)((addr + 5) - ((unsigned int)t + 5 + 5));
    DWORD old;
    if (!VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) return;
    p[0] = 0xE9;
    *(int*)(p + 1) = (int)((unsigned int)playmovie_thunk - (addr + 5));
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
    g_playmovieTramp = (unsigned int)t;
    plog("playmovie hook installed tramp %p", (void*)t);
}

__declspec(naked) static void playmovie_thunk(void) {
    __asm {
        pushad
        call log_playmovie_arg
        popad
        mov  eax, [g_playmovieTramp]
        jmp  eax
    }
}


static const unsigned char SIG_LOOKUP[8] = { 0x51, 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x10, 0x56 };

static void install_inline(unsigned int addr) {
    unsigned char* p = (unsigned char*)addr;
    unsigned char* t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) { plog("VirtualAlloc trampoline failed"); return; }
    memcpy(t, p, 8);
    t[8] = 0xE9;
    *(int*)(t + 9) = (int)((addr + 8) - ((unsigned int)t + 8 + 5));
    g_tramp = (unsigned int)t;

    DWORD old;
    if (!VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) { plog("VirtualProtect failed"); return; }
    p[0] = 0xE9;
    *(int*)(p + 1) = (int)((unsigned int)lookup_thunk - (addr + 5));
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
    plog("inline hook installed at %p, trampoline %p", (void*)addr, (void*)g_tramp);
}

static int in_range(unsigned int p, unsigned int lo, unsigned int hi) { return p >= lo && p < hi; }

static DWORD WINAPI init_thread(LPVOID) {
    char path[MAX_PATH];
    GetModuleFileNameA((HMODULE)NULL, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = 0;
    strcat(path, "cn_hook.log");
    g_log = fopen(path, "w");
    plog("hook dll loaded (inline)");
    overlay_start();
    plog("subtitle overlay started");

    HMODULE base = GetModuleHandleA(NULL);
    unsigned int lo = (unsigned int)base, hi = lo + 0x800000;

    for (int pass = 0; pass < 40 && g_lookupAddr == 0; pass++) {
        MEMORY_BASIC_INFORMATION mbi;
        unsigned char* p = (unsigned char*)0x00010000;
        while ((unsigned int)p < 0x7FFF0000) {
            if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
            if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & PAGE_GUARD) &&
                (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) {
                unsigned char* b = (unsigned char*)mbi.BaseAddress;
                unsigned int sz = mbi.RegionSize;
                for (unsigned int off = 0; off + 0x30 < sz; off += 4) {
                    unsigned char* a = b + off;
                    if (*(unsigned short*)a != 25330) continue;
                    unsigned char* obj = a - 0xc;
                    if (*(unsigned char*)(obj + 0x1c) != 1) continue;
                    unsigned int runs = *(unsigned int*)(obj + 0x24);
                    if (runs < 2 || runs > 16) continue;
                    unsigned int vt = *(unsigned int*)obj;
                    if (!in_range(vt, lo, hi)) continue;
                    unsigned int cand = *(unsigned int*)(vt + 0x10);
                    if (!in_range(cand, lo, hi)) continue;
                    if (memcmp((void*)cand, SIG_LOOKUP, 8) != 0) continue;
                    g_lookupAddr = cand;
                    break;
                }
            }
            if (g_lookupAddr) break;
            p = (unsigned char*)mbi.BaseAddress + mbi.RegionSize;
        }
        if (!g_lookupAddr) Sleep(250);
    }
    install_movie_hook();
    install_playmovie_hook();
    // install_iftext_diag();  // 诊断用，暂关闭（会导致启动崩溃）
    if (!g_lookupAddr) { plog("lookup NOT found"); return 0; }
    plog("lookup found at %p", (void*)g_lookupAddr);
    install_inline(g_lookupAddr);

    for (;;) {
        Sleep(5000);
        plog("status: markers=%d modified=%d mode=%d", g_hits, g_mods, g_mode);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        CreateThread(NULL, 0, init_thread, NULL, 0, NULL);
    }
    return TRUE;
}