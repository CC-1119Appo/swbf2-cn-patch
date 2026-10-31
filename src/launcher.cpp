
#include <windows.h>
#include <stdio.h>
static FILE* lg = NULL;
static void L(const char* s) { if (lg) { fprintf(lg, "%s\n", s); fflush(lg); } }
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    char dir[MAX_PATH], dll[MAX_PATH], exe[MAX_PATH], cmd[MAX_PATH * 2], logp[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) *(slash + 1) = 0;
    strcpy(logp, dir); strcat(logp, "launcher.log");
    lg = fopen(logp, "w");
    L("launcher start");
    strcpy(dll, dir); strcat(dll, "cn_hook.dll");
    strcpy(exe, dir); strcat(exe, "BattlefrontII.exe");
    fprintf(lg, "exe=%s dll=%s\n", exe, dll); fflush(lg);
    if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) { L("ERROR: exe not found"); return 1; }
    if (GetFileAttributesA(dll) == INVALID_FILE_ATTRIBUTES) { L("ERROR: dll not found"); return 1; }
    strcpy(cmd, "\""); strcat(cmd, exe); strcat(cmd, "\"");
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, dir, &si, &pi)) {
        fprintf(lg, "CreateProcess failed err=%lu\n", GetLastError()); fflush(lg);
        MessageBoxA(NULL, "CreateProcess failed", "SWBF2 CN", MB_ICONERROR);
        return 1;
    }
    L("process created (suspended)");
    SIZE_T len = strlen(dll) + 1;
    void* mem = VirtualAllocEx(pi.hProcess, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    fprintf(lg, "VirtualAllocEx=%p err=%lu\n", mem, GetLastError()); fflush(lg);
    if (mem && WriteProcessMemory(pi.hProcess, mem, dll, len, NULL)) {
        L("dll path written");
        void* loadLib = (void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
        HANDLE th = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, mem, 0, NULL);
        fprintf(lg, "thread=%p err=%lu\n", th, GetLastError()); fflush(lg);
        if (th) { WaitForSingleObject(th, 15000); CloseHandle(th); L("LoadLibrary thread done"); }
    } else {
        fprintf(lg, "WriteProcessMemory failed err=%lu\n", GetLastError()); fflush(lg);
    }
    ResumeThread(pi.hThread);
    L("resumed");
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 0;
}
