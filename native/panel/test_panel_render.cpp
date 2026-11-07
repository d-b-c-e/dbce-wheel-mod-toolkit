// Offline render check for the shared F6 panel: draws each page with sample content on a WARP D3D11 device at several
// window sizes and saves PNGs (build\panel-<page>-<w>x<h>.png) for visual review against UX-1. No game, no device.
// Also checks that the header commands (Close, Stop FFB) and the panel lie inside the window and, on a three-screen
// span, on the centre screen. UI-1: every page of the GRID and art of rally sample descriptors (samples\) at 1280x720,
// 1920x1080 and 7680x1440, More options closed and open and the Developer tab, with row geometry checks
// (build\desc-<game>-<page>[-more]-<w>x<h>.png), toasts, and values and events through the mouse.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include "dbce_panel.h"
#include "../settings/dbce_ini_single_key_win.hpp"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/imgui_internal.h"
#include "../third_party/imgui/backends/imgui_impl_dx11.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static bool g_ffb = true;
static int g_strength = 50, g_triple = 2;
static bool g_tel = true;
static bool ffbOn() { return g_ffb; }
static int g_selfTestTicks;
static void selfTestTick() { ++g_selfTestTicks; }
static void stopFfb() { g_ffb = false; }

static void drawPage(dbce::panel::Page page, bool advanced)
{
    using namespace dbce::panel;
    if (page == PAGE_FFB) {
        BeginGroup("ffb-main");
        OffOn("FFB", &g_ffb, "Force feedback from the game, through the mod. F8 stops it at any time.");
        Percent("Strength", &g_strength, 0, 100, 50, "50% matches art of rally at 50% (STD-021).");
        Status("Status", g_ffb ? "Active: the game is sending forces" : "Off", g_ffb ? Tone::Good : Tone::Normal);
        EndGroup();
        if (advanced) {
            BeginGroup("ffb-adv");
            Heading("Effects the game uses");
            Status("Constant force", "on");
            Status("Damper", "on");
            Status("Rumble (square)", "silenced by the game's gain");
            EndGroup();
        }
    } else if (page == PAGE_CAMERAS) {
        static const char *const kScreens[3] = {"Off", "Surround", "Separate monitors"};
        BeginGroup("cam-triple");
        Choice("Triple screens", kScreens, 3, &g_triple, "Separate monitors: one wide view over your three screens. Applies at the next game start.");
        Status("Layout now", "Three side-by-side monitors (7680x1440)", Tone::Good);
        EndGroup();
    } else if (page == PAGE_TELEMETRY) {
        BeginGroup("tel");
        OffOn("Telemetry", &g_tel, "Forza Horizon data to 127.0.0.1:20777 for dashboards such as SimHub.");
        Status("Sending", "Speed, gear, rpm. Pedals, steering and motion are not available in this game.");
        EndGroup();
    } else if (page == PAGE_HELP) {
        BeginGroup("help-log");
        Status("Log", "%LOCALAPPDATA%\\dbce\\example\\example.log");
        if (Command("Create support file")) {}
        EndGroup();
    }
}

static bool savePng(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *tex, int w, int h, const wchar_t *path)
{
    D3D11_TEXTURE2D_DESC d;
    tex->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    ID3D11Texture2D *st = nullptr;
    if (FAILED(dev->CreateTexture2D(&d, nullptr, &st))) return false;
    ctx->CopyResource(st, tex);
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) { st->Release(); return false; }
    IWICImagingFactory *f = nullptr; IWICBitmapEncoder *enc = nullptr; IWICBitmapFrameEncode *fr = nullptr; IWICStream *s = nullptr;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) &&
              SUCCEEDED(f->CreateStream(&s)) && SUCCEEDED(s->InitializeFromFilename(path, GENERIC_WRITE)) &&
              SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && SUCCEEDED(enc->Initialize(s, WICBitmapEncoderNoCache)) &&
              SUCCEEDED(enc->CreateNewFrame(&fr, nullptr)) && SUCCEEDED(fr->Initialize(nullptr)) && SUCCEEDED(fr->SetSize(w, h));
    // The PNG encoder takes BGRA: swap red and blue from the RGBA render target.
    std::string px((size_t)m.RowPitch * h, '\0');
    memcpy(&px[0], m.pData, px.size());
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) { char *p = &px[(size_t)y * m.RowPitch + (size_t)x * 4]; char r = p[0]; p[0] = p[2]; p[2] = r; }
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    ok = ok && SUCCEEDED(fr->SetPixelFormat(&fmt)) && fmt == GUID_WICPixelFormat32bppBGRA &&
         SUCCEEDED(fr->WritePixels(h, m.RowPitch, (UINT)px.size(), (BYTE *)&px[0])) && SUCCEEDED(fr->Commit()) && SUCCEEDED(enc->Commit());
    if (fr) fr->Release(); if (enc) enc->Release(); if (s) s->Release(); if (f) f->Release();
    ctx->Unmap(st, 0); st->Release();
    return ok;
}

