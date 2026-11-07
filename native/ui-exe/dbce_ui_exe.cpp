// dbce-ui.exe: the F6 panel core (native/panel, UI-1) in a normal window, for setup while the game is closed (UI-3;
// docs/architecture/2026-11/04-cross-cutting.md F.3 --setup, 05-ux-simplification.md 4.2, in dbce-project-mgmt).
//
//   dbce-ui.exe --setup <dbce-mod.json> [--config <ini>]
//
// It draws the descriptor's pages with the same core and widgets as F6 in the game (D3D11 + Dear ImGui). Each value
// lives in the mod's ini, section [Settings], under its row id (ffb.strength=50); the ini is the descriptor's "config"
// (relative to the descriptor file) or --config. Every change writes only its own key at once (STD-044, through
// native/settings saveKeys: other lines, comments, encoding and line endings are kept). There is no Apply step.
// Cameras rows and status lines need the running game and are left out; bindings show their saved input and can be
// cleared. Esc, Close or the window's X ends it.
#include "../panel/dbce_panel.h"
#include "../panel/dbce_panel_descriptor.h"
#include "../panel/dbce_panel_input.h"
#include "../panel/dbce_overlay_internal.h"
#include "../settings/dbce_ini_single_key_win.hpp"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_dx11.h"
#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

namespace panel = dbce::panel;
using panel::Row;
using panel::RowKind;

static const char kSection[] = "Settings";
static const wchar_t kClass[] = L"dbce-ui-setup";

static panel::Descriptor g_desc;        // the rows the window draws (Cameras left out)
static std::string g_ini;               // ANSI path for native/settings
static std::vector<std::string> g_value; // per row: what the ini holds (as written), "" when absent

static ID3D11Device *g_device;
static ID3D11DeviceContext *g_context;
static IDXGISwapChain *g_chain;
static ID3D11RenderTargetView *g_rtv;
static bool g_resize;

static std::wstring wide(const std::string &s)
{
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
// native/settings opens the file with the ANSI API: refuse a path that does not survive the round trip.
static bool ansiPath(const std::wstring &w, std::string &out)
{
    BOOL lost = FALSE;
    const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, nullptr, 0, nullptr, &lost);
    if (n <= 1 || lost) return false;
    out.assign((size_t)n - 1, '\0');
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, &out[0], n, nullptr, &lost);
    return !lost;
}
static void fail(const std::wstring &text)
{
    MessageBoxW(nullptr, text.c_str(), L"Wheel mod setup", MB_OK | MB_ICONWARNING);
}

static bool lowerEq(const std::string &a, const std::string &b)
{
    return dbce::settings::lower(a) == dbce::settings::lower(b);
}

// ---- values: ini text <-> panel ----------------------------------------------------------------------------------
static std::string numberText(const Row &r, double v)
{
    char b[64];
    sprintf_s(b, "%.*f", r.kind == RowKind::Number ? r.decimals : 0, v);
    return b;
}
static std::string defaultText(const Row &r)
{
    switch (r.kind) {
    case RowKind::Toggle: return r.defaultValue != 0 ? "1" : "0";
    case RowKind::Choice: return r.choices.empty() ? std::string() : r.choices[(size_t)r.defaultValue];
    case RowKind::Percent: case RowKind::Slider: case RowKind::Number: return numberText(r, r.defaultValue);
    default: return std::string();
    }
}
// Push one row's value to the panel from its ini text; an absent or unreadable value shows the default.
static void push(const Row &r, const std::string &text)
{
    const char *id = r.id.c_str();
    switch (r.kind) {
    case RowKind::Toggle: {
        bool v = r.defaultValue != 0;
        if (text == "1" || lowerEq(text, "true") || lowerEq(text, "on")) v = true;
        if (text == "0" || lowerEq(text, "false") || lowerEq(text, "off")) v = false;
        panel::SetToggle(id, v);
        break;
    }
    case RowKind::Choice: {
        int v = (int)r.defaultValue;
        for (size_t i = 0; i < r.choices.size(); ++i) if (lowerEq(text, r.choices[i])) v = (int)i;
        panel::SetChoice(id, v);
        break;
    }
    case RowKind::Percent: case RowKind::Slider: case RowKind::Number: {
        char *end = nullptr;
        const double v = strtod(text.c_str(), &end);
        panel::SetNumber(id, !text.empty() && end && !*end ? v : r.defaultValue);
        break;
    }
    case RowKind::Binding:
        panel::SetText(id, text.empty() ? (r.text.empty() ? "Not set" : r.text.c_str()) : text.c_str());
        break;
    default: break;
    }
}
static void loadValues()
{
    g_value.assign(g_desc.rows.size(), std::string());
    std::vector<char> buf(4096);
    for (size_t i = 0; i < g_desc.rows.size(); ++i) {
        const Row &r = g_desc.rows[i];
        if (!panel::RowIsControl(r.kind) || r.kind == RowKind::Action) continue;
        const DWORD n = GetPrivateProfileStringA(kSection, r.id.c_str(), "", buf.data(), (DWORD)buf.size(), g_ini.c_str());
        g_value[i] = std::string(buf.data(), n);
        push(r, g_value[i]);
    }
}
static int rowIndex(const std::string &id)
{
    for (size_t i = 0; i < g_desc.rows.size(); ++i) if (g_desc.rows[i].id == id) return (int)i;
    return -1;
}

