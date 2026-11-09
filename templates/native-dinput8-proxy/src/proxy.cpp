// __NAME__ wheel mod (x64): a dinput8.dll proxy on the toolkit's shared proxy core.
//
// Everything a proxy mod needs in every game is in src/vendor (the toolkit, never edited here): start-up, the log, the
// test switches, vtable hooks, the wheel presentation, the force layer and the F6 panel. This file holds only what is
// this game's: its identity, what to start, its F6 pages and its own hooks.
//
// Log: %LOCALAPPDATA%\dbce\__ID__\dbce-__ID__.log. Settings: dbce-__ID__.ini beside the game's exe.
// Unattended runs: put ffb-fence.on in the log folder before the game starts; no force can then reach a device.
#include <winsock2.h>
#include "vendor/dinput-proxy/dbce_proxy_core.hpp"
#include "vendor/dinput-proxy/dbce_ffb_layer.h"
#include "vendor/panel/dbce_panel.h"
#include "vendor/panel/dbce_overlay_d3d11.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "user32.lib")

namespace proxy = dbce::proxy;
static const proxy::Identity kMod = {"__ID__", "__NAME__ mod", "0.1.0"};

// ---- F6 panel: runtime pages only (setup belongs to Wheelkit, STD-053) -------------------------------------------------
static void drawPanelPage(dbce::panel::Page page, bool advanced)
{
    using namespace dbce::panel;
    if (page == PAGE_FFB) {
        dbce::ffb::DrawPanelPage(advanced);
    } else if (page == PAGE_HELP) {
        BeginGroup("help-log");
        Status("Log", proxy::state().logPath[0] ? proxy::state().logPath : "not available");
        EndGroup();
    }
}

// Runs once, after the proxy is pinned, on the thread of the game's first DirectInput call.
static void started()
{
    const proxy::State &s = proxy::state();

    dbce::ffb::Options ffb;
    ffb.iniPath = s.ini;
    ffb.log = proxy::logLine;
    ffb.mute = s.mute;
    dbce::ffb::Init(ffb);

    static char modName[160];
    sprintf_s(modName, "%s %s (%s)", kMod.name, kMod.version, s.exeName);
    dbce::panel::Config panel;
    panel.modName = modName;
    panel.modRepositoryUrl = "https://github.com/d-b-c-e/__REPO__";
    panel.wheelkitRepositoryUrl = "https://github.com/d-b-c-e/dbce-wheelkit";
    panel.iniPath = s.ini;
    panel.pages[dbce::panel::PAGE_FFB] = true;
    panel.drawPage = drawPanelPage;
    panel.ffbOn = [] { return dbce::ffb::On(); };
    panel.stopFfb = [] { dbce::ffb::SetOn(false); };
    panel.log = proxy::logLine;
    dbce::panel::Configure(panel);
    dbce::overlay::StartD3D11(proxy::logLine);

    // TODO(__ID__): start this game's own hooks and workers here (cameras, triple screens, telemetry).
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE h, DWORD version, REFIID riid, LPVOID *out, LPUNKNOWN unk)
{
    proxy::start(kMod, started);
    proxy::CreateOptions options;
    options.presentAsWheel = true;                       // remove if the game already accepts a direct-drive base as a wheel
    options.hookDirectInput = dbce::ffb::HookDirectInput;   // Strength, FFB Off/On and Stop FFB (F8) for the game's own effects
    return proxy::create(h, version, riid, out, unk, options);
}
DBCE_PROXY_FORWARD_EXPORTS(kMod, started)
