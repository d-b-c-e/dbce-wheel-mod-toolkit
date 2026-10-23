// Offline check of the D3D12 overlay path on WARP: the real present hook and queue capture (dbce_overlay_d3d11/d3d12),
// a flip-model swap chain on a hidden window, three presented frames with the panel open, the last one read back to
// build\overlay-d3d12.png. Before it, a 16-buffer chain the renderer cannot take closes the opened panel, and its
// replacement on the same window is the one drawn on. No game, no device.
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wincodec.h>
#include <stdio.h>
#include <string>
#include "dbce_panel.h"
#include "dbce_overlay_d3d11.h"
#include "dbce_overlay_d3d12.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

static int failures;
static bool g_ready, g_drawn;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static void logLine(const char *s)
{
    printf("  log: %s\n", s);
    if (strstr(s, "queue capture installed")) g_ready = true;
    if (strstr(s, "D3D12 renderer ready")) g_drawn = true;
}
static bool ffbOn() { return true; }
static void stopFfb() {}
static void drawPage(dbce::panel::Page page, bool)
{
    if (page == dbce::panel::PAGE_FFB) { dbce::panel::BeginGroup("g"); bool on = true; dbce::panel::OffOn("FFB", &on, "D3D12 overlay check."); dbce::panel::EndGroup(); }
}

static bool savePng(const char *rgba, UINT pitch, UINT w, UINT h, const wchar_t *path)
{
    std::string px(rgba, (size_t)pitch * h);
    for (UINT y = 0; y < h; ++y) for (UINT x = 0; x < w; ++x) { char *p = &px[(size_t)y * pitch + (size_t)x * 4]; char r = p[0]; p[0] = p[2]; p[2] = r; }
    IWICImagingFactory *f = nullptr; IWICBitmapEncoder *enc = nullptr; IWICBitmapFrameEncode *fr = nullptr; IWICStream *s = nullptr;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) &&
              SUCCEEDED(f->CreateStream(&s)) && SUCCEEDED(s->InitializeFromFilename(path, GENERIC_WRITE)) &&
              SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && SUCCEEDED(enc->Initialize(s, WICBitmapEncoderNoCache)) &&
              SUCCEEDED(enc->CreateNewFrame(&fr, nullptr)) && SUCCEEDED(fr->Initialize(nullptr)) && SUCCEEDED(fr->SetSize(w, h)) &&
              SUCCEEDED(fr->SetPixelFormat(&fmt)) && SUCCEEDED(fr->WritePixels(h, pitch, (UINT)px.size(), (BYTE *)&px[0])) &&
              SUCCEEDED(fr->Commit()) && SUCCEEDED(enc->Commit());
    if (fr) fr->Release(); if (enc) enc->Release(); if (s) s->Release(); if (f) f->Release();
    return ok;
}

