// Offline check on WARP that the shared D3D11 overlay draws the panel with correct contrast on an sRGB back buffer (the
// washed-out panel found on art of rally's single-screen fullscreen window; UI-9S), and unchanged on a plain UNORM one.
// One hidden window, a one-buffer sequential chain for each format, and the panel's background pixel read back after
// Present. Without the linearization the sRGB render-target encode would leave that pixel light grey; with it the pixel
// stays dark and matches the UNORM render.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include <string.h>
#include "dbce_panel.h"
#include "dbce_overlay_d3d11.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

static int failures;
static bool g_installed;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static void logLine(const char *s)
{
    printf("  log: %s\n", s);
    if (strstr(s, "present hook installed")) g_installed = true;
}
static void drawPage(dbce::panel::Page, bool) {}

// One game frame: the back buffer cleared green, then Present (the hook draws the open panel over it).
static bool frame(ID3D11Device *dev, ID3D11DeviceContext *ctx, IDXGISwapChain *sc)
{
    ID3D11Texture2D *back = nullptr;
    ID3D11RenderTargetView *rtv = nullptr;
    if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&back))) || FAILED(dev->CreateRenderTargetView(back, nullptr, &rtv))) { if (back) back->Release(); return false; }
    const float green[4] = {0.0f, 0.63f, 0.0f, 1.0f};
    ctx->ClearRenderTargetView(rtv, green);
    rtv->Release();
    back->Release();
    return SUCCEEDED(sc->Present(0, 0));
}

// R, G, B at (x, y) of the back buffer after Present (a one-buffer sequential chain keeps what it presented).
static bool pixel(ID3D11Device *dev, ID3D11DeviceContext *ctx, IDXGISwapChain *sc, UINT x, UINT y, unsigned char rgb[3])
{
    ID3D11Texture2D *back = nullptr, *staging = nullptr;
    if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    D3D11_TEXTURE2D_DESC d;
    back->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    bool ok = SUCCEEDED(dev->CreateTexture2D(&d, nullptr, &staging));
    if (ok) {
        ctx->CopyResource(staging, back);
        D3D11_MAPPED_SUBRESOURCE m;
        ok = SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m));
        if (ok) { memcpy(rgb, (const BYTE *)m.pData + (size_t)y * m.RowPitch + (size_t)x * 4, 3); ctx->Unmap(staging, 0); }
        staging->Release();
    }
    back->Release();
    return ok;
}

// Draw the open panel on a one-buffer sequential chain of the given format and return its background pixel.
static bool backgroundPixel(ID3D11Device *dev, ID3D11DeviceContext *ctx, IDXGIFactory2 *factory, HWND hwnd,
                            DXGI_FORMAT format, UINT w, UINT h, unsigned char rgb[3])
{
    DXGI_SWAP_CHAIN_DESC blt = {};
    blt.BufferCount = 1; blt.BufferDesc.Width = w; blt.BufferDesc.Height = h; blt.BufferDesc.Format = format;
    blt.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; blt.OutputWindow = hwnd; blt.SampleDesc.Count = 1; blt.Windowed = TRUE;
    blt.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    IDXGISwapChain *sc = nullptr;
    const HRESULT hr = factory->CreateSwapChain(dev, &blt, &sc);
    printf("  %s chain: 0x%08lx\n", format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? "sRGB" : "UNORM", (unsigned long)hr);
    if (FAILED(hr) || !sc) return false;
    dbce::panel::SetOpen(true);
    for (int i = 0; i < 2; ++i) if (!frame(dev, ctx, sc)) { sc->Release(); return false; }
    const bool ok = pixel(dev, ctx, sc, w / 2 - 300, h / 2, rgb);
    dbce::panel::SetOpen(false);
    sc->Release();
    return ok;
}

int main()
{
    dbce::panel::Config c;
    c.modName = "D3D11 sRGB check";
    c.drawPage = drawPage;
    c.log = logLine;
    dbce::panel::Configure(c);
    CHECK(dbce::overlay::StartD3D11(logLine));
    for (int i = 0; i < 100 && !g_installed; ++i) Sleep(50);
    CHECK(g_installed);
    if (failures) return 1;

    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"dbce-srgb-check";
    RegisterClassExW(&wc);
    const UINT W = 1920, H = 1080;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, (int)W, (int)H, nullptr, nullptr, wc.hInstance, nullptr);
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    CHECK(hwnd && SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx)));
    IDXGIDevice *xdev = nullptr; IDXGIAdapter *adapter = nullptr; IDXGIFactory2 *factory = nullptr;
    CHECK(dev && SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&xdev))) && SUCCEEDED(xdev->GetAdapter(&adapter)) &&
          SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))));
    if (failures) return 1;

    unsigned char unorm[3] = {}, srgb[3] = {};
    // The plain UNORM render is the known-correct reference; the sRGB render must come out the same (dark, not washed out).
    CHECK(backgroundPixel(dev, ctx, factory, hwnd, DXGI_FORMAT_R8G8B8A8_UNORM, W, H, unorm));
    CHECK(backgroundPixel(dev, ctx, factory, hwnd, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, W, H, srgb));
    const auto dark = [](const unsigned char p[3]) { return p[0] < 60 && p[1] < 60 && p[2] < 70; };
    CHECK(dark(unorm));
    CHECK(dark(srgb));
    for (int i = 0; i < 3; ++i) {
        const int diff = (int)srgb[i] - (int)unorm[i];
        CHECK(diff >= -4 && diff <= 4);
    }
    printf("  UNORM pixel: %u %u %u\n", unorm[0], unorm[1], unorm[2]);
    printf("  sRGB  pixel: %u %u %u\n", srgb[0], srgb[1], srgb[2]);

    if (dev) dev->Release();
    if (ctx) ctx->Release();
    printf(failures ? "FAIL %d D3D11 sRGB checks\n" : "PASS D3D11 sRGB check (panel contrast on an sRGB back buffer matches UNORM)\n", failures);
    return failures ? 1 : 0;
}