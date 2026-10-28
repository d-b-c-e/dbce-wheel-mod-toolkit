// Offline render check for the shared F6 panel: draws each page with sample content on a WARP D3D11 device at several
// window sizes and saves PNGs (build\panel-<page>-<w>x<h>.png) for visual review against UX-1. No game, no device.
// Also checks that the header commands (Close, Stop FFB) and the panel lie inside the window and, on a three-screen
// span, on the centre screen.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include "dbce_panel.h"
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
                WritePrivateProfileStringA("Panel", "Page", pg, ini);
                WritePrivateProfileStringA("Panel", "View", view ? "Advanced" : "Simple", ini);
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
            WritePrivateProfileStringA("Panel", "ShowFrameRate", on ? "1" : "0", ini);
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
    DeleteFileA(ini);
    dbce::panel::SetStatusLine("", dbce::panel::StatusTone::Busy);
    CHECK(!dbce::panel::StatusLineActive());
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    printf(failures ? "FAIL %d panel render checks\n" : "PASS panel render checks (%d screenshots)\n", failures ? failures : shots);
    return failures ? 1 : 0;
}