// The box of pixels that differ from the bottom-left one (the cleared backdrop); false when there are none.
static bool inkBox(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *tex, int w, int h, RECT *box)
{
    D3D11_TEXTURE2D_DESC d;
    tex->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    ID3D11Texture2D *st = nullptr;
    if (FAILED(dev->CreateTexture2D(&d, nullptr, &st))) return false;
    ctx->CopyResource(st, tex);
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) { st->Release(); return false; }
    const UINT32 *row0 = (const UINT32 *)m.pData;
    const UINT32 bg = *(const UINT32 *)((const char *)row0 + (size_t)(h - 1) * m.RowPitch);
    *box = {w, h, -1, -1};
    for (int y = 0; y < h; ++y) {
        const UINT32 *row = (const UINT32 *)((const char *)row0 + (size_t)y * m.RowPitch);
        for (int x = 0; x < w; ++x) if (row[x] != bg) {
            box->left = (std::min)(box->left, (LONG)x); box->right = (std::max)(box->right, (LONG)x);
            box->top = (std::min)(box->top, (LONG)y); box->bottom = (std::max)(box->bottom, (LONG)y);
        }
    }
    ctx->Unmap(st, 0); st->Release();
    return box->right >= 0;
}

// ---- UI-1: pages drawn from the GRID and art of rally sample descriptors ----------------------------------------
static void renderFrames(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *rtv, int w, int h, int frames)
{
    for (int frame = 0; frame < frames; ++frame) {
        if (dbce::panel::Prepare(w, h)) { dbce::panel::BuildFonts(); ImGui_ImplDX11_InvalidateDeviceObjects(); }
        ImGui_ImplDX11_NewFrame();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)w, (float)h);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        dbce::panel::Draw(w, h);
        ImGui::Render();
        const float bg[4] = {0.30f, 0.36f, 0.42f, 1.0f};
        ctx->ClearRenderTargetView(rtv, bg);
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}

static void click(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *rtv, int w, int h, float x, float y)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    renderFrames(ctx, rtv, w, h, 1);
    io.AddMouseButtonEvent(0, true);
    renderFrames(ctx, rtv, w, h, 1);
    io.AddMouseButtonEvent(0, false);
    renderFrames(ctx, rtv, w, h, 2);
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    renderFrames(ctx, rtv, w, h, 1);
}

static ImGuiWindow *bodyOf(ImGuiWindow *panel)
{
    for (ImGuiWindow *win : ImGui::GetCurrentContext()->Windows)
        if (win->ParentWindow == panel && (win->Flags & ImGuiWindowFlags_ChildWindow) && strstr(win->Name, "##body") && win->Active) return win;
    return nullptr;
}

static bool waiting(const dbce::panel::PanelEvent &e, dbce::panel::PanelEvent::Type type, const char *id)
{
    return e.type == type && e.id == id;
}