int main(int argc, char **argv)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const char *outDir = argc > 1 ? argv[1] : "build";
    dbce::panel::Config c;
    c.modName = "D3D12 overlay check";
    c.pages[dbce::panel::PAGE_FFB] = true;
    c.drawPage = drawPage;
    c.ffbOn = ffbOn;
    c.stopFfb = stopFfb;
    c.log = logLine;
    dbce::panel::Configure(c);
    dbce::overlay::EnableD3D12();
    CHECK(dbce::overlay::StartD3D11(logLine));
    for (int i = 0; i < 100 && !g_ready; ++i) Sleep(50);
    CHECK(g_ready);

    IDXGIFactory4 *fac = nullptr; IDXGIAdapter *warp = nullptr; ID3D12Device *dev = nullptr; ID3D12CommandQueue *q = nullptr;
    CHECK(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&fac))) && SUCCEEDED(fac->EnumWarpAdapter(IID_PPV_ARGS(&warp))));
    CHECK(SUCCEEDED(D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev))));
    D3D12_COMMAND_QUEUE_DESC qd = {}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CHECK(SUCCEEDED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q))));
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"dbce-d3d12-check";
    RegisterClassExW(&wc);
    const UINT W = 2560, H = 1440;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, W, H, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = W; sd.Height = H; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 16; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    // First a chain the renderer cannot take (more than its 8 buffers): the opened panel closes instead of sitting
    // invisible on the game's input. The game then replaces it on the same window with the chain drawn on below.
    IDXGISwapChain1 *wide = nullptr;
    CHECK(SUCCEEDED(fac->CreateSwapChainForHwnd(q, hwnd, &sd, nullptr, nullptr, &wide)) && wide);
    if (wide) {
        dbce::panel::SetOpen(true);
        CHECK(SUCCEEDED(wide->Present(0, 0)));
        CHECK(!dbce::panel::IsOpen());
        wide->Release();
    }
    sd.BufferCount = 2;
    IDXGISwapChain1 *sc1 = nullptr; IDXGISwapChain3 *sc = nullptr;
    CHECK(SUCCEEDED(fac->CreateSwapChainForHwnd(q, hwnd, &sd, nullptr, nullptr, &sc1)) && SUCCEEDED(sc1->QueryInterface(IID_PPV_ARGS(&sc))));
    if (failures) return 1;
    ID3D12CommandAllocator *alloc = nullptr; ID3D12GraphicsCommandList *list = nullptr; ID3D12Fence *fence = nullptr;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    list->Close();
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT64 fv = 0;
    auto waitGpu = [&]() { q->Signal(fence, ++fv); if (fence->GetCompletedValue() < fv) { fence->SetEventOnCompletion(fv, ev); WaitForSingleObject(ev, 5000); } };
    // A "game" that executes work on its queue (the overlay captures it) and presents.
    dbce::panel::SetOpen(true);
    UINT lastIdx = 0;
    for (int frame = 0; frame < 3; ++frame) {
        waitGpu();
        alloc->Reset(); list->Reset(alloc, nullptr);
        list->Close();
        ID3D12CommandList *l[1] = {list};
        q->ExecuteCommandLists(1, l);
        lastIdx = sc->GetCurrentBackBufferIndex();
        CHECK(SUCCEEDED(sc->Present(0, 0)));
    }
    waitGpu();
    CHECK(g_drawn);
    // Read back the last presented buffer (flip sequential keeps it).
    ID3D12Resource *buf = nullptr;
    sc->GetBuffer(lastIdx, IID_PPV_ARGS(&buf));
    D3D12_RESOURCE_DESC rd = buf->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT64 total = 0;
    dev->GetCopyableFootprints(&rd, 0, 1, 0, &fp, nullptr, nullptr, &total);
    D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd = {}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1; bd.DepthOrArraySize = 1;
    bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *rb = nullptr;
    CHECK(SUCCEEDED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb))));
    alloc->Reset(); list->Reset(alloc, nullptr);
    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = buf;
    b.Transition.Subresource = 0; b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &b);
    D3D12_TEXTURE_COPY_LOCATION dst = {}; dst.pResource = rb; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
    D3D12_TEXTURE_COPY_LOCATION src = {}; src.pResource = buf; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    list->ResourceBarrier(1, &b);
    list->Close();
    ID3D12CommandList *l2[1] = {list};
    q->ExecuteCommandLists(1, l2);
    waitGpu();
    char *p = nullptr;
    D3D12_RANGE rr = {0, (SIZE_T)total};
    CHECK(SUCCEEDED(rb->Map(0, &rr, (void **)&p)));
    // The panel's opaque background (#1E2126) must appear in the middle of the frame.
    const unsigned char *mid = (const unsigned char *)p + (size_t)(H / 2) * fp.Footprint.RowPitch + (size_t)(W / 2 - 300) * 4;
    printf("  centre-left pixel %u %u %u\n", mid[0], mid[1], mid[2]);
    CHECK(mid[0] < 60 && mid[1] < 60 && mid[2] < 70);
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%hs\\overlay-d3d12.png", outDir);
    CHECK(savePng(p, fp.Footprint.RowPitch, W, H, path));
    rb->Unmap(0, nullptr);
    dbce::panel::SetOpen(false);
    printf(failures ? "FAIL %d D3D12 overlay checks\n" : "PASS D3D12 overlay check (panel drawn through the present hook)\n", failures);
    return failures ? 1 : 0;
}
