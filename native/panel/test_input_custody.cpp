// Actual message hook/handler and DrainInput, with fabricated MSGs and fake Win32 calls.
// No real queue/provider delivery is established. No desktop cursor query,
// window creation, focus change, input hook, device, or ClipCursor invocation.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "dbce_panel_input.h"
#include "dbce_panel.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_internal.h"

namespace {
HWND panelWindow, foregroundWindow, mockPanelRoot, mockForegroundRoot;
int clipCalls;
bool panelOpen;
bool captureWaiting;
bool compareAvailable;
int compareCalls;
bool cameraKeySet, cameraKeyConsumes;
int cameraKeyCalls, cameraKeyVk, cameraKeyDown, cameraKeyRepeat;
void (*onClient)();
HWND childWindow;
int chainCalls, attachCalls, unhookCalls;
bool attachFails;
HOOKPROC installedCallback;
LRESULT WINAPI fakeChain(HHOOK, int, WPARAM, LPARAM) { ++chainCalls; return 71; }
BOOL WINAPI fakeChild(HWND parent, HWND child) { return parent == panelWindow && child == childWindow; }
DWORD WINAPI fakeThread(HWND, LPDWORD) { return 42; }
HHOOK WINAPI fakeAttach(int kind, HOOKPROC callback, HINSTANCE module, DWORD thread) {
    ++attachCalls;
    if (kind != WH_GETMESSAGE || module || thread != 42) return nullptr;
    installedCallback = callback;
    return attachFails ? nullptr : reinterpret_cast<HHOOK>(std::uintptr_t(123));
}
BOOL WINAPI fakeUnhook(HHOOK) { ++unhookCalls; return TRUE; }

HWND WINAPI fakeForeground() { return foregroundWindow; }
HWND WINAPI fakeAncestor(HWND window, UINT) {
    return window == panelWindow ? mockPanelRoot : mockForegroundRoot;
}
BOOL WINAPI fakeClip(const RECT*) { ++clipCalls; return TRUE; }
BOOL WINAPI fakeCursor(POINT*) { return FALSE; }
BOOL WINAPI fakeClient(HWND, RECT*) {
    if (onClient) { const auto callback = onClient; onClient = nullptr; callback(); }
    return FALSE;
}
BOOL WINAPI fakeScreen(HWND, POINT*) { return FALSE; }
}

#define CallNextHookEx fakeChain
#define IsChild fakeChild
#define GetWindowThreadProcessId fakeThread
#define SetWindowsHookExW fakeAttach
#define UnhookWindowsHookEx fakeUnhook
#define GetForegroundWindow fakeForeground
#define GetAncestor fakeAncestor
#define ClipCursor fakeClip
#define GetCursorPos fakeCursor
#define GetClientRect fakeClient
#define ScreenToClient fakeScreen
#include "dbce_panel_input.cpp"
#undef CallNextHookEx
#undef IsChild
#undef GetWindowThreadProcessId
#undef SetWindowsHookExW
#undef UnhookWindowsHookEx
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
bool CancelBindingCapture() { const bool was = captureWaiting; captureWaiting = false; return was; }
bool NavigateFromKey(int, bool, bool) { return false; }
bool CompareAvailable() { return compareAvailable; }
void CompareFromKey() { ++compareCalls; }
bool CameraKeyFromMessage(int vk, bool down, bool repeat)
{
    if (!cameraKeySet) return false;
    ++cameraKeyCalls; cameraKeyVk = vk; cameraKeyDown = down; cameraKeyRepeat = repeat;
    return cameraKeyConsumes;
}
}}