static int descriptorChecks(ID3D11Device *dev, ID3D11DeviceContext *ctx, const char *outDir, const char *samples, const char *ini)
{
    using namespace dbce::panel;
    struct Sample { const char *file, *name; bool telemetry, developerRows; };
    const Sample kSamples[] = {{"grid-2019.settings.json", "grid", true, false}, {"art-of-rally.settings.json", "art", true, true}};
    const int sizes[][2] = {{1280, 720}, {1920, 1080}, {7680, 1440}};
    int shots = 0;
    for (const Sample &sm : kSamples) {
        Descriptor d;
        std::string error;
        wchar_t path[MAX_PATH];
        swprintf_s(path, L"%hs\\%hs", samples, sm.file);
        CHECK(LoadDescriptor(path, d, error));
        if (!error.empty()) { fprintf(stderr, "%s: %s\n", sm.name, error.c_str()); continue; }
        for (auto &sz : sizes) {
            const int w = sz[0], h = sz[1];
            const bool span = w >= h * 2.5f;
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
            ID3D11Texture2D *tex = nullptr; ID3D11RenderTargetView *rtv = nullptr;
            dev->CreateTexture2D(&td, nullptr, &tex);
            dev->CreateRenderTargetView(tex, nullptr, &rtv);
            for (int developer = 0; developer < 2; ++developer) {
                DeleteFileA(ini);
                Config c;
                c.iniPath = ini;
                c.descriptor = &d;
                c.ffbOn = ffbOn;
                c.stopFfb = stopFfb;
                Configure(c);
                ClearStatusLine();
                SetDeveloper(developer != 0);
                // The fixed page set: empty pages hidden, Help and Display always, Developer only behind the switch
                // and only with developer rows (GRID has none).
                CHECK(DescriptorPageShown(StdPage::Controls) && DescriptorPageShown(StdPage::Ffb) && DescriptorPageShown(StdPage::Display));
                CHECK(DescriptorPageShown(StdPage::Cameras) && DescriptorPageShown(StdPage::Help));
                CHECK(DescriptorPageShown(StdPage::Telemetry) == sm.telemetry);
                CHECK(DescriptorPageShown(StdPage::Developer) == (developer != 0 && sm.developerRows));
                for (int pi = 0; pi < (int)StdPage::Count; ++pi) {
                    const StdPage page = (StdPage)pi;
                    if (!DescriptorPageShown(page)) continue;
                    if ((page == StdPage::Developer) != (developer != 0)) continue;   // the other pages once, switch off
                    for (int more = 0; more < (page == StdPage::Developer ? 1 : 2); ++more) {
                        SetOpen(true);
                        ShowDescriptorPage(page, more != 0);
                        renderFrames(ctx, rtv, w, h, 3);
                        CHECK(DescriptorPage() == page);
                        ImGuiWindow *win = ImGui::FindWindowByName("##dbce-panel");
                        CHECK(win != nullptr && win->Active);
                        if (!win) continue;
                        const float px0 = win->Pos.x, px1 = win->Pos.x + win->Size.x;
                        CHECK(px0 >= 0 && win->Pos.y >= 0 && px1 <= w && win->Pos.y + win->Size.y <= h);
                        if (span) CHECK(px0 >= w / 3.0f && px1 <= 2.0f * w / 3.0f);
                        ImGuiWindow *body = bodyOf(win);
                        CHECK(body != nullptr);
                        if (!body) continue;
                        // Every page fits without scrolling until More options opens (05-ux-simplification.md 1.0).
                        if (!more && page != StdPage::Developer) CHECK(body->ScrollMax.y <= 0.5f);
                        // Every row of this page drawn, inside the panel, in order, none overlapping.
                        std::vector<std::pair<float, float>> spans;
                        auto expect = [&](const char *id, bool mustBeVisible) {
                            float r[4];
                            const bool drawn = RowRect(id, r);
                            if (!drawn) { fprintf(stderr, "FAIL %s %s %dx%d: row %s not drawn\n", sm.name, StdPageId(page), w, h, id); ++failures; return; }
                            const bool inside = r[0] >= px0 && r[2] <= px1 + 0.5f && r[1] >= win->Pos.y && r[3] >= r[1];
                            const bool shown = r[1] >= body->InnerRect.Min.y - 0.5f && r[3] <= body->InnerRect.Max.y + 0.5f;
                            if (!inside || (mustBeVisible && !shown)) {
                                fprintf(stderr, "FAIL %s %s %dx%d: row %s at %.0f,%.0f-%.0f,%.0f outside the panel or body\n",
                                        sm.name, StdPageId(page), w, h, id, r[0], r[1], r[2], r[3]);
                                ++failures;
                            }
                            spans.push_back({r[1], r[3]});
                        };
                        int drawnRows = 0;
                        for (const Row &row : d.rows) {
                            const bool here = page == StdPage::Developer ? row.level == RowLevel::Developer
                                : row.page == page && (row.level == RowLevel::Simple || (more && row.level == RowLevel::Advanced));
                            if (here) { expect(row.id.c_str(), row.level == RowLevel::Simple); ++drawnRows; }
                            else if (row.level == RowLevel::Developer || row.page != page) {
                                float r[4];
                                CHECK(!RowRect(row.id.c_str(), r));   // not on this page
                            }
                        }
                        if (page == StdPage::Display) { expect("panel.display-status", true); if (more) expect("panel.show-frame-rate", false); }
                        if (page == StdPage::Help) { expect("panel.version", true); expect("panel.keys", true); expect("panel.mod-page", true); if (more) expect("panel.text-size", false); }
                        bool hasMore = page == StdPage::Display || page == StdPage::Help;
                        for (const Row &row : d.rows) hasMore = hasMore || (row.page == page && row.level == RowLevel::Advanced);
                        if (page != StdPage::Developer && hasMore) expect("panel.more", true);
                        std::sort(spans.begin(), spans.end());
                        for (size_t i = 1; i < spans.size(); ++i) CHECK(spans[i].first >= spans[i - 1].second - 0.5f);
                        CHECK(drawnRows > 0 || page == StdPage::Display || page == StdPage::Help);
                        wchar_t png[MAX_PATH];
                        swprintf_s(png, L"%hs\\desc-%hs-%hs%hs-%dx%d.png", outDir, sm.name, StdPageId(page), more ? "-more" : "", w, h);
                        CHECK(savePng(dev, ctx, tex, w, h, png));
                        ++shots;
                    }
                }
            }
            // Toasts (camera name, the F7 view) at the bottom of the centre screen, panel closed, never input.
            SetOpen(false);
            ShowToast("Bonnet camera \xC2\xB7 Game view", "Numpad + - zoom \xC2\xB7 F7 Realistic view");
            CHECK(ToastActive() && DrawWanted());
            renderFrames(ctx, rtv, w, h, 3);
            ImGuiWindow *toast = ImGui::FindWindowByName("##dbce-toast");
            CHECK(toast != nullptr && toast->Active);
            if (toast) {
                const float x0 = span ? w / 3.0f : 0.0f, x1 = span ? 2.0f * w / 3.0f : (float)w;
                CHECK(toast->Pos.x >= x0 && toast->Pos.x + toast->Size.x <= x1);
                CHECK(toast->Pos.y >= h * 0.6f && toast->Pos.y + toast->Size.y <= h);
                CHECK(std::fabs(toast->Pos.x + toast->Size.x / 2 - (x0 + x1) / 2) <= 2.0f);
                CHECK((toast->Flags & ImGuiWindowFlags_NoInputs) == ImGuiWindowFlags_NoInputs);
                CHECK(!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard);
            }
            wchar_t png[MAX_PATH];
            swprintf_s(png, L"%hs\\desc-%hs-toast-%dx%d.png", outDir, sm.name, w, h);
            CHECK(savePng(dev, ctx, tex, w, h, png));
            ++shots;
            ShowToast(nullptr);
            CHECK(!ToastActive() && !DrawWanted());
            rtv->Release(); tex->Release();
        }

        // Values in, events out (art only: it uses every kind), at 1920x1080 with the mouse.
        if (strcmp(sm.name, "art")) continue;
        const int w = 1920, h = 1080;
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
        ID3D11Texture2D *tex = nullptr; ID3D11RenderTargetView *rtv = nullptr;
        dev->CreateTexture2D(&td, nullptr, &tex);
        dev->CreateRenderTargetView(tex, nullptr, &rtv);
        DeleteFileA(ini);
        Config c;
        c.iniPath = ini;
        c.descriptor = &d;
        c.selfTestTick = selfTestTick;
        Configure(c);
        SetDeveloper(false);
        // ST-1: PollDev ticks the self-test every frame; OpenForCheck opens a page the build shows and refuses others.
        g_selfTestTicks = 0;
        PollDev();
        PollDev();
        CHECK(g_selfTestTicks == 2);
        CHECK(OpenForCheck("FFB", "more") && IsOpen() && DescriptorPage() == StdPage::Ffb && DescriptorMoreOptions());
        CHECK(OpenForCheck("controls", "") && DescriptorPage() == StdPage::Controls && !DescriptorMoreOptions());
        CHECK(!OpenForCheck("Replays", "more") && DescriptorPage() == StdPage::Controls && !DescriptorMoreOptions());
        CHECK(!OpenForCheck("Developer", "") && DescriptorPage() == StdPage::Controls);   // hidden: switch off
        CHECK(OpenForCheck("", "") && DescriptorPage() == StdPage::Controls);
        SetOpen(false);
        PanelEvent e;
        CHECK(!PollEvent(e));
        CHECK(SetToggle("ffb.enabled", true) && !SetToggle("ffb.strength", true) && !SetToggle("no.such.row", true));
        CHECK(SetNumber("ffb.strength", 150) && !SetNumber("ffb.enabled", 1));
        CHECK(SetChoice("controls.gearbox", 2) && !SetChoice("controls.gearbox", 3));
        CHECK(SetText("controls.steering", "MOZA R12 \xC2\xB7 X axis") && !SetText("ffb.enabled", "x"));
        CHECK(!PollEvent(e));   // values the mod sets are not echoed back
        const float s = Scale();
        float r[4];
        SetOpen(true);
        ShowDescriptorPage(StdPage::Ffb, false);
        renderFrames(ctx, rtv, w, h, 3);
        CHECK(RowRect("ffb.enabled", r));
        const float label = (std::max)(200.0f * s, (r[2] - r[0]) * 0.38f);
        click(ctx, rtv, w, h, r[0] + label + 10 * s, r[1] + 10 * s);   // the "Off" segment
        CHECK(PollEvent(e) && waiting(e, PanelEvent::Type::Changed, "ffb.enabled") && !e.toggle);
        CHECK(!PollEvent(e));
        CHECK(RowRect("panel.more", r));
        click(ctx, rtv, w, h, r[0] + 40 * s, r[1] + 10 * s);
        CHECK(RowRect("ffb.crash", r));   // More options opened by the click
        ShowDescriptorPage(StdPage::Controls, false);
        renderFrames(ctx, rtv, w, h, 2);
        CHECK(RowRect("controls.throttle", r));
        click(ctx, rtv, w, h, r[0] + label + 20 * s, r[1] + 10 * s);
        CHECK(PollEvent(e) && waiting(e, PanelEvent::Type::Capture, "controls.throttle"));
        renderFrames(ctx, rtv, w, h, 2);
        swprintf_s(path, L"%hs\\desc-art-controls-capturing-%dx%d.png", outDir, w, h);
        CHECK(savePng(dev, ctx, tex, w, h, path));
        CHECK(CancelBindingCapture() && !CancelBindingCapture());   // Esc stops waiting, the panel stays open
        CHECK(IsOpen());
        ShowDescriptorPage(StdPage::Display, false);
        renderFrames(ctx, rtv, w, h, 2);
        CHECK(RowRect("display.screens", r));
        click(ctx, rtv, w, h, r[0] + label + 10 * s, r[1] + 10 * s);   // "Off" is selected: no change
        CHECK(!PollEvent(e));
        ShowDescriptorPage(StdPage::Help, false);
        renderFrames(ctx, rtv, w, h, 2);
        CHECK(RowRect("help.report", r));
        click(ctx, rtv, w, h, r[0] + 10 * s, r[1] + 10 * s);
        CHECK(PollEvent(e) && waiting(e, PanelEvent::Type::Action, "help.report"));
        SetOpen(false);
        rtv->Release(); tex->Release();
        ++shots;
    }
    SetDeveloper(false);
    return shots;
}

