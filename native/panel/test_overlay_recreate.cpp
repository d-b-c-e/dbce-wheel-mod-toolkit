// Offline check on WARP that the shared DXGI present hook (dbce_overlay_d3d11) follows swap chains the game recreates on
// its window, and leaves the game's input alone on one it cannot draw. One hidden window, no game, no device:
//   1. a one-buffer sequential (blt) chain: the open panel is drawn (read back after Present); the chain is released and
//      another made on the same window, which is adopted and drawn on too (before: same size, never adopted, so F6
//      opened a panel nobody saw);
//   2. a flip-model chain can then be made on that window, after the overlay drew on the previous one;
//   3. a D3D12 chain with the D3D12 renderer off, which it cannot draw on: the open panel closes and F6 and other keys
//      reach the game.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <string.h>
#include "dbce_panel.h"
#include "dbce_overlay_d3d11.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

static int failures;
static bool g_installed, g_notDrawable;
static int g_adopted;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static void logLine(const char *s)
{
    printf("  log: %s\n", s);
    if (strstr(s, "present hook installed")) g_installed = true;
    if (strstr(s, "drawing on the swap chain")) ++g_adopted;
    if (strstr(s, "neither D3D11 nor an enabled D3D12")) g_notDrawable = true;
}
static void drawPage(dbce::panel::Page, bool) {}

// As the game would take a key press off its queue, through the panel's real WH_GETMESSAGE hook: true when it got it.
static bool keyReachesGame(HWND hwnd, int vk)
{
    PostMessageW(hwnd, WM_KEYDOWN, (WPARAM)vk, 1);
    MSG m = {};
    if (!PeekMessageW(&m, hwnd, WM_KEYDOWN, WM_KEYDOWN, PM_REMOVE)) return false;
    return m.message == WM_KEYDOWN;
}

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
    printf("  pixel (%u, %u): %u %u %u\n", x, y, rgb[0], rgb[1], rgb[2]);
    return ok;
}

int main()
{
    dbce::panel::Config c;
    c.modName = "D3D11 recreate check";
    c.drawPage = drawPage;
    c.log = logLine;
    dbce::panel::Configure(c);
    CHECK(dbce::overlay::StartD3D11(logLine));
    for (int i = 0; i < 100 && !g_installed; ++i) Sleep(50);
    CHECK(g_installed);

    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"dbce-recreate-check";
    RegisterClassExW(&wc);
    const UINT W = 1920, H = 1080;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, (int)W, (int)H, nullptr, nullptr, wc.hInstance, nullptr);
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx)));
    IDXGIDevice *xdev = nullptr; IDXGIAdapter *adapter = nullptr; IDXGIFactory2 *factory = nullptr;
    CHECK(dev && SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&xdev))) && SUCCEEDED(xdev->GetAdapter(&adapter)) &&
          SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))));
    if (failures) return 1;
    DXGI_SWAP_CHAIN_DESC blt = {};
    blt.BufferCount = 1; blt.BufferDesc.Width = W; blt.BufferDesc.Height = H; blt.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    blt.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; blt.OutputWindow = hwnd; blt.SampleDesc.Count = 1; blt.Windowed = TRUE;
    blt.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    unsigned char px[3] = {};
    const auto isPanel = [&]() { return px[0] < 60 && px[1] < 60 && px[2] < 70; };   // the panel's background, #1E2126

    // 1. A blt chain, then its replacement on the same window. The replacement is made while the first still exists, so
    //    it is a different chain (a freed chain's address may come back for the next one, which then simply counts as
    //    the same chain, as it should).
    dbce::panel::SetOpen(true);
    IDXGISwapChain *first = nullptr, *second = nullptr;
    CHECK(SUCCEEDED(factory->CreateSwapChain(dev, &blt, &first)) && first);
    if (!first) return 1;
    for (int i = 0; i < 2; ++i) CHECK(frame(dev, ctx, first));
    CHECK(g_adopted == 1);
    CHECK(pixel(dev, ctx, first, W / 2 - 300, H / 2, px) && isPanel());
    CHECK(SUCCEEDED(factory->CreateSwapChain(dev, &blt, &second)) && second);
    if (!second) return 1;
    first->Release();
    ctx->ClearState(); ctx->Flush();
    for (int i = 0; i < 2; ++i) CHECK(frame(dev, ctx, second));
    CHECK(g_adopted == 2);   // same window, same size: adopted (before: only a larger window could take over)
    CHECK(pixel(dev, ctx, second, W / 2 - 300, H / 2, px) && isPanel());

    // 2. A flip-model chain on that window once the game has let go of the drawn one.
    second->Release();
    ctx->ClearState(); ctx->Flush();
    DXGI_SWAP_CHAIN_DESC1 flip = {};
    flip.Width = W; flip.Height = H; flip.Format = DXGI_FORMAT_R8G8B8A8_UNORM; flip.SampleDesc.Count = 1;
    flip.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; flip.BufferCount = 2; flip.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    IDXGISwapChain1 *third = nullptr;
    const HRESULT hr = factory->CreateSwapChainForHwnd(dev, hwnd, &flip, nullptr, nullptr, &third);
    printf("  flip chain on the same window: 0x%08lx\n", (unsigned long)hr);
    CHECK(SUCCEEDED(hr) && third);
    if (third) {
        for (int i = 0; i < 2; ++i) CHECK(frame(dev, ctx, third));
        third->Release();
        ctx->ClearState(); ctx->Flush();
    }

    // 3. A D3D12 chain on the same window, with the D3D12 renderer not enabled (no EnableD3D12): adopted but not drawable,
    //    so the panel closes and input stays the game's. (A D3D10.1 chain is no such case: it answers as a D3D11 device.)
    IDXGIFactory4 *factory4 = nullptr; IDXGIAdapter *warp = nullptr; ID3D12Device *dev12 = nullptr; ID3D12CommandQueue *queue = nullptr;
    D3D12_COMMAND_QUEUE_DESC qd = {}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CHECK(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))) && SUCCEEDED(factory4->EnumWarpAdapter(IID_PPV_ARGS(&warp))) &&
          SUCCEEDED(D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev12))) &&
          SUCCEEDED(dev12->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))));
    IDXGISwapChain1 *fourth = nullptr;
    // Also a flip chain after the flip chain the overlay drew on: before the overlay released its view of the back
    // buffer after each frame, that view kept the released chain alive and this creation failed.
    const HRESULT hr12 = queue ? factory4->CreateSwapChainForHwnd(queue, hwnd, &flip, nullptr, nullptr, &fourth) : E_FAIL;
    printf("  D3D12 flip chain on the same window: 0x%08lx\n", (unsigned long)hr12);
    CHECK(SUCCEEDED(hr12) && fourth);
    if (fourth) {
        dbce::panel::SetOpen(true);
        CHECK(SUCCEEDED(fourth->Present(0, 0)));
        CHECK(g_notDrawable && !dbce::panel::IsOpen());
        CHECK(keyReachesGame(hwnd, VK_F6) && !dbce::panel::IsOpen());
        CHECK(keyReachesGame(hwnd, 'A'));
        CHECK(SUCCEEDED(fourth->Present(0, 0)));
        CHECK(!dbce::panel::IsOpen());
        fourth->Release();
    }
    if (queue) queue->Release();
    if (dev12) dev12->Release();
    printf(failures ? "FAIL %d D3D11 recreate checks\n" : "PASS D3D11 recreate check (recreated chains followed; an undrawable one leaves input to the game)\n", failures);
    return failures ? 1 : 0;
}