static int hookChecks, hookFailures;
static std::vector<std::string> traceLines;
static void verify(bool good, const char *label)
{
    ++hookChecks;
    if (!good) { ++hookFailures; std::printf("FAIL message fixture: %s\n", label); }
}
static void traceLog(const char *line)
{
    verify(dbce::panel::g_cs.RecursionCount == 0, "trace logging occurs outside input lock");
    traceLines.emplace_back(line);
}
static void messageChecks(bool trace)
{
    using namespace dbce::panel;
    DetachInput();
    panelWindow = reinterpret_cast<HWND>(std::uintptr_t(1));
    childWindow = reinterpret_cast<HWND>(std::uintptr_t(2));
    const HWND foreign = reinterpret_cast<HWND>(std::uintptr_t(3));
    verify(AttachInput(panelWindow) && installedCallback == getMessageHook && InputWindow() == panelWindow, "actual attach uses fake thread-local hook");
    const int priorAttach = attachCalls;
    verify(AttachInput(panelWindow) && attachCalls == priorAttach, "same-window attach idempotent");
    if (trace) TraceInput("on", traceLog); else TraceInput("off", nullptr);
    const auto dispatch = [](HWND window, UINT kind, WPARAM key, LPARAM bits, bool consumed) {
        MSG m{}; m.hwnd = window; m.message = kind; m.wParam = key; m.lParam = bits;
        const int before = chainCalls;
        verify(getMessageHook(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&m)) == 71 && chainCalls == before + 1, "each message chains once with original result");
        verify(m.message == (consumed ? WM_NULL : kind), "WM_NULL only for consumed input");
        verify(m.hwnd == window && m.wParam == key && m.lParam == bits, "other MSG fields retained");
    };
    verify(getMessageHook(-1, PM_REMOVE, 1) == 71 && !panelOpen, "negative code never dereferences invalid MSG");
    verify(getMessageHook(HC_ACTION, PM_NOREMOVE, 1) == 71 && !panelOpen, "NOREMOVE never dereferences invalid MSG");
    verify(getMessageHook(HC_ACTION, PM_REMOVE, 0) == 71 && !panelOpen, "null MSG chains");
    dispatch(foreign, WM_KEYDOWN, VK_F6, 0, false);
    dispatch(nullptr, WM_KEYDOWN, VK_F6, 0, false);
    verify(!panelOpen && g_count == 0, "foreign/null window do not toggle or enqueue");
    dispatch(panelWindow, WM_KEYDOWN, VK_F6, 0, true);
    verify(panelOpen, "first F6 down opens");
    dispatch(panelWindow, WM_KEYDOWN, VK_F6, (LPARAM(1) << 30), true);
    dispatch(panelWindow, WM_KEYUP, VK_F6, 0, true);
    verify(panelOpen, "F6 repeat/up do not toggle");
    dispatch(childWindow, WM_KEYDOWN, VK_F6, 0, true);
    verify(!panelOpen, "owned child next F6 down closes once");
    dispatch(panelWindow, WM_KEYDOWN, VK_ESCAPE, 0, false);
    verify(!panelOpen, "Escape while closed passes through");
    dispatch(panelWindow, WM_KEYDOWN, VK_F6, 0, true);
    dispatch(panelWindow, WM_SYSKEYDOWN, VK_F4, (LPARAM(1) << 29), false);
    dispatch(panelWindow, WM_SYSKEYDOWN, VK_TAB, (LPARAM(1) << 29), false);
    dispatch(panelWindow, WM_KEYDOWN, VK_LWIN, 0, false);
    dispatch(panelWindow, WM_KEYUP, VK_RWIN, 0, false);
    dispatch(panelWindow, WM_KEYDOWN, VK_ESCAPE, (LPARAM(1) << 30), true);
    dispatch(panelWindow, WM_KEYUP, VK_ESCAPE, 0, true);
    verify(panelOpen, "Escape repeat/up does not close");
    dispatch(panelWindow, WM_KEYDOWN, VK_ESCAPE, 0, true);
    verify(!panelOpen, "Escape nonrepeat closes open panel");
    dispatch(panelWindow, WM_USER, 123, 456, false);
    dispatch(panelWindow, WM_KEYDOWN, VK_F6, 0, true);
    const int queued = g_count;
    dispatch(childWindow, WM_LBUTTONDOWN, 0, 0, true);
    dispatch(childWindow, WM_LBUTTONUP, 0, 0, true);
    verify(g_count == queued + 2, "actual mouse handler enqueues both button edges");
    foregroundWindow = nullptr;
    DrainInput(1920, 1080);
    verify(g_count == 0 && ImGui::GetCurrentContext()->InputEventsQueue.Size > 0, "fabricated mouse events drained into real ImGui IO");
    if (trace) {
        verify(g_trace.callbacks == 21 && g_trace.action == 21 && g_trace.remove == 20 && g_trace.noRemove == 1, "trace distinguishes removed/peek callbacks without reading peek pointer");
        verify(g_trace.owned == 14 && g_trace.child == 3 && g_trace.foreign == 1 && g_trace.nullWindow == 1 && g_trace.nullMessage == 1, "trace ownership categories match fabricated inputs");
        verify(g_trace.f6Down == 6 && g_trace.f6Repeat == 1 && g_trace.f6Up == 1 && g_trace.escDown == 2 && g_trace.escRepeat == 1 && g_trace.escUp == 1, "trace whitelist edge/repeat counters exact");
        verify(g_trace.mouseSeen == 2 && g_trace.mouseQueued == 2 && g_trace.opened == 3 && g_trace.closed == 2 && g_trace.consumed == 11, "trace mouse/open/consumed counters exact");
    } else verify(g_trace.callbacks == 0, "disabled tracing collects no callbacks");
    const int beforeCommand = g_count;
    TraceInput("on", nullptr);   // defined reset: counters only; leave actual panel/queue intact
    verify(panelOpen && g_count == beforeCommand && g_trace.callbacks == 0, "trace on resets counters, never panel or queue");
    if (!trace) TraceInput("off", nullptr);
    dispatch(panelWindow, WM_LBUTTONDOWN, 0, 0, true);
    const int beforeDetach = unhookCalls;
    DetachInput();
    verify(unhookCalls == beforeDetach + 1 && !panelOpen && g_count == 0 && !InputWindow(), "detach unhooks, closes and drops queue");
    dispatch(panelWindow, WM_KEYDOWN, VK_F6, 0, false);
    dispatch(childWindow, WM_LBUTTONDOWN, 0, 0, false);
    verify(!panelOpen && g_count == 0, "late callbacks after detach cannot reopen/enqueue");
    TraceInput("off", traceLog);
}