int main(int argc, char **argv)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const char *outDir = argc > 1 ? argv[1] : "build";
    ID3D11Device *dev = nullptr; ID3D11DeviceContext *ctx = nullptr;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx))) {
        fprintf(stderr, "no WARP device\n"); return 2;
    }
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplDX11_Init(dev, ctx);
    const int sizes[][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}, {7680, 1440}};
    CHECK(!dbce::panel::StatusLineActive());
    const char *pages[] = {"FFB", "Cameras", "Telemetry", "Help", "REC", "Saving", "Saved", "Hidden"};
    char ini[MAX_PATH];
    GetTempPathA(MAX_PATH, ini);
    strcat_s(ini, "dbce-panel-test.ini");
    int shots = 0;
    for (const char *pg : pages) {
        for (int view = 0; view < 2; ++view) {
            for (auto &sz : sizes) {
                const int w = sz[0], h = sz[1];
                const bool status = !strcmp(pg, "REC") || !strcmp(pg, "Saving") || !strcmp(pg, "Saved");
                const bool hidden = !strcmp(pg, "Hidden");
                if ((status || hidden) && view) continue;
                if (view == 1 && w != 2560) continue;   // Advanced once, at the owner's monitor size
                DeleteFileA(ini);
                CHECK(dbce::settings::saveKeys(ini, "Panel", {{"Page", pg}, {"View", view ? "Advanced" : "Simple"}}));
                dbce::panel::Config c;
                c.modName = "Example mod 0.0.0 (panel render check)";
                c.modRepositoryUrl = "https://github.com/d-b-c-e/dbce-wheel-mod-toolkit";
                c.wheelkitRepositoryUrl = "https://github.com/d-b-c-e/dbce-wheelkit";
                c.iniPath = ini;
                c.pages[dbce::panel::PAGE_CONTROLS] = false;   // nothing to offer here: hidden (STD-017)
                c.pages[dbce::panel::PAGE_FFB] = true;
                c.pages[dbce::panel::PAGE_CAMERAS] = true;
                c.pages[dbce::panel::PAGE_TELEMETRY] = true;
                c.drawPage = drawPage;
                c.ffbOn = ffbOn;
                c.stopFfb = stopFfb;
                dbce::panel::Configure(c);
                dbce::panel::ClearStatusLine();
                dbce::panel::SetOpen(!status && !hidden);
                if (status) {
                    char line[128];
                    strcpy_s(line, !strcmp(pg, "REC") ? "REC  01:23  |  F11 to stop and save" :
                        !strcmp(pg, "Saving") ? "Saving recording  01:23  |  Keep the game open" : "Recording saved  |  You can keep playing");
                    dbce::panel::SetStatusLine(line, !strcmp(pg, "REC") ? dbce::panel::StatusTone::Recording :
                        !strcmp(pg, "Saving") ? dbce::panel::StatusTone::Busy : dbce::panel::StatusTone::Success);
                    line[0] = 0; // setter owns a copy, not the caller's buffer
                    CHECK(dbce::panel::StatusLineActive());
                }
                g_ffb = true;
                D3D11_TEXTURE2D_DESC td = {};
                td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
                ID3D11Texture2D *tex = nullptr; ID3D11RenderTargetView *rtv = nullptr;
                dev->CreateTexture2D(&td, nullptr, &tex);
                dev->CreateRenderTargetView(tex, nullptr, &rtv);
                for (int frame = 0; frame < 3; ++frame) {   // a few frames so auto-resized groups settle
                    if (dbce::panel::Prepare(w, h)) { dbce::panel::BuildFonts(); ImGui_ImplDX11_InvalidateDeviceObjects(); }
                    ImGui_ImplDX11_NewFrame();
                    ImGuiIO &io = ImGui::GetIO();
                    io.DisplaySize = ImVec2((float)w, (float)h);
                    io.DeltaTime = 1.0f / 60.0f;
                    ImGui::NewFrame();
                    dbce::panel::Draw(w, h);
                    ImGui::Render();
                    const float bg[4] = {0.30f, 0.36f, 0.42f, 1.0f};   // a game-like backdrop
                    ctx->ClearRenderTargetView(rtv, bg);
                    ctx->OMSetRenderTargets(1, &rtv, nullptr);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                }
                // Geometry: the panel window lies inside the frame, on the centre screen of a span.
                ImGuiWindow *win = ImGui::FindWindowByName(status ? "##dbce-session-status" : "##dbce-panel");
                if (hidden) CHECK(ImGui::GetDrawData()->TotalVtxCount == 0);
                else CHECK(win != nullptr && win->Active);
                if (win && !hidden) {
                    CHECK(win->Pos.x >= 0 && win->Pos.y >= 0 && win->Pos.x + win->Size.x <= w && win->Pos.y + win->Size.y <= h);
                    if (w >= h * 2.5f) CHECK(win->Pos.x >= w / 3.0f && win->Pos.x + win->Size.x <= 2.0f * w / 3.0f);
                    if (status) {
                        CHECK((win->Flags & ImGuiWindowFlags_NoInputs) == ImGuiWindowFlags_NoInputs);
                        CHECK(!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard);
                        CHECK(!dbce::panel::IsOpen());
                    }
                }
                wchar_t path[MAX_PATH];
                swprintf_s(path, L"%hs\\panel-%hs-%hs-%dx%d.png", outDir, pg, view ? "advanced" : "simple", w, h);
                CHECK(savePng(dev, ctx, tex, w, h, path));
                ++shots;
                rtv->Release(); tex->Release();
            }
        }
    }
    // STD-024: with the panel closed the counter draws only when [Panel] ShowFrameRate is on, in the top-right corner of
    // the centre screen (the centre third of a 3:1 span, the whole 16:9 window), and takes no input.
    const int counterSizes[][2] = {{1920, 1080}, {5760, 1920}};
    for (auto &sz : counterSizes) {
        for (int on = 0; on < 2; ++on) {
            const int w = sz[0], h = sz[1];
            DeleteFileA(ini);
            CHECK(dbce::settings::saveKey(ini, "Panel", "ShowFrameRate", on ? "1" : "0"));
            dbce::panel::Config c;
            c.iniPath = ini;
            dbce::panel::Configure(c);
            dbce::panel::ClearStatusLine();
            dbce::panel::SetOpen(false);
            CHECK(dbce::panel::FrameCounterOn() == (on != 0));
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
            ID3D11Texture2D *tex = nullptr; ID3D11RenderTargetView *rtv = nullptr;
            dev->CreateTexture2D(&td, nullptr, &tex);
            dev->CreateRenderTargetView(tex, nullptr, &rtv);
            for (int frame = 0; frame < 2; ++frame) {
                dbce::panel::NoteFrame();
                if (dbce::panel::Prepare(w, h)) { dbce::panel::BuildFonts(); ImGui_ImplDX11_InvalidateDeviceObjects(); }
                ImGui_ImplDX11_NewFrame();
                ImGui::GetIO().DisplaySize = ImVec2((float)w, (float)h);
                ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
                ImGui::NewFrame();
                dbce::panel::Draw(w, h);
                ImGui::Render();
                const float bg[4] = {0.30f, 0.36f, 0.42f, 1.0f};
                ctx->ClearRenderTargetView(rtv, bg);
                ctx->OMSetRenderTargets(1, &rtv, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            }
            CHECK(!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard && !dbce::panel::IsOpen());
            RECT box;
            const bool ink = inkBox(dev, ctx, tex, w, h, &box);
            if (!on) CHECK(!ink && ImGui::GetDrawData()->TotalVtxCount == 0);
            else {
                const float left = w >= h * 2.5f ? w / 3.0f : 0.0f, right = w >= h * 2.5f ? 2.0f * w / 3.0f : (float)w;
                CHECK(ink);
                CHECK(box.right < right && box.left > left + (right - left) * 0.75f && box.top >= 0 && box.bottom < h / 8);
                printf("counter %dx%d: ink x %ld-%ld, y %ld-%ld\n", w, h, box.left, box.right, box.top, box.bottom);
                wchar_t path[MAX_PATH];
                swprintf_s(path, L"%hs\\panel-counter-%dx%d.png", outDir, w, h);
                CHECK(savePng(dev, ctx, tex, w, h, path));
            }
            rtv->Release(); tex->Release();
        }
    }
    // UI-1: every page of the GRID and art of rally sample descriptors, toasts, and values in and events out.
    shots += descriptorChecks(dev, ctx, outDir, argc > 2 ? argv[2] : "samples", ini);
    DeleteFileA(ini);
    dbce::panel::SetStatusLine("", dbce::panel::StatusTone::Busy);
    CHECK(!dbce::panel::StatusLineActive());
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    printf(failures ? "FAIL %d panel render checks\n" : "PASS panel render checks (%d screenshots)\n", failures ? failures : shots);
    return failures ? 1 : 0;
}
