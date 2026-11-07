// dbce-ui.exe --setup check (UI-3): starts the exe on the art of rally sample descriptor with a copy of
// fixtures\before.ini, hidden, and drives it as a player would with the keyboard (keys posted to its window, which
// go through the panel's own input hook): Right to Force feedback, Down, Enter turns Force feedback off, Down, Left
// twice lowers Strength from 50 to 40, Esc closes. The ini must then equal fixtures\after.ini byte for byte: only
// ffb.enabled and ffb.strength changed, every other byte (comments, other sections, CRLF, spacing) kept (STD-044).
//   test_setup.exe <dbce-ui.exe> <descriptor.json> <fixtures dir> <work dir>
#include <windows.h>
#include <stdio.h>
#include <string>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static std::string readAll(const std::wstring &path)
{
    std::string s;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return s;
    LARGE_INTEGER n;
    if (GetFileSizeEx(f, &n) && n.QuadPart < (1 << 20)) {
        s.resize((size_t)n.QuadPart);
        DWORD got = 0;
        if (!s.empty() && (!ReadFile(f, &s[0], (DWORD)s.size(), &got, nullptr) || got != s.size())) s.clear();
    }
    CloseHandle(f);
    return s;
}

struct Find { DWORD pid; HWND hwnd; };
static BOOL CALLBACK findWindow(HWND h, LPARAM lp)
{
    Find *f = (Find *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 64);
    if (pid == f->pid && !wcscmp(cls, L"dbce-ui-setup")) { f->hwnd = h; return FALSE; }
    return TRUE;
}

static void key(HWND h, UINT vk)
{
    PostMessageW(h, WM_KEYDOWN, vk, 1);
    PostMessageW(h, WM_KEYUP, vk, 0xC0000001);
    Sleep(250);   // navigation applies one command per drawn frame
}

int wmain(int argc, wchar_t **argv)
{
    if (argc < 5) { fprintf(stderr, "usage: test_setup <dbce-ui.exe> <descriptor> <fixtures> <work>\n"); return 2; }
    const std::wstring exe = argv[1], desc = argv[2], fixtures = argv[3], work = argv[4];
    const std::wstring ini = work + L"\\dbce-art.ini";
    CreateDirectoryW(work.c_str(), nullptr);
    CHECK(CopyFileW((fixtures + L"\\before.ini").c_str(), ini.c_str(), FALSE));
    const std::string before = readAll(ini), expected = readAll(fixtures + L"\\after.ini");
    CHECK(!before.empty() && !expected.empty() && before != expected);

    std::wstring cmd = L"\"" + exe + L"\" --setup \"" + desc + L"\" --config \"" + ini + L"\"";
    STARTUPINFOW si = {sizeof si};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;   // nothing appears on the desktop; posted keys still reach the panel's hook
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        fprintf(stderr, "FAIL: cannot start %ls (%lu)\n", exe.c_str(), GetLastError());
        return 1;
    }
    Find f = {pi.dwProcessId, nullptr};
    for (int i = 0; i < 100 && !f.hwnd; ++i) { Sleep(100); EnumWindows(findWindow, (LPARAM)&f); }
    CHECK(f.hwnd != nullptr);
    if (f.hwnd) {
        wchar_t title[128] = {};
        GetWindowTextW(f.hwnd, title, 128);
        CHECK(!wcscmp(title, L"Art of Rally Wheel Mod setup"));
        Sleep(1000);   // first frames, fonts
        CHECK(readAll(ini) == before);   // opening writes nothing
        key(f.hwnd, VK_RIGHT);    // tab row: Controls -> Force feedback
        key(f.hwnd, VK_DOWN);     // Force feedback (the status line is hidden in setup)
        key(f.hwnd, VK_RETURN);   // On -> Off
        key(f.hwnd, VK_DOWN);     // Strength
        key(f.hwnd, VK_LEFT);     // 50 -> 45
        key(f.hwnd, VK_LEFT);     // 45 -> 40
        key(f.hwnd, VK_ESCAPE);   // close: the exe ends
    }
    const DWORD waited = WaitForSingleObject(pi.hProcess, 10000);
    CHECK(waited == WAIT_OBJECT_0);
    if (waited != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 9);   // our own test child, never a game
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CHECK(code == 0);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    const std::string after = readAll(ini);
    CHECK(after == expected);
    if (after != expected) {
        fprintf(stderr, "---- got (%zu bytes)\n%s\n---- expected (%zu bytes)\n%s\n", after.size(), after.c_str(), expected.size(),
                expected.c_str());
    }
    printf(failures ? "FAIL: dbce-ui --setup byte diff (%d)\n" : "PASS: dbce-ui --setup wrote only ffb.enabled and ffb.strength\n",
           failures);
    return failures ? 1 : 0;
}
