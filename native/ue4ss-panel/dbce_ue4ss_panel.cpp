// F6 panel for UE4SS games with the toolkit's triple renderer (STD-030): the shared panel and overlay (../panel) in a
// small DLL that a UE4SS Lua mod loads with package.loadlib, so nothing depends on the UE4SS C++ ABI. D3D11 and D3D12
// (the overlay draws on the game's own present queue).
//
// Per game, panel.ini beside the DLL ([Mod] Game, Version, LogName, RendererConfig = the renderer's config.ini relative
// to the DLL's folder). Pages (STD-017): Cameras (the STD-022 triple selector and the side-screen angle, written to the
// renderer's config.ini and applied at the next game start; the STD-023 frame rate) and Help. Render and settings only:
// no input to the game, no gameplay or save change. The panel's own [Panel] section (view, UI scale, page) follows the
// renderer's keys in the same config.ini, which the renderer ignores.
//
// Log: %LOCALAPPDATA%\dbce\<LogName>\dbce-<LogName>-panel.log. Unattended checks: %LOCALAPPDATA%\dbce\<LogName>\panel-dev.txt
// ("open Cameras [simple|advanced]" or "close"; dbce_panel.h). First user: ADRIFT (dbce-mods-adrift 0.2.0, where it began).
#include <windows.h>
#include <share.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>
#include "dbce_renderer_config.hpp"
#include "../panel/dbce_panel.h"
#include "../panel/dbce_overlay_d3d11.h"
#include "../panel/dbce_overlay_d3d12.h"
#include "../third_party/imgui/imgui.h"

static char g_game[64] = "UE4SS game", g_version[32] = "", g_logName[32] = "ue4ss";
static const char *const kScreens[3] = {"Off", "Surround", "Separate monitors"};

static FILE *g_log;
static CRITICAL_SECTION g_logCs;
static ULONGLONG g_t0;
static char g_logFile[MAX_PATH], g_config[MAX_PATH], g_dev[MAX_PATH];

static void dlog(const char *fmt, ...)
{
    if (!g_log) return;
    char buf[640];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    SYSTEMTIME t;
    GetLocalTime(&t);
    EnterCriticalSection(&g_logCs);
    fprintf(g_log, "%02d:%02d:%02d.%03d +%7.3f %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, (GetTickCount64() - g_t0) / 1000.0, buf);
    fflush(g_log);
    LeaveCriticalSection(&g_logCs);
}
static void panelLog(const char *line) { dlog("%s", line); }

// ---- the renderer's config -----------------------------------------------------------------------------------
static bool readText(const char *path, std::string *out)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (1 << 20);
    if (ok) {
        out->assign((size_t)size.QuadPart, '\0');
        DWORD got = 0;
        ok = size.QuadPart == 0 || (ReadFile(f, &(*out)[0], (DWORD)size.QuadPart, &got, nullptr) && got == (DWORD)size.QuadPart);
    }
    CloseHandle(f);
    return ok;
}
// Written beside the file, then moved over it, so a reader never sees half a file.
static bool writeText(const char *path, const std::string &text)
{
    char tmp[MAX_PATH];
    sprintf_s(tmp, "%s.dbce-tmp", path);
    HANDLE f = CreateFileA(tmp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    bool ok = WriteFile(f, text.data(), (DWORD)text.size(), &put, nullptr) && put == text.size();
    CloseHandle(f);
    if (ok) ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileA(tmp);
    return ok;
}
static void saveConfig(std::string (*change)(const std::string &, int), int value, const char *what)
{
    std::string text;
    if (!readText(g_config, &text)) { dbce::panel::NoteSaved(false, "config.ini not found"); dlog("panel: could not read %s (%lu)", g_config, GetLastError()); return; }
    if (writeText(g_config, change(text, value))) { dbce::panel::NoteSaved(true); dlog("panel: saved %s", what); }
    else { dbce::panel::NoteSaved(false, "Settings could not be saved"); dlog("panel: could not save %s (%lu)", what, GetLastError()); }
}

// ---- the layout, as the renderer finds it at start (its DetectLayout) --------------------------------------
struct Layout { bool triple = false, separate = false; int w = 0, h = 0; };
static BOOL CALLBACK collectMonitor(HMONITOR m, HDC, LPRECT, LPARAM p)
{
    MONITORINFO mi{sizeof mi};
    if (GetMonitorInfoW(m, &mi)) reinterpret_cast<std::vector<RECT> *>(p)->push_back(mi.rcMonitor);
    return TRUE;
}
static Layout detectLayout()
{
    Layout l;
    const int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    l.w = w; l.h = h;
    if (h > 0 && w >= 4 * h) { l.triple = true; return l; }
    std::vector<RECT> mons;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&mons));
    if (mons.size() != 3) return l;
    std::sort(mons.begin(), mons.end(), [](const RECT &a, const RECT &b) { return a.left < b.left; });
    const LONG w0 = mons[0].right - mons[0].left, h0 = mons[0].bottom - mons[0].top;
    for (size_t i = 0; i < 3; ++i) {
        if (mons[i].right - mons[i].left != w0 || mons[i].bottom - mons[i].top != h0 || mons[i].top != mons[0].top) return l;
        if (i && mons[i].left != mons[i - 1].right) return l;
    }
    l.triple = l.separate = true;
    l.w = mons[2].right - mons[0].left;
    l.h = h0;
    return l;
}

