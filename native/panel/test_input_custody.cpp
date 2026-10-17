// Actual DrainInput code, fake window/cursor calls only. No desktop cursor query,
// window creation, focus change, input hook, device, or ClipCursor invocation.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include "dbce_panel_input.h"
#include "dbce_panel.h"
#include "../third_party/imgui/imgui.h"

namespace {
HWND panelWindow, foregroundWindow, panelRoot, foregroundRoot;
int clipCalls;
bool panelOpen;
HWND WINAPI fakeForeground() { return foregroundWindow; }
HWND WINAPI fakeAncestor(HWND window, UINT) {
    return window == panelWindow ? panelRoot : foregroundRoot;
}
BOOL WINAPI fakeClip(const RECT*) { ++clipCalls; return TRUE; }
BOOL WINAPI fakeCursor(POINT*) { return FALSE; }
BOOL WINAPI fakeClient(HWND, RECT*) { return FALSE; }
BOOL WINAPI fakeScreen(HWND, POINT*) { return FALSE; }
}

#define GetForegroundWindow fakeForeground
#define GetAncestor fakeAncestor
#define ClipCursor fakeClip
#define GetCursorPos fakeCursor
#define GetClientRect fakeClient
#define ScreenToClient fakeScreen
#include "dbce_panel_input.cpp"
#undef GetForegroundWindow
#undef GetAncestor
#undef ClipCursor
#undef GetCursorPos
#undef GetClientRect
#undef ScreenToClient

namespace dbce { namespace panel {
bool IsOpen() { return panelOpen; }
void SetOpen(bool open) { panelOpen = open; }
void Toggle() { panelOpen = !panelOpen; }
bool StopFfbAvailable() { return false; }
void StopFfbFromKey() {}
}}

int main()
{
    ImGui::CreateContext();
    const auto handle = [](std::uintptr_t value) { return reinterpret_cast<HWND>(value); };
    int checks = 0;
    const auto run = [&](const char* label, HWND window, HWND foreground,
                         HWND root, HWND activeRoot, bool open, int expected) {
        panelWindow = window; foregroundWindow = foreground;
        panelRoot = root; foregroundRoot = activeRoot; panelOpen = open;
        dbce::panel::g_hwnd = window;
        dbce::panel::g_wasOpen = false;
        clipCalls = 0;
        dbce::panel::DrainInput(1920, 1080);
        if (clipCalls != expected) { std::printf("FAIL %s opening: %d\n", label, clipCalls); return false; }
        ++checks;
        clipCalls = 0;
        dbce::panel::DrainInput(1920, 1080);
        if (clipCalls != (expected ? 1 : 0)) { std::printf("FAIL %s steady: %d\n", label, clipCalls); return false; }
        ++checks;
        return true;
    };
    bool ok = true;
    ok &= run("hidden/inactive", handle(1), handle(2), handle(3), handle(4), true, 0);
    ok &= run("missing panel", nullptr, handle(2), nullptr, handle(3), true, 0);
    ok &= run("missing foreground", handle(1), nullptr, handle(3), nullptr, true, 0);
    ok &= run("missing panel root", handle(1), handle(2), nullptr, handle(3), true, 0);
    ok &= run("missing foreground root", handle(1), handle(2), handle(3), nullptr, true, 0);
    ok &= run("both roots missing", handle(1), handle(2), nullptr, nullptr, true, 0);
    ok &= run("same valid root", handle(1), handle(2), handle(3), handle(3), true, 2);
    ok &= run("closed panel", handle(1), handle(2), handle(3), handle(3), false, 0);
    ImGui::DestroyContext();
    std::printf("Input cursor custody: %d/16 checks, %s; fake APIs only\n", checks, ok ? "PASS" : "FAIL");
    return ok && checks == 16 ? 0 : 1;
}