// ---- changes and actions from the panel ----------------------------------------------------------------------------
static void stage(dbce::settings::Changes &changes, int i, const std::string &text)
{
    for (auto &c : changes) if (c.first == g_desc.rows[(size_t)i].id) { c.second = text; return; }
    changes.push_back({g_desc.rows[(size_t)i].id, text});
}
// "Restore defaults" (<page>.restore) and "Restore all defaults" (help.restore-all): rows back to their defaults,
// writing only the keys whose value changes. A page restore also clears that page's bindings; restore-all keeps them.
static void restore(dbce::settings::Changes &changes, bool all, panel::StdPage page)
{
    for (size_t i = 0; i < g_desc.rows.size(); ++i) {
        const Row &r = g_desc.rows[i];
        if (!panel::RowIsControl(r.kind) || r.kind == RowKind::Action || (!all && r.page != page)) continue;
        if (r.kind == RowKind::Binding && all) continue;
        const std::string text = defaultText(r);
        if (g_value[i].empty() && r.kind != RowKind::Binding) { push(r, text); continue; }   // absent: already default
        if (g_value[i] == text) continue;
        stage(changes, (int)i, text);
        push(r, text);
    }
}
static void pollPanel()
{
    dbce::settings::Changes changes;
    panel::PanelEvent e;
    while (panel::PollEvent(e)) {
        const int i = rowIndex(e.id);
        if (i < 0) continue;
        const Row &r = g_desc.rows[(size_t)i];
        switch (e.type) {
        case panel::PanelEvent::Type::Changed:
            if (r.kind == RowKind::Toggle) stage(changes, i, e.toggle ? "1" : "0");
            else if (r.kind == RowKind::Choice && e.choice >= 0 && e.choice < (int)r.choices.size()) stage(changes, i, r.choices[(size_t)e.choice]);
            else if (r.kind == RowKind::Percent || r.kind == RowKind::Slider || r.kind == RowKind::Number) stage(changes, i, numberText(r, e.number));
            break;
        case panel::PanelEvent::Type::Clear:
            if (!g_value[(size_t)i].empty()) stage(changes, i, "");
            push(r, "");
            break;
        case panel::PanelEvent::Type::Capture:   // needs the wheel read in the game (UI-3 follow-up)
            push(r, g_value[(size_t)i]);
            panel::ShowToast("Set this in the game:", "press F6 while it runs.");
            break;
        case panel::PanelEvent::Type::Action: {
            const std::string &id = r.id;
            const size_t dot = id.find('.');
            if (id == "help.restore-all") restore(changes, true, r.page);
            else if (dot != std::string::npos && id.compare(dot, std::string::npos, ".restore") == 0) restore(changes, false, r.page);
            else panel::ShowToast("Start the game to use this.");
            break;
        }
        }
    }
    if (changes.empty()) return;
    const bool ok = dbce::settings::saveKeys(g_ini.c_str(), kSection, changes);
    if (ok) for (const auto &c : changes) g_value[(size_t)rowIndex(c.first)] = c.second;
    panel::NoteSaved(ok, ok ? nullptr : "Settings could not be saved");
}

