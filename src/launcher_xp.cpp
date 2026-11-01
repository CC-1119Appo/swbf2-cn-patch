// launcher_xp.cpp - 无 CRT 依赖的启动器（Windows XP SP3+）
#include <windows.h>

static HANDLE g_log = INVALID_HANDLE_VALUE;

static void L(const char* s) {
    if (g_log == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(g_log, s, (DWORD)lstrlenA(s), &w, NULL);
    WriteFile(g_log, "\r\n", 2, &w, NULL);
}

static void LA(const char* a, const char* b) {
    if (g_log == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(g_log, a, (DWORD)lstrlenA(a), &w, NULL);
    WriteFile(g_log, b, (DWORD)lstrlenA(b), &w, NULL);
    WriteFile(g_log, "\r\n", 2, &w, NULL);
}

static void LNum(const char* prefix, unsigned int v) {
    char buf[64];
    int k = 0;
    while (prefix[k] && k < 50) { buf[k] = prefix[k]; k++; }
    char tmp[16]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n > 0) buf[k++] = tmp[--n];
    buf[k] = 0;
    L(buf);
}

extern "C" void WINAPI EntryPoint(void) {
    char dir[MAX_PATH], dll[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH * 2], logp[MAX_PATH];

    GetModuleFileNameA(NULL, dir, MAX_PATH);
    int n = lstrlenA(dir);
    while (n > 0 && dir[n - 1] != '\\') n--;
    dir[n] = 0;

    lstrcpyA(logp, dir); lstrcatA(logp, "launcher.log");
    g_log = CreateFileA(logp, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    L("launcher start");

    lstrcpyA(dll, dir); lstrcatA(dll, "cn_hook_xp.dll");
    lstrcpyA(exe, dir); lstrcatA(exe, "BattlefrontII.exe");
    LA("exe=", exe);
    LA("dll=", dll);

    if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) { L("ERROR: exe not found"); return; }

    lstrcpyA(cmd, "\""); lstrcatA(cmd, exe); lstrcatA(cmd, "\"");

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, dir, &si, &pi)) {
        LNum("CreateProcess failed err=", GetLastError());
        MessageBoxA(NULL, "CreateProcess failed", "SWBF2 CN", MB_ICONERROR);
        return;
    }
    L("process created (suspended)");

    BOOL injected = FALSE;
    if (GetFileAttributesA(dll) != INVALID_FILE_ATTRIBUTES) {
        SIZE_T len = (SIZE_T)lstrlenA(dll) + 1;
        void* mem = VirtualAllocEx(pi.hProcess, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (mem && WriteProcessMemory(pi.hProcess, mem, dll, len, NULL)) {
            L("dll path written");
            void* loadLib = (void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
            HANDLE th = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, mem, 0, NULL);
            if (th) { WaitForSingleObject(th, 15000); CloseHandle(th); injected = TRUE; L("LoadLibrary thread done"); }
            else LNum("CreateRemoteThread failed err=", GetLastError());
        } else {
            LNum("WriteProcessMemory failed err=", GetLastError());
        }
    } else {
        L("WARN: cn_hook.dll not found, running without font hook");
    }

    ResumeThread(pi.hThread);
    L(injected ? "resumed (with hook)" : "resumed (no hook)");
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (g_log != INVALID_HANDLE_VALUE) CloseHandle(g_log);
}