// ---- settings: as read at start (what this session runs with) and as chosen since -------------------------
static Layout g_layout;
static std::string g_tripleAtStart = "auto";   // the renderer's Triple: auto, on or off
static int g_screensAtStart = 2, g_screensChoice = 2;
static int g_sideAtStart = 70, g_side = 70;
static bool g_sideDirty;
static std::string g_rig;                      // "708.4 x 398.5 mm, bezel 8 mm, vertical FOV 58.7°"

static bool threeViews() { return g_tripleAtStart == "on" || (g_tripleAtStart != "off" && g_layout.triple); }

static void readStart()
{
    g_layout = detectLayout();
    std::string text;
    if (!readText(g_config, &text)) { dlog("config: %s not found; defaults shown", g_config); return; }
    std::string t = dbce::renderer::TopValue(text, "Triple");
    for (char &ch : t) ch = (char)tolower((unsigned char)ch);
    g_tripleAtStart = t == "on" || t == "off" ? t : "auto";
    g_screensAtStart = g_screensChoice = dbce::renderer::ScreensOf(text, g_layout.separate);
    std::string side = dbce::renderer::TopValue(text, "SideAngle");
    if (!side.empty()) g_sideAtStart = g_side = (std::max)(0, (std::min)(90, (int)(atof(side.c_str()) + 0.5)));
    char rig[160];
    sprintf_s(rig, "%s x %s mm, bezel %s mm, vertical FOV %s\xC2\xB0", dbce::renderer::TopValue(text, "PanelWidthMm").c_str(),
              dbce::renderer::TopValue(text, "PanelHeightMm").c_str(), dbce::renderer::TopValue(text, "BezelMm").c_str(),
              dbce::renderer::TopValue(text, "VerticalFovDegrees").c_str());
    g_rig = rig;
    dlog("config: Triple = %s, screens %s, side %d deg; layout %s %dx%d; %s", g_tripleAtStart.c_str(), kScreens[g_screensAtStart],
         g_sideAtStart, g_layout.separate ? "three side-by-side monitors" : g_layout.triple ? "Surround span" : "single display",
         g_layout.w, g_layout.h, threeViews() ? "three views this session" : "one view this session");
}

static std::string withScreens(const std::string &text, int choice) { return dbce::renderer::WithScreens(text, choice); }
static std::string withSide(const std::string &text, int side) { return dbce::renderer::SetTopValue(text, "SideAngle", std::to_string(side)); }

static void drawPage(dbce::panel::Page page, bool advanced)
{
    using namespace dbce::panel;
    if (page == PAGE_CAMERAS) {
        BeginGroup("triple");
        if (Choice("Triple screens", kScreens, 3, &g_screensChoice,
                   "Surround and Separate monitors both turn on three views (the centre and two side screens at their angle); "
                   "the mod finds which layout is connected when the game starts. Applies at the next game start."))
            saveConfig(withScreens, g_screensChoice, "triple screens");
        char now[160];
        if (g_tripleAtStart == "off") strcpy_s(now, "Off: one view");
        else if (g_tripleAtStart == "on") strcpy_s(now, "Three views (Triple = On in config.ini)");
        else if (g_layout.separate) sprintf_s(now, "Three views over three separate monitors (%dx%d)", g_layout.w, g_layout.h);
        else if (g_layout.triple) sprintf_s(now, "Three views on the Surround display (%dx%d)", g_layout.w, g_layout.h);
        else strcpy_s(now, "One view: no Surround display or three side-by-side monitors at start");
        Status("This session", now, threeViews() ? Tone::Good : Tone::Normal);
        if (g_screensChoice != g_screensAtStart) Text("Restart the game to apply the new choice.");
        EndGroup();

        BeginGroup("sides");
        if (Slider("Side screens", &g_side, 0, 90, 70, "%d\xC2\xB0",
                   "The angle of your side monitors. 0\xC2\xB0 is one flat wide view: the road continues straight onto the side screens. "
                   "Applies at the next game start."))
            g_sideDirty = true;
        if (g_sideDirty && !ImGui::IsAnyItemActive()) {   // saved once the slider is let go, not on every step of a drag
            saveConfig(withSide, g_side, "side angle");
            g_sideDirty = false;
        }
        if (g_side != g_sideAtStart) Text("Restart the game to apply the new angle.");
        if (advanced) {
            Status("Rig", g_rig.empty() ? "not read" : g_rig.c_str());
            Text("Rig measurements are edited in config.ini (Help shows where).");
        }
        EndGroup();
    } else if (page == PAGE_HELP) {
        BeginGroup("help-files");
        Status("Config", g_config[0] ? g_config : "not found");
        Status("Log", g_logFile[0] ? g_logFile : "not available");
        if (advanced) Text("Menus and the HUD move to the centre screen while the three views are on (DbceTripleUi). The renderer's own lines are in ue4ss\\UE4SS.log.");
        EndGroup();
    }
}