// ---- window and D3D11 ----------------------------------------------------------------------------------------------
static void releaseTarget()
{
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}
static bool makeTarget()
{
    ID3D11Texture2D *back = nullptr;
    if (FAILED(g_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back)) || !back) return false;
    const bool ok = SUCCEEDED(g_device->CreateRenderTargetView(back, nullptr, &g_rtv));
    back->Release();
    return ok;
}
static bool startD3D(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    // Hardware first; WARP where there is no usable GPU (a VM or remote session).
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_chain,
                                                    &g_device, nullptr, &g_context)))
            return makeTarget();
    }
    return false;
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE: g_resize = true; return 0;
    case WM_SETCURSOR:   // the panel draws its own cursor in the client area, as in the game
        if (LOWORD(lp) == HTCLIENT) { SetCursor(nullptr); return TRUE; }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring descPath, iniPath;
    for (int i = 1; argv && i + 1 < argc; ++i) {
        if (!_wcsicmp(argv[i], L"--setup")) descPath = argv[++i];
        else if (!_wcsicmp(argv[i], L"--config")) iniPath = argv[++i];
    }
    if (argv) LocalFree(argv);
    if (descPath.empty()) {
        fail(L"Start setup from the mod's folder:\n\ndbce-ui.exe --setup dbce-mod.json");
        return 2;
    }
    wchar_t full[MAX_PATH];
    if (!GetFullPathNameW(descPath.c_str(), MAX_PATH, full, nullptr)) { fail(L"The mod's settings file was not found."); return 2; }
    descPath = full;
    panel::Descriptor all;
    std::string error;
    if (!panel::LoadDescriptor(descPath.c_str(), all, error)) {
        fail(L"The mod's settings file could not be read.\n\n" + wide(error));
        return 2;
    }
    if (iniPath.empty() && !all.config.empty()) {
        iniPath = descPath.substr(0, descPath.find_last_of(L"\\/") + 1) + wide(all.config);
    }
    if (iniPath.empty()) { fail(L"This mod has no settings to set up here."); return 2; }
    if (!GetFullPathNameW(iniPath.c_str(), MAX_PATH, full, nullptr) || !ansiPath(full, g_ini)) {
        fail(L"The mod's folder name has characters this setup cannot use.");
        return 2;
    }
    // The Cameras page needs the running game; everything else is drawn as in F6.
    g_desc = all;
    g_desc.rows.clear();
    for (const Row &r : all.rows) if (r.page != panel::StdPage::Cameras) g_desc.rows.push_back(r);

    const std::string title = g_desc.product + " setup";
    const std::string modName = g_desc.version.empty() ? g_desc.product : g_desc.product + " " + g_desc.version;
    panel::Config c;
    c.title = title.c_str();
    c.modName = modName.c_str();
    c.iniPath = nullptr;   // the panel's own [Panel] view and page are the game's; setup writes only setting keys
    c.descriptor = &g_desc;
    c.gameFrames = false;  // no frame rate to show with the game closed
    c.modRepositoryUrl = g_desc.modPage.empty() ? nullptr : g_desc.modPage.c_str();
    panel::Configure(c);
    for (const Row &r : g_desc.rows) if (r.kind == RowKind::Status) panel::SetRowVisible(r.id.c_str(), false);
    loadValues();

    WNDCLASSEXW wc = {sizeof wc};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    const UINT dpi = GetDpiForSystem();
    RECT rc = {0, 0, MulDiv(1008, (int)dpi, 96), MulDiv(916, (int)dpi, 96)};
    const DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRectExForDpi(&rc, style, FALSE, 0, dpi);
    HWND hwnd = CreateWindowExW(0, kClass, wide(title).c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
                                rc.bottom - rc.top, nullptr, nullptr, inst, nullptr);
    if (!hwnd || !startD3D(hwnd)) { fail(L"Setup could not open its window (no Direct3D 11)."); return 3; }
    dbce::overlay::detail::ensureImGui();
    if (!ImGui_ImplDX11_Init(g_device, g_context)) { fail(L"Setup could not start its drawing."); return 3; }
    ShowWindow(hwnd, show);
    panel::AttachInput(hwnd);
    panel::SetOpen(true);

    bool running = true;
    while (running) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) running = false;
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        if (!running) break;
        if (!panel::IsOpen()) { DestroyWindow(hwnd); continue; }   // Esc or Close
        pollPanel();
        if (g_resize) {
            g_resize = false;
            releaseTarget();
            g_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            makeTarget();
        }
        RECT cr;
        GetClientRect(hwnd, &cr);
        const int w = cr.right - cr.left, h = cr.bottom - cr.top;
        if (w <= 0 || h <= 0 || !g_rtv) { Sleep(50); continue; }   // minimized
        if (panel::Prepare(w, h)) { panel::BuildFonts(); ImGui_ImplDX11_InvalidateDeviceObjects(); }
        ImGui_ImplDX11_NewFrame();
        dbce::overlay::detail::drawFrame(w, h);
        const float clear[4] = {0.07f, 0.075f, 0.085f, 1.0f};
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (g_chain->Present(1, 0) == DXGI_STATUS_OCCLUDED) Sleep(16);
    }
    pollPanel();   // a change made on the last frame
    panel::DetachInput();
    ImGui_ImplDX11_Shutdown();
    releaseTarget();
    if (g_chain) g_chain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    return 0;
}
