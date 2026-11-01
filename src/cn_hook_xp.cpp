// cn_hook_xp.cpp - 字体定位钩子（无 CRT 依赖，Windows XP SP3+）
#include <windows.h>

typedef unsigned int (__thiscall *fn_lookup_t)(void* self, unsigned int code);

static void plog(const char* fmt, ...);
static void mcopy(void* dst, const void* src, unsigned int n) {
    __asm {
        mov  esi, src
        mov  edi, dst
        mov  ecx, n
        rep  movsb
        cld
    }
}
static int mcmp(const void* a, const void* b, unsigned int n) {
    const unsigned char* x = (const unsigned char*)a;
    const unsigned char* y = (const unsigned char*)b;
    for (unsigned int i = 0; i < n; i++) {
        unsigned char xa = x[i], yb = y[i];
        if (xa != yb) return (int)xa - (int)yb;
    }
    return 0;
}

static HANDLE g_log = INVALID_HANDLE_VALUE;
static unsigned int g_tramp = 0;
static unsigned int g_lookupAddr = 0;
static volatile int g_mode = 0;
static unsigned char g_tmp[24];
static unsigned char g_zero[24];
static int g_shift = 0;
static int g_vanillaLoadShift = 0;
static int g_unitShift = 41;
static int g_hits = 0, g_mods = 0;
static unsigned char g_modeFh[16];
static int g_nModeFh = 0;

static void plog(const char* fmt, ...) {
    if (g_log == INVALID_HANDLE_VALUE) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    DWORD w = 0;
    WriteFile(g_log, buf, (DWORD)lstrlenA(buf), &w, NULL);
    WriteFile(g_log, "\r\n", 2, &w, NULL);
}

static void note_mode_fh(unsigned char fh) {
    for (int i = 0; i < g_nModeFh; i++) if (g_modeFh[i] == fh) return;
    if (g_nModeFh < 16) {
        g_modeFh[g_nModeFh++] = fh;
        plog("mode1 fh=%u seen", (unsigned int)fh);
    }
}

static void lookup_thunk(void);

static unsigned int c_lookup(void* self, unsigned int code) {
    unsigned int ch = code & 0xFFFF;

    if (ch == 0x0301) { g_mode = 1; g_hits++; return (unsigned int)g_zero; }
    if (ch == 0x0302) { g_mode = 0; return (unsigned int)g_zero; }
    if (ch == 0x0303) { g_mode = 3; g_hits++; return (unsigned int)g_zero; }
    if (ch == 0x0304) { g_mode = 0; return (unsigned int)g_zero; }
    if (ch == 0x0305) { g_mode = 2; g_hits++; return (unsigned int)g_zero; }
    if (ch == 0x0306) { g_mode = 0; return (unsigned int)g_zero; }

    unsigned int r = ((fn_lookup_t)g_tramp)(self, code);
    if (g_mode == 0 || !r) return r;

    unsigned char fh = *(unsigned char*)((char*)self + 0x0E);
    if (g_mode == 1) note_mode_fh(fh);
    int scaleW = 100;
    int scaleH = 100, shift = 0;
    if (g_mode == 3) { shift = g_vanillaLoadShift; }
    else if (g_mode == 2) { shift = -g_unitShift; }
    else if (fh == 22 || fh == 23 || fh == 27 || fh == 33) { shift = g_shift; }
    else { shift = g_shift - 10; }

    {
        mcopy(g_tmp, (void*)r, 24);
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
    }
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

static const unsigned char SIG_LOOKUP[8] = { 0x51, 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x10, 0x56 };

static void install_inline(unsigned int addr) {
    unsigned char* p = (unsigned char*)addr;
    unsigned char* t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) { plog("VirtualAlloc trampoline failed"); return; }
    mcopy(t, p, 8);
    t[8] = 0xE9;
    *(int*)(t + 9) = (int)((addr + 8) - ((unsigned int)t + 8 + 5));
    g_tramp = (unsigned int)t;

    DWORD old;
    if (!VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) { plog("VirtualProtect failed"); return; }
    p[0] = 0xE9;
    *(int*)(p + 1) = (int)((unsigned int)lookup_thunk - (addr + 5));
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
    plog("inline hook installed at %08X, trampoline %08X", addr, g_tramp);
}

static int in_range(unsigned int p, unsigned int lo, unsigned int hi) { return p >= lo && p < hi; }

static DWORD WINAPI init_thread(LPVOID) {
    char path[MAX_PATH];
    GetModuleFileNameA((HMODULE)NULL, path, MAX_PATH);
    int i = lstrlenA(path);
    while (i > 0 && path[i - 1] != '\\') i--;
    path[i] = 0;
    lstrcatA(path, "cn_hook.log");
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    plog("hook dll loaded (no-CRT build)");
    plog("overlay disabled (embedded subtitles build)");

    HMODULE base = GetModuleHandleA(NULL);
    unsigned int lo = (unsigned int)base, hi = lo + 0x800000;

    for (int pass = 0; pass < 12 && g_lookupAddr == 0; pass++) {
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
                    if (mcmp((void*)cand, SIG_LOOKUP, 8) != 0) continue;
                    g_lookupAddr = cand;
                    break;
                }
            }
            if (g_lookupAddr) break;
            p = (unsigned char*)mbi.BaseAddress + mbi.RegionSize;
        }
        if (!g_lookupAddr) Sleep(250);
    }
    if (!g_lookupAddr) { plog("lookup NOT found"); return 0; }
    plog("lookup found at %08X", g_lookupAddr);
    install_inline(g_lookupAddr);
    return 0;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        CreateThread(NULL, 0, init_thread, NULL, 0, NULL);
    }
    return TRUE;
}