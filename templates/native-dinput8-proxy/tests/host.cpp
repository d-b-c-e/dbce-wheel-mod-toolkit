// Offline start check: built as build\host\__EXE__ beside a copy of build\dinput8.dll, so the proxy loads under the
// game's exe name. The first DirectInput8Create must start the mod (its log gains the start line and the panel's
// overlay line) and hand back a working DirectInput. No game, no device access beyond creating the interface; the
// force fence is switched on for the run, so nothing could reach a wheel even if one is attached.
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <share.h>
#include <stdio.h>
#include <string.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

static int failures;
static void check(bool ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) failures++; }

static long fileSize(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &a) ? (long)a.nFileSizeLow : 0;
}
static bool logGains(const char *path, long from, const char *needle)
{
    static char b[65536];
    for (int i = 0; i < 100; ++i) {
        if (FILE *f = _fsopen(path, "rb", _SH_DENYNO)) {
            fseek(f, from, SEEK_SET);
            const size_t got = fread(b, 1, sizeof b - 1, f);
            fclose(f);
            b[got] = 0;
            if (strstr(b, needle)) return true;
        }
        Sleep(100);
    }
    return false;
}

int main()
{
    char folder[MAX_PATH] = "", logPath[MAX_PATH] = "", fence[MAX_PATH] = "";
    if (!GetEnvironmentVariableA("LOCALAPPDATA", folder, MAX_PATH - 80)) { puts("FAIL no LOCALAPPDATA"); return 1; }
    strcat_s(folder, "\\dbce");
    CreateDirectoryA(folder, nullptr);
    strcat_s(folder, "\\__ID__");
    CreateDirectoryA(folder, nullptr);
    sprintf_s(logPath, "%s\\dbce-__ID__.log", folder);
    sprintf_s(fence, "%s\\ffb-fence.on", folder);
    const bool hadFence = GetFileAttributesA(fence) != INVALID_FILE_ATTRIBUTES;
    if (!hadFence) { if (FILE *f = fopen(fence, "w")) fclose(f); }
    const long from = fileSize(logPath);

    IDirectInput8A *di = nullptr;
    const HRESULT hr = DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A, (void **)&di, nullptr);
    check(SUCCEEDED(hr) && di, "DirectInput8Create through the proxy succeeds");
    check(logGains(logPath, from, "dbce __NAME__ mod 0.1.0 in __EXE__"), "the log has the mod's start line under the game's exe name");
    check(logGains(logPath, from, "FENCED"), "the start line reports the force fence");
    check(logGains(logPath, from, "fenced interface"), "the game was given the fenced DirectInput");
    if (di) di->Release();

    if (!hadFence) DeleteFileA(fence);
    printf(failures ? "%d check(s) failed\n" : "host checks passed\n", failures);
    return failures ? 1 : 0;
}