// ---- start (once, from the Lua mod) ----------------------------------------------------------------------------
static bool openLog()
{
    char dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH);
    if (!n || n >= MAX_PATH - 100) return false;
    strcat_s(dir, "\\dbce");
    CreateDirectoryA(dir, nullptr);
    strcat_s(dir, "\\");
    strcat_s(dir, g_logName);
    CreateDirectoryA(dir, nullptr);
    sprintf_s(g_dev, "%s\\panel-dev.txt", dir);
    char name[64];
    sprintf_s(name, "\\dbce-%s-panel.log", g_logName);
    strcat_s(dir, name);
    strcpy_s(g_logFile, dir);
    g_log = _fsopen(dir, "a", _SH_DENYWR);
    return g_log != nullptr;
}

static BOOL CALLBACK startOnce(PINIT_ONCE, PVOID, PVOID *)
{
    InitializeCriticalSection(&g_logCs);
    g_t0 = GetTickCount64();
    // panel.ini beside this DLL names the game and the renderer's config. Pinned: the Lua state never unloads the DLL
    // while the overlay's hooks point into it.
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)&startOnce, &self);
    char path[MAX_PATH] = "", dir[MAX_PATH], ini[MAX_PATH], rel[MAX_PATH] = "";
    GetModuleFileNameA(self, path, MAX_PATH);
    strcpy_s(dir, path);
    if (char *slash = strrchr(dir, '\\')) *slash = 0;
    sprintf_s(ini, "%s\\panel.ini", dir);
    GetPrivateProfileStringA("Mod", "Game", "UE4SS game", g_game, sizeof g_game, ini);
    GetPrivateProfileStringA("Mod", "Version", "", g_version, sizeof g_version, ini);
    GetPrivateProfileStringA("Mod", "LogName", "ue4ss", g_logName, sizeof g_logName, ini);
    for (char *p = g_logName; *p; ++p) if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') *p = '_';   // a folder name
    GetPrivateProfileStringA("Mod", "RendererConfig", "", rel, sizeof rel, ini);
    openLog();
    dlog("%s panel %s: %s", g_game, g_version, path);
    char joined[MAX_PATH * 2];
    sprintf_s(joined, "%s\\%s", dir, rel);
    if (!rel[0] || !GetFullPathNameA(joined, MAX_PATH, g_config, nullptr)) { g_config[0] = 0; dlog("panel.ini: no RendererConfig (%s)", ini); }
    readStart();

    static char modName[128];
    sprintf_s(modName, "%s mod %s", g_game, g_version);
    dbce::panel::Config c;
    c.modName = modName;
    c.iniPath = g_config;
    c.pages[dbce::panel::PAGE_CAMERAS] = true;
    c.drawPage = drawPage;
    c.log = panelLog;
    c.devFile = g_dev;
    c.triplesOn = threeViews;
    dbce::panel::Configure(c);
    dbce::overlay::EnableD3D12();   // the game presents with D3D12
    dbce::overlay::StartD3D11(panelLog);
    return TRUE;
}

// package.loadlib(path, "luaopen_dbce_ue4ss_panel") returns this as a Lua C function; called once, it starts the panel
// and returns no values (the Lua state is not touched).
extern "C" __declspec(dllexport) int luaopen_dbce_ue4ss_panel(void *)
{
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, startOnce, nullptr, nullptr);
    return 0;
}
