// Separate-monitor span for a dinput8.dll proxy whose game sizes one window by itself, on the proxy core
// (dbce_proxy_core.hpp). C++17, header-only, x64. Moved from DiRT 4 (AD-5); DiRT Rally 2.0, GRID (2019) and V-Rally 4
// carry the same code.
//
// The game's sizing questions about the primary monitor (adapter output 0) are answered with the span over the racing
// row, and its window is placed over the span, or, when it asks for a captioned window framing the span, created
// borderless over it. Planar like Surround, not angle-correct. In this header:
//   - the span rules: pure functions of rectangles, tested offline (test_proxy_family.cpp), with GRID (2019)'s Center
//     screen target (the racing row's middle panel);
//   - the span: [Triple] Screens from the mod's ini at start (readScreens), and the monitors, read once on the game's
//     thread at its first question (user32 is not used in DllMain);
//   - logging wrappers for the exe's user32 window and display-mode imports, which change an answer only while the span
//     is on (the wide-character display and monitor ones, EnumDisplaySettingsW and GetMonitorInfoW, from DiRT Rally 2.0
//     and GRID (2019));
//   - the import-table patcher that points the exe's import slots at wrappers (imports::resolve, imports::patch).
// Each wrapper calls the original with the same arguments and returns its result unchanged and only logs (bounded),
// unless the span is on. Originals come from the module's export (GetProcAddress), never from the slot, so a slot the
// loader had not yet written cannot become the "original". Every wrapper takes the thread's last-error value right after
// the real call and puts it back after logging (Astra's review of 3dbbc94: logging calls GetLocalTime and the CRT, and
// callers such as SetWindowLong* users read GetLastError to tell a 0 result from a failure).
//
// At process attach, after dbce::proxy::attach() (kernel32 only):
//   static dbce::proxy::imports::Hook g_imports[] = {
//       DBCE_PROXY_IMPORT("user32.dll", dbce::proxy::span, GetSystemMetrics), ...   // the imports this exe sizes with
//   };
//   dbce::proxy::span::readScreens(dbce::proxy::state().ini);
//   dbce::proxy::imports::resolve(g_imports);
//   dbce::proxy::imports::patch((BYTE *)GetModuleHandleA(nullptr), g_imports, true);
#pragma once
#include "dbce_proxy_core.hpp"
#include <string>
#include <vector>
#if __has_include("../display/dbce_racing_row.hpp")
#include "../display/dbce_racing_row.hpp"            // vendored: src/vendor/display
#else
#include "../../shared/display/dbce_racing_row.hpp"  // the toolkit's own tree
#endif

static_assert(sizeof(void *) == 8, "dbce_proxy_span.hpp patches x64 import tables");