int main()
{
    ImGui::CreateContext();
    const auto handle = [](std::uintptr_t value) { return reinterpret_cast<HWND>(value); };
    int checks = 0;
    const auto run = [&](const char* label, HWND window, HWND foreground,
                         HWND root, HWND activeRoot, bool open, int expected) {
        panelWindow = window; foregroundWindow = foreground;
        mockPanelRoot = root; mockForegroundRoot = activeRoot; panelOpen = open;
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
    messageChecks(false);
    messageChecks(true);
    using namespace dbce::panel;
    verify(AttachInput(panelWindow), "F7 fixture attaches fake input window");
    panelOpen = false;
    MSG compare{}; compare.hwnd = panelWindow; compare.message = WM_KEYDOWN; compare.wParam = VK_F7;
    verify(!dbce::panel::handle(&compare) && compareCalls == 0, "F7 without capability passes through");
    compareAvailable = true;
    verify(dbce::panel::handle(&compare) && compareCalls == 1 && !panelOpen, "F7 calls current compare route once without opening panel");
    compare.lParam = LPARAM(1) << 30;
    verify(dbce::panel::handle(&compare) && compareCalls == 1, "F7 repeat does not call compare again");
    compare.message = WM_KEYUP; compare.lParam = 0;
    verify(dbce::panel::handle(&compare) && compareCalls == 1, "F7 release is consumed without compare");
    compareAvailable = false;
    // Camera keys: offered to the mod only while the panel is closed; consumed only when the mod says so.
    MSG key{}; key.hwnd = panelWindow; key.message = WM_KEYDOWN; key.wParam = VK_ADD;
    verify(!dbce::panel::handle(&key) && cameraKeyCalls == 0, "numpad + without a camera-key route passes through");
    cameraKeySet = true; cameraKeyConsumes = true;
    verify(dbce::panel::handle(&key) && cameraKeyCalls == 1 && cameraKeyVk == VK_ADD && cameraKeyDown && !cameraKeyRepeat,
           "numpad + down offered to the camera-key route and consumed");
    key.lParam = LPARAM(1) << 30;
    verify(dbce::panel::handle(&key) && cameraKeyCalls == 2 && cameraKeyRepeat, "a repeat is offered as a repeat");
    key.message = WM_KEYUP; key.lParam = (LPARAM(1) << 30) | (LPARAM(1) << 31);
    verify(dbce::panel::handle(&key) && cameraKeyCalls == 3 && !cameraKeyDown, "the release is offered too");
    cameraKeyConsumes = false; key.message = WM_KEYDOWN; key.lParam = 0; key.wParam = 'W';
    verify(!dbce::panel::handle(&key) && cameraKeyCalls == 4, "a key the mod does not take reaches the game");
    compareAvailable = true; compare.message = WM_KEYDOWN; compare.lParam = 0;
    verify(dbce::panel::handle(&compare) && cameraKeyCalls == 4, "F7 keeps its own route, not the camera-key route");
    compareAvailable = false;
    MSG f6{}; f6.hwnd = panelWindow; f6.message = WM_KEYDOWN; f6.wParam = VK_F6;
    verify(dbce::panel::handle(&f6) && cameraKeyCalls == 4, "F6 keeps its own route");
    panelOpen = true; cameraKeyConsumes = true; key.wParam = VK_ADD;
    verify(dbce::panel::handle(&key) && cameraKeyCalls == 4, "panel open: keys go to the panel, never the camera-key route");
    MSG esc{}; esc.hwnd = panelWindow; esc.message = WM_KEYDOWN; esc.wParam = VK_ESCAPE;
    captureWaiting = true;
    verify(dbce::panel::handle(&esc) && panelOpen && !captureWaiting, "Esc during a binding capture cancels it, the panel stays open");
    verify(dbce::panel::handle(&esc) && !panelOpen, "the next Esc closes the panel");
    panelOpen = true;
    panelOpen = false; cameraKeySet = false; cameraKeyConsumes = false;
    DetachInput();
    attachFails = true;
    TraceInput("on", nullptr);
    verify(!AttachInput(panelWindow) && g_trace.attachFailed == 1, "failed hook attachment is witnessed");
    attachFails = false;
    verify(AttachInput(panelWindow), "failed attachment may retry normally");
    g_trace.callbacks = kTraceLimit;
    verify(getMessageHook(HC_ACTION, PM_NOREMOVE, 1) == 71 && g_trace.callbacks == kTraceLimit, "counter saturates without input change");
    const bool openBefore = panelOpen;
    const int queueBefore = g_count;
    for (unsigned i = 0; i < kTraceReports + 2; ++i) TraceInput("snapshot", traceLog);
    const size_t reportLines = traceLines.size();
    TraceInput("snapshot", traceLog);
    verify(traceLines.size() == reportLines && g_traceReports == kTraceReports, "process-wide report cap fixed");
    verify(panelOpen == openBefore && g_count == queueBefore, "report cap does not affect input state");
    panelOpen = false;
    g_wasOpen = false;
    TraceInput("on", nullptr);
    onClient = [] {
        MSG m{}; m.hwnd = panelWindow; m.message = WM_KEYDOWN; m.wParam = VK_F6;
        verify(dbce::panel::getMessageHook(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&m)) == 71 && m.message == WM_NULL,
               "F6 interleaves after render open snapshot, before queue lock");
    };
    DrainInput(1920, 1080);
    verify(panelOpen && g_trace.open && g_trace.opened == 1 && g_trace.closed == 0,
           "locked trace state has no phantom close from stale render snapshot");
    verify(!g_wasOpen, "original render snapshot behavior unchanged by trace fix");
    TraceInput("off", nullptr);
    DetachInput();
    ImGui::DestroyContext();
    std::printf("Input cursor custody: %d/16 checks, %s; fake APIs only\n", checks, ok ? "PASS" : "FAIL");
    std::printf("Message custody: %d checks, %d failures; fabricated MSGs/fake Win32, not provider delivery\n", hookChecks, hookFailures);
    return ok && checks == 16 && hookFailures == 0 ? 0 : 1;
}