namespace dbce { namespace proxy {

// Restores the real call's last-error value when the wrapper returns.
struct KeepLastError {
    DWORD e;
    KeepLastError() : e(GetLastError()) {}
    ~KeepLastError() { SetLastError(e); }
};

// Bounded logging: the first `first` calls of each wrapper, then every 500th.
inline bool sample(volatile LONG &n, LONG first = 40) { LONG k = InterlockedIncrement(&n); return k <= first || k % 500 == 0; }

// The original of an import-slot wrapper: o_<name>, next to w_<name>.
#define DBCE_PROXY_ORIG(name) inline decltype(&::name) o_##name = nullptr;

namespace span {

// ---- the span rules (STD-015/022) -----------------------------------------------------------------------------------
inline bool SameRect(const RECT &a, const RECT &b) { return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom; }

// The span over the racing row ("Sim Racing"), by the toolkit's selector (shared/display): the primary in the centre
// with an equal display touching each side, no gap or overlap. A display above or below the row, such as the rig's
// dashboard (2026-10-13), is ignored; any other layout is refused. Only that separate-monitor row is a span: a primary
// at least 4:1 wide is Surround, one display the game already fills, as before. `reason` says what was found, for the log.
inline dbce::display::RacingRow RowFromMonitors(const std::vector<RECT> &r, const RECT &primary, std::string *reason)
{
    std::vector<dbce::display::MonitorRect> monitors;
    for (const RECT &m : r)
        monitors.push_back({int(m.left), int(m.top), int(m.right - m.left), int(m.bottom - m.top), SameRect(m, primary)});
    dbce::display::RacingRow row = dbce::display::select_racing_row(monitors, 4.0);
    if (reason) *reason = row.to_string() + " (" + row.reason + ")";
    return row;
}
inline bool SpanFromMonitors(const std::vector<RECT> &r, const RECT &primary, RECT &out, std::string *reason = nullptr)
{
    const dbce::display::RacingRow row = RowFromMonitors(r, primary, reason);
    if (row.kind != dbce::display::RacingRowKind::Separate) return false;
    out = {row.x, row.y, row.x + row.width, row.y + row.height};
    return true;
}
// Center screen (GRID (2019), 2026-10-25, while GRID has no real three-view rendering): one view on the racing row's
// middle panel (STD-038's home display). On Surround that is the primary's middle third, which the span answers then
// describe; a separate row's primary is the center screen too. Keep placement active even though its dimensions stay
// unchanged: the game's saved output can name a side monitor, and a primary-size window still needs to be placed on the
// center.
inline bool CenterFromMonitors(const std::vector<RECT> &r, const RECT &primary, RECT &out, std::string *reason = nullptr)
{
    const dbce::display::RacingRow row = RowFromMonitors(r, primary, reason);
    if (row.kind == dbce::display::RacingRowKind::Separate) { out = primary; return true; }
    if (row.kind != dbce::display::RacingRowKind::Surround) return false;
    const int third = row.width / 3;
    out = {row.x + third, row.y, row.x + 2 * third, row.y + row.height};
    return true;
}

// The game sizes its window from adapter output 0 (the primary monitor): IDXGIOutput::GetDesc and
// EnumDisplaySettingsA(that output, ENUM_CURRENT_SETTINGS). While the span is on, an answer describing exactly the
// primary monitor describes the span instead; any other answer is left alone.
inline bool SpanDesktop(const RECT &primary, const RECT &span, RECT &desktop)
{
    if (!SameRect(desktop, primary)) return false;
    desktop = span;
    return true;
}
inline bool SpanMode(const RECT &primary, const RECT &span, DWORD &width, DWORD &height, POINTL &position)
{
    if (width != DWORD(primary.right - primary.left) || height != DWORD(primary.bottom - primary.top) ||
        position.x != primary.left || position.y != primary.top)
        return false;
    width = DWORD(span.right - span.left);
    height = DWORD(span.bottom - span.top);
    position = {span.left, span.top};
    return true;
}
// DiRT 4 windowed creates a captioned window whose client area is the configured size: CreateWindowExW at the outer
// size AdjustWindowRect gives for that style (7680x1440 -> 7696x1479 for WS_OVERLAPPEDWINDOW, 2026-10-08 log). A
// top-level creation whose outer size is exactly the span framed by its own style is created borderless (WS_POPUP,
// keeping only visibility and clipping bits) at the span's corner and size instead. framedW/framedH: the span's outer
// size for that style, computed by the caller with AdjustWindowRect.
inline bool SpanFramedCreation(const RECT &span, int framedW, int framedH, DWORD &style, int &x, int &y, int &w, int &h)
{
    if (w != framedW || h != framedH) return false;
    if (!(style & WS_CAPTION) && !(style & WS_THICKFRAME)) return false;   // already borderless: SpanPlacement's case
    style = WS_POPUP | (style & (WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS));
    x = span.left; y = span.top; w = span.right - span.left; h = span.bottom - span.top;
    return true;
}
// The game then sizes its window to that size without moving it (SWP_NOMOVE at its creation corner). A sizing call
// to exactly the span size is placed at the span's corner instead; every other call is left alone.
inline bool SpanPlacement(const RECT &span, int &x, int &y, int cx, int cy, UINT &flags)
{
    if (flags & SWP_NOSIZE) return false;
    if (cx != span.right - span.left || cy != span.bottom - span.top) return false;
    x = span.left;
    y = span.top;
    flags &= ~UINT(SWP_NOMOVE);
    return true;
}

// ---- the span (opt-in) ----------------------------------------------------------------------------------------------
// [Triple] Screens = Off | Surround | Separate monitors in the mod's ini, read at start (default Surround: nothing
// changes; Surround already opens 7680x1440 by itself). With Separate monitors and a three-monitor racing row, the
// game's sizing questions about the primary monitor are answered with the span, and its sizing call to that size is
// placed at the span's corner. The monitors are read once, on the game thread at the first question, by g_init: the
// separate-monitor racing row (spanInit, below), or a game's own rule that reads them, logs what it found and calls
// turnOn() with its target (GRID (2019): Center screen or Wide span, from its own [Triple] Screens choices).
inline bool g_wanted;
inline INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
inline bool g_active;
inline HWND g_window;                 // the window created borderless over the span (the game's main window)
inline int g_framedW, g_framedH;      // its outer size with the style the game asked for
inline RECT g_span, g_primary;
inline char g_primaryName[CCHDEVICENAME];
inline wchar_t g_primaryNameW[CCHDEVICENAME];
inline volatile LONG n_spanDesc, n_spanMode, n_spanPlace;

// Reads [Triple] Screens (kernel32 only) and logs it. 0 Off, 1 Surround, 2 Separate monitors (the span is wanted).
inline int readScreens(const char *ini)
{
    char v[64] = "";
    GetPrivateProfileStringA("Triple", "Screens", "Surround", v, sizeof v, ini);
    g_wanted = !_stricmp(v, "Separate monitors") || !_stricmp(v, "Separate");
    log("triple screens: %s (%s)", v, g_wanted ? "span over three separate monitors requested" : "window left alone");
    return g_wanted ? 2 : !_stricmp(v, "Off") ? 0 : 1;
}

struct MonitorList { std::vector<RECT> rects; RECT primary; char primaryName[CCHDEVICENAME]; bool havePrimary; };
inline BOOL CALLBACK collectMonitor(HMONITOR m, HDC, LPRECT rc, LPARAM lp)
{
    auto *l = (MonitorList *)lp;
    l->rects.push_back(*rc);
    MONITORINFOEXA mi{};
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(m, &mi) && (mi.dwFlags & MONITORINFOF_PRIMARY)) {
        l->primary = mi.rcMonitor;
        strcpy_s(l->primaryName, mi.szDevice);
        l->havePrimary = true;
    }
    return TRUE;   // every display, so neither the primary nor a side of the row can be cut off
}
// The span is on: the primary's sizing answers describe `target` from now on.
inline void turnOn(const MonitorList &l, const RECT &target)
{
    g_span = target; g_primary = l.primary;
    strcpy_s(g_primaryName, l.primaryName);
    MultiByteToWideChar(CP_ACP, 0, g_primaryName, -1, g_primaryNameW, CCHDEVICENAME);
    g_active = true;
}
inline BOOL CALLBACK spanInit(PINIT_ONCE, PVOID, PVOID *)
{
    // Callers hold a KeepLastError, so the game's last-error value survives these calls.
    if (!g_wanted) return TRUE;
    MonitorList l{};
    RECT span{};
    std::string row;
    if (!EnumDisplayMonitors(nullptr, nullptr, collectMonitor, (LPARAM)&l) || !l.havePrimary || !SpanFromMonitors(l.rects, l.primary, span, &row)) {
        log("span: separate monitors requested, but no racing row (%s); window left alone",
            row.empty() ? (l.havePrimary ? "the displays could not be read" : "no primary display") : row.c_str());
        return TRUE;
    }
    log("span: racing row %s", row.c_str());
    turnOn(l, span);
    log("span: on, %ldx%ld at (%ld,%ld); primary %s (%ld,%ld)-(%ld,%ld) answered as the span", span.right - span.left,
        span.bottom - span.top, span.left, span.top, g_primaryName, l.primary.left, l.primary.top, l.primary.right, l.primary.bottom);
    return TRUE;
}
// The once-only monitor read and target choice; a game with its own rule sets this at process attach.
inline PINIT_ONCE_FN g_init = spanInit;
inline bool active()
{
    if (!g_wanted) return false;
    InitOnceExecuteOnce(&g_once, g_init, nullptr, nullptr);
    return g_active;
}

// ---- user32 import-slot wrappers ------------------------------------------------------------------------------------
DBCE_PROXY_ORIG(GetSystemMetrics) DBCE_PROXY_ORIG(EnumDisplaySettingsA) DBCE_PROXY_ORIG(SetWindowPos) DBCE_PROXY_ORIG(CreateWindowExA)
DBCE_PROXY_ORIG(AdjustWindowRect) DBCE_PROXY_ORIG(AdjustWindowRectEx) DBCE_PROXY_ORIG(GetClientRect) DBCE_PROXY_ORIG(GetWindowRect)
DBCE_PROXY_ORIG(ShowWindow) DBCE_PROXY_ORIG(SetWindowLongA) DBCE_PROXY_ORIG(SetWindowLongPtrA) DBCE_PROXY_ORIG(GetWindowPlacement)
DBCE_PROXY_ORIG(CreateWindowExW) DBCE_PROXY_ORIG(SetWindowLongPtrW) DBCE_PROXY_ORIG(EnumDisplaySettingsW) DBCE_PROXY_ORIG(GetMonitorInfoW)
DBCE_PROXY_ORIG(MonitorFromWindow) DBCE_PROXY_ORIG(SetWindowPlacement)

inline volatile LONG n_gsm, n_eds, n_swp, n_cwe, n_awr, n_gcr, n_gwr, n_sw, n_swl, n_gwp;

inline int WINAPI w_GetSystemMetrics(int i)
{
    int r = o_GetSystemMetrics(i);
    KeepLastError keep;
    if (sample(n_gsm, 60)) log("GetSystemMetrics(%d) = %d", i, r);
    return r;
}
inline BOOL WINAPI w_EnumDisplaySettingsA(LPCSTR dev, DWORD mode, DEVMODEA *dm)
{
    BOOL r = o_EnumDisplaySettingsA(dev, mode, dm);
    KeepLastError keep;
    if (r && dm && mode == ENUM_CURRENT_SETTINGS && active() && (!dev || !_stricmp(dev, g_primaryName)) &&
        SpanMode(g_primary, g_span, dm->dmPelsWidth, dm->dmPelsHeight, dm->dmPosition) && sample(n_spanMode, 10))
        log("span: EnumDisplaySettingsA(%s, current) answered %lux%lu at (%ld,%ld)", dev ? dev : "null", dm->dmPelsWidth,
            dm->dmPelsHeight, dm->dmPosition.x, dm->dmPosition.y);
    if (sample(n_eds, 60))
        log("EnumDisplaySettingsA(%s, %ld) = %d: %lux%lu @%lu Hz pos (%ld,%ld)", dev ? dev : "null", (long)mode, r,
            r && dm ? dm->dmPelsWidth : 0, r && dm ? dm->dmPelsHeight : 0, r && dm ? dm->dmDisplayFrequency : 0,
            r && dm ? dm->dmPosition.x : 0, r && dm ? dm->dmPosition.y : 0);
    return r;
}
// Wide-character display mode and monitor questions (DiRT Rally 2.0 and GRID (2019) import EnumDisplaySettingsW and ask
// through monitor handles).
inline volatile LONG n_edsw, n_gmi, n_mfw, n_swpl;
inline BOOL WINAPI w_EnumDisplaySettingsW(LPCWSTR dev, DWORD mode, DEVMODEW *dm)
{
    BOOL r = o_EnumDisplaySettingsW(dev, mode, dm);
    KeepLastError keep;
    if (r && dm && mode == ENUM_CURRENT_SETTINGS && active() && (!dev || !_wcsicmp(dev, g_primaryNameW)) &&
        SpanMode(g_primary, g_span, dm->dmPelsWidth, dm->dmPelsHeight, dm->dmPosition) && sample(n_spanMode, 10))
        log("span: EnumDisplaySettingsW(%ls, current) answered %lux%lu at (%ld,%ld)", dev ? dev : L"null", dm->dmPelsWidth,
            dm->dmPelsHeight, dm->dmPosition.x, dm->dmPosition.y);
    if (sample(n_edsw, 60))
        log("EnumDisplaySettingsW(%ls, %ld) = %d: %lux%lu @%lu Hz pos (%ld,%ld)", dev ? dev : L"null", (long)mode, r,
            r && dm ? dm->dmPelsWidth : 0, r && dm ? dm->dmPelsHeight : 0, r && dm ? dm->dmDisplayFrequency : 0,
            r && dm ? dm->dmPosition.x : 0, r && dm ? dm->dmPosition.y : 0);
    return r;
}
// The primary monitor's rectangle is answered as the span; its work area keeps its own top and bottom (taskbar).
inline BOOL WINAPI w_GetMonitorInfoW(HMONITOR m, LPMONITORINFO mi)
{
    BOOL r = o_GetMonitorInfoW(m, mi);
    KeepLastError keep;
    if (r && mi && active()) {
        const RECT work = mi->rcWork;
        if (SpanDesktop(g_primary, g_span, mi->rcMonitor)) {
            mi->rcWork = {g_span.left, work.top, g_span.right, work.bottom};
            if (sample(n_spanDesc, 10)) log("span: GetMonitorInfoW(%p) answered the span", (void *)m);
        }
    }
    if (r && mi && sample(n_gmi, 40))
        log("GetMonitorInfoW(%p) = monitor (%ld,%ld)-(%ld,%ld) work (%ld,%ld)-(%ld,%ld) flags 0x%lx", (void *)m, mi->rcMonitor.left,
            mi->rcMonitor.top, mi->rcMonitor.right, mi->rcMonitor.bottom, mi->rcWork.left, mi->rcWork.top, mi->rcWork.right,
            mi->rcWork.bottom, mi->dwFlags);
    return r;
}
inline HMONITOR WINAPI w_MonitorFromWindow(HWND h, DWORD flags)
{
    HMONITOR r = o_MonitorFromWindow(h, flags);
    KeepLastError keep;
    if (sample(n_mfw, 40)) log("MonitorFromWindow(%p, %lu) = %p", (void *)h, flags, (void *)r);
    return r;
}
inline BOOL WINAPI w_SetWindowPlacement(HWND h, const WINDOWPLACEMENT *p)
{
    BOOL r = o_SetWindowPlacement(h, p);
    KeepLastError keep;
    if (sample(n_swpl, 20) && p) log("SetWindowPlacement(%p, show %u, normal (%ld,%ld)-(%ld,%ld)) = %d", (void *)h, p->showCmd,
                                     p->rcNormalPosition.left, p->rcNormalPosition.top, p->rcNormalPosition.right, p->rcNormalPosition.bottom, r);
    return r;
}
inline BOOL WINAPI w_SetWindowPos(HWND h, HWND after, int x, int y, int cx, int cy, UINT f)
{
    {
        KeepLastError before;   // the span check must not change the caller's last error either
        DWORD pid = 0;
        if (active() && h && h == g_window && !(f & SWP_NOSIZE) && cx == g_framedW && cy == g_framedH) {
            cx = g_span.right - g_span.left; cy = g_span.bottom - g_span.top;   // the framed size of the span, now borderless
        }
        if (active() && h && !GetParent(h) && GetWindowThreadProcessId(h, &pid) && pid == GetCurrentProcessId() &&
            SpanPlacement(g_span, x, y, cx, cy, f) && sample(n_spanPlace, 10))
            log("span: SetWindowPos(%p) placed at (%d,%d) %dx%d", (void *)h, x, y, cx, cy);
    }
    BOOL r = o_SetWindowPos(h, after, x, y, cx, cy, f);
    KeepLastError keep;
    if (sample(n_swp, 60)) log("SetWindowPos(%p, %p, %d, %d, %d, %d, 0x%x) = %d", (void *)h, (void *)after, x, y, cx, cy, f, r);
    return r;
}
inline HWND WINAPI w_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style, int x, int y, int w, int h, HWND parent,
                                     HMENU menu, HINSTANCE inst, LPVOID param)
{
    HWND r = o_CreateWindowExA(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    KeepLastError keep;
    if (sample(n_cwe, 20))
        log("CreateWindowExA(ex 0x%lx, class %s, \"%s\", style 0x%lx, %d, %d, %d, %d, parent %p) = %p", ex,
            IS_INTRESOURCE(cls) ? "#atom" : cls, name ? name : "", style, x, y, w, h, (void *)parent, (void *)r);
    return r;
}
// DiRT 4 creates its window with the wide-character call. While the span is on, a window created at exactly the span
// size is created at the span's corner (SpanPlacement; a windowed 7680x1440 setting asks for that size), and a
// captioned window framing the span is created borderless over it (SpanFramedCreation).
inline HWND WINAPI w_CreateWindowExW(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y, int w, int h, HWND parent,
                                     HMENU menu, HINSTANCE inst, LPVOID param)
{
    bool framedCreation = false;
    {
        KeepLastError before;
        UINT f = SWP_NOMOVE;
        if (!parent && active()) {
            RECT framed = {0, 0, g_span.right - g_span.left, g_span.bottom - g_span.top};
            const DWORD was = style;
            if (o_AdjustWindowRect && o_AdjustWindowRect(&framed, style, FALSE) &&
                SpanFramedCreation(g_span, framed.right - framed.left, framed.bottom - framed.top, style, x, y, w, h)) {
                g_framedW = framed.right - framed.left; g_framedH = framed.bottom - framed.top; framedCreation = true;
                if (sample(n_spanPlace, 10)) log("span: CreateWindowExW framed window (style 0x%lx) created borderless at (%d,%d) %dx%d, style 0x%lx", was, x, y, w, h, style);
            } else if (SpanPlacement(g_span, x, y, w, h, f) && sample(n_spanPlace, 10))
                log("span: CreateWindowExW placed at (%d,%d) %dx%d", x, y, w, h);
        }
    }
    HWND r = o_CreateWindowExW(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    KeepLastError keep;
    if (framedCreation && r) g_window = r;
    if (sample(n_cwe, 20))
        log("CreateWindowExW(ex 0x%lx, class %ls, \"%ls\", style 0x%lx, %d, %d, %d, %d, parent %p) = %p", ex,
            IS_INTRESOURCE(cls) ? L"#atom" : cls, name ? name : L"", style, x, y, w, h, (void *)parent, (void *)r);
    return r;
}
// The span window stays borderless: a later style change that brings back a caption or frame keeps WS_POPUP instead.
inline LONG_PTR spanStyle(HWND h, int i, LONG_PTR v)
{
    if (!h || h != g_window || i != GWL_STYLE || !(v & (WS_CAPTION | WS_THICKFRAME))) return v;
    const LONG_PTR kept = WS_POPUP | (v & (WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_MINIMIZE | WS_MAXIMIZE));
    if (sample(n_spanPlace, 10)) log("span: style 0x%llx on the span window kept borderless (0x%llx)", (unsigned long long)v, (unsigned long long)kept);
    return kept;
}
inline LONG_PTR WINAPI w_SetWindowLongPtrW(HWND h, int i, LONG_PTR v)
{
    { KeepLastError before; v = spanStyle(h, i, v); }
    LONG_PTR r = o_SetWindowLongPtrW(h, i, v);
    KeepLastError keep;
    if (sample(n_swl, 30)) log("SetWindowLongPtrW(%p, %d, 0x%llx) was 0x%llx (last error %lu)", (void *)h, i, (unsigned long long)v,
                               (unsigned long long)r, keep.e);
    return r;
}
inline BOOL WINAPI w_AdjustWindowRect(LPRECT rc, DWORD style, BOOL menu)
{
    RECT in = rc ? *rc : RECT{};
    BOOL r = o_AdjustWindowRect(rc, style, menu);
    KeepLastError keep;
    if (sample(n_awr, 20)) log("AdjustWindowRect((%ld,%ld)-(%ld,%ld), 0x%lx) -> (%ld,%ld)-(%ld,%ld)", in.left, in.top, in.right, in.bottom,
                               style, rc ? rc->left : 0, rc ? rc->top : 0, rc ? rc->right : 0, rc ? rc->bottom : 0);
    return r;
}
inline BOOL WINAPI w_AdjustWindowRectEx(LPRECT rc, DWORD style, BOOL menu, DWORD ex)
{
    RECT in = rc ? *rc : RECT{};
    BOOL r = o_AdjustWindowRectEx(rc, style, menu, ex);
    KeepLastError keep;
    if (sample(n_awr, 20)) log("AdjustWindowRectEx((%ld,%ld)-(%ld,%ld), 0x%lx, ex 0x%lx) -> (%ld,%ld)-(%ld,%ld)", in.left, in.top, in.right,
                               in.bottom, style, ex, rc ? rc->left : 0, rc ? rc->top : 0, rc ? rc->right : 0, rc ? rc->bottom : 0);
    return r;
}
// Called often: logged only when the answer changes (per wrapper), plus the bounded sample.
inline BOOL WINAPI w_GetClientRect(HWND h, LPRECT rc)
{
    thread_local RECT last;   // per thread: two threads asking about different windows do not race (Astra, f0c4e57)
    BOOL r = o_GetClientRect(h, rc);
    KeepLastError keep;
    if (r && rc && memcmp(rc, &last, sizeof last)) { last = *rc; if (sample(n_gcr, 60)) log("GetClientRect(%p) = %ldx%ld", (void *)h, rc->right - rc->left, rc->bottom - rc->top); }
    return r;
}
inline BOOL WINAPI w_GetWindowRect(HWND h, LPRECT rc)
{
    thread_local RECT last;   // per thread: two threads asking about different windows do not race (Astra, f0c4e57)
    BOOL r = o_GetWindowRect(h, rc);
    KeepLastError keep;
    if (r && rc && memcmp(rc, &last, sizeof last)) { last = *rc; if (sample(n_gwr, 60)) log("GetWindowRect(%p) = (%ld,%ld)-(%ld,%ld)", (void *)h, rc->left, rc->top, rc->right, rc->bottom); }
    return r;
}
inline BOOL WINAPI w_ShowWindow(HWND h, int cmd)
{
    BOOL r = o_ShowWindow(h, cmd);
    KeepLastError keep;
    if (sample(n_sw, 20)) log("ShowWindow(%p, %d) = %d", (void *)h, cmd, r);
    return r;
}
inline LONG WINAPI w_SetWindowLongA(HWND h, int i, LONG v)
{
    LONG r = o_SetWindowLongA(h, i, v);
    KeepLastError keep;
    if (sample(n_swl, 30)) log("SetWindowLongA(%p, %d, 0x%lx) was 0x%lx (last error %lu)", (void *)h, i, v, r, keep.e);
    return r;
}
inline LONG_PTR WINAPI w_SetWindowLongPtrA(HWND h, int i, LONG_PTR v)
{
    { KeepLastError before; v = spanStyle(h, i, v); }
    LONG_PTR r = o_SetWindowLongPtrA(h, i, v);
    KeepLastError keep;
    if (sample(n_swl, 30)) log("SetWindowLongPtrA(%p, %d, 0x%llx) was 0x%llx (last error %lu)", (void *)h, i, (unsigned long long)v,
                               (unsigned long long)r, keep.e);
    return r;
}
inline BOOL WINAPI w_GetWindowPlacement(HWND h, WINDOWPLACEMENT *p)
{
    BOOL r = o_GetWindowPlacement(h, p);
    KeepLastError keep;
    if (sample(n_gwp, 20) && p) log("GetWindowPlacement(%p) = %d: show %u normal (%ld,%ld)-(%ld,%ld)", (void *)h, r, p->showCmd,
                                    p->rcNormalPosition.left, p->rcNormalPosition.top, p->rcNormalPosition.right, p->rcNormalPosition.bottom);
    return r;
}

} // namespace span

// ---- the import table -----------------------------------------------------------------------------------------------
namespace imports {

struct Hook { const char *dll, *name; void *hook; void **orig; };
// One entry: the DLL, and the namespace holding w_<name> and o_<name>, such as dbce::proxy::span.
#define DBCE_PROXY_IMPORT(dll, ns, name) {dll, #name, (void *)&ns::w_##name, (void **)&ns::o_##name}

// Looks each original up in a module the exe already imports (loaded, so nothing is loaded here; kernel32 only).
template <size_t N> inline void resolve(Hook (&hooks)[N])
{
    for (auto &h : hooks) {
        HMODULE m = GetModuleHandleA(h.dll);
        *h.orig = m ? (void *)GetProcAddress(m, h.name) : nullptr;
        if (!*h.orig) log("import %s!%s: export not found, left unwrapped", h.dll, h.name);
    }
}

// Points the exe's import slots for the functions above at the wrappers. Returns how many slots were (re)written; a slot
// already holding its wrapper is left alone. Only by-name imports of the listed DLLs are touched, and only those whose
// original was resolved.
template <size_t N> inline int patch(BYTE *base, Hook (&hooks)[N], bool logEach)
{
    auto dos = (IMAGE_DOS_HEADER *)base;
    auto nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return -1;
    auto &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return -1;
    int written = 0;
    for (auto d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); d->Name; ++d) {
        const char *dll = (const char *)(base + d->Name);
        if (!d->OriginalFirstThunk) continue;   // no name table: cannot identify slots safely
        auto names = (IMAGE_THUNK_DATA64 *)(base + d->OriginalFirstThunk);
        auto slots = (IMAGE_THUNK_DATA64 *)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            const char *fn = (const char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name;
            for (auto &h : hooks) {
                if (_stricmp(dll, h.dll) || strcmp(fn, h.name) || !*h.orig) continue;
                if ((void *)slots->u1.Function == h.hook) break;
                DWORD old;
                if (VirtualProtect(&slots->u1.Function, sizeof(ULONGLONG), PAGE_READWRITE, &old)) {
                    bool wasOriginal = (void *)slots->u1.Function == *h.orig;
                    slots->u1.Function = (ULONGLONG)h.hook;
                    VirtualProtect(&slots->u1.Function, sizeof(ULONGLONG), old, &old);
                    ++written;
                    if (logEach && !wasOriginal) log("import %s!%s: slot did not hold the export before patching", dll, fn);
                }
                break;
            }
        }
    }
    return written;
}

} // namespace imports
}} // namespace dbce::proxy
