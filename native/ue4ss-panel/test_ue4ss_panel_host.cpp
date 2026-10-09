// Offline check of the UE4SS panel DLL as a game gets it: loaded from a UE4SS-shaped folder
// (<root>\ue4ss\Mods\DbcePanel\panel\dbce-ue4ss-panel.dll + panel.ini, <root>\ue4ss\Mods\DbceTriple\config.ini) and started
// through its luaopen export, as the DbcePanel Lua mod does. A hidden 7680x1440 flip-model D3D12 swap chain on WARP
// stands in for the game: frames are cleared grey and presented, the dev channel opens the Cameras page, and the last
// frame is read back: the panel must be on the centre third and nowhere else, and opening it must not change
// config.ini. No game, no device, no window shown. Usage: panel_host.exe <dll> <config.ini> <out.png>
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <share.h>
#include <stdio.h>
#include <wincodec.h>
#include <string>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static std::string readAll(const char *path, long from = 0)
{
    std::string s;
    if (FILE *f = _fsopen(path, "rb", _SH_DENYNO)) {
        fseek(f, from, SEEK_SET);
        char b[4096];
        size_t got;
        while ((got = fread(b, 1, sizeof b, f)) > 0) s.append(b, got);
        fclose(f);
    }
    return s;
}
static long sizeOf(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &a) ? (long)a.nFileSizeLow : 0;
}
static void writeAll(const char *path, const char *text)
{
    if (FILE *f = _fsopen(path, "wb", _SH_DENYNO)) { fputs(text, f); fclose(f); }
}

static bool savePng(const unsigned char *rgba, UINT pitch, UINT w, UINT h, const char *path)
{
    std::string px((const char *)rgba, (size_t)pitch * h);
    for (UINT y = 0; y < h; ++y) for (UINT x = 0; x < w; ++x) { char *p = &px[(size_t)y * pitch + (size_t)x * 4]; char r = p[0]; p[0] = p[2]; p[2] = r; p[3] = '\xff'; }
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    IWICImagingFactory *f = nullptr; IWICBitmapEncoder *enc = nullptr; IWICBitmapFrameEncode *fr = nullptr; IWICStream *s = nullptr;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) &&
              SUCCEEDED(f->CreateStream(&s)) && SUCCEEDED(s->InitializeFromFilename(wpath, GENERIC_WRITE)) &&
              SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && SUCCEEDED(enc->Initialize(s, WICBitmapEncoderNoCache)) &&
              SUCCEEDED(enc->CreateNewFrame(&fr, nullptr)) && SUCCEEDED(fr->Initialize(nullptr)) && SUCCEEDED(fr->SetSize(w, h)) &&
              SUCCEEDED(fr->SetPixelFormat(&fmt)) && SUCCEEDED(fr->WritePixels(h, pitch, (UINT)px.size(), (BYTE *)&px[0])) &&
              SUCCEEDED(fr->Commit()) && SUCCEEDED(enc->Commit());
    if (fr) fr->Release(); if (enc) enc->Release(); if (s) s->Release(); if (f) f->Release();
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: panel_host <dll> <config.ini> <out.png>\n"); return 2; }
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    char local[MAX_PATH], logPath[MAX_PATH], devPath[MAX_PATH];
    GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH);
    sprintf_s(logPath, "%s\\dbce\\ue4sstest\\dbce-ue4sstest-panel.log", local);   // panel.ini LogName = ue4sstest
    sprintf_s(devPath, "%s\\dbce\\ue4sstest\\panel-dev.txt", local);
    const long logFrom = sizeOf(logPath);
    const std::string configBefore = readAll(argv[2]);
    CHECK(!configBefore.empty());

    HMODULE dll = LoadLibraryA(argv[1]);
    CHECK(dll != nullptr);
    auto open = dll ? (int (*)(void *))GetProcAddress(dll, "luaopen_dbce_ue4ss_panel") : nullptr;
    CHECK(open != nullptr);
    if (failures) return 1;
    CHECK(open(nullptr) == 0);   // what Lua's call does: no values returned
    CHECK(open(nullptr) == 0);   // a second call starts nothing twice
    std::string log;
    for (int i = 0; i < 100 && log.find("D3D12 queue capture installed") == std::string::npos; ++i) { Sleep(50); log = readAll(logPath, logFrom); }
    CHECK(log.find("D3D12 queue capture installed") != std::string::npos);
    CHECK(log.find("not found") == std::string::npos);   // the config beside the mod folder was read
    CHECK(log.find("config: Triple = auto") != std::string::npos);
    CHECK(log.find("Test game panel 9.9.9") != std::string::npos);   // panel.ini read

    IDXGIFactory4 *fac = nullptr; IDXGIAdapter *warp = nullptr; ID3D12Device *dev = nullptr; ID3D12CommandQueue *q = nullptr;
    CHECK(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&fac))) && SUCCEEDED(fac->EnumWarpAdapter(IID_PPV_ARGS(&warp))));
    CHECK(SUCCEEDED(D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev))));
    if (failures) return 1;
    D3D12_COMMAND_QUEUE_DESC qd = {}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CHECK(SUCCEEDED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q))));
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"dbce-ue4ss-panel-check";
    RegisterClassExW(&wc);
    const UINT W = 7680, H = 1440;
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, W, H, nullptr, nullptr, wc.hInstance, nullptr);   // never shown
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = W; sd.Height = H; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    IDXGISwapChain1 *sc1 = nullptr; IDXGISwapChain3 *sc = nullptr;
    CHECK(SUCCEEDED(fac->CreateSwapChainForHwnd(q, hwnd, &sd, nullptr, nullptr, &sc1)) && SUCCEEDED(sc1->QueryInterface(IID_PPV_ARGS(&sc))));
    if (failures) return 1;
    ID3D12DescriptorHeap *heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC hd = {}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 2;
    CHECK(SUCCEEDED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap))));
    const UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ID3D12Resource *bufs[2] = {};
    for (UINT i = 0; i < 2; ++i) {
        sc->GetBuffer(i, IID_PPV_ARGS(&bufs[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart(); h.ptr += (SIZE_T)i * inc;
        dev->CreateRenderTargetView(bufs[i], nullptr, h);
    }
    ID3D12CommandAllocator *alloc = nullptr; ID3D12GraphicsCommandList *list = nullptr; ID3D12Fence *fence = nullptr;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    list->Close();
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT64 fv = 0;
    auto waitGpu = [&]() { q->Signal(fence, ++fv); if (fence->GetCompletedValue() < fv) { fence->SetEventOnCompletion(fv, ev); WaitForSingleObject(ev, 5000); } };
    auto barrier = [&](ID3D12Resource *r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
        D3D12_RESOURCE_BARRIER br = {}; br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; br.Transition.pResource = r;
        br.Transition.Subresource = 0; br.Transition.StateBefore = a; br.Transition.StateAfter = b;
        list->ResourceBarrier(1, &br);
    };
    // The "game": clear each frame grey on its queue (the overlay captures that queue), then present.
    UINT lastIdx = 0;
    bool opened = false, drawn = false;
    auto frame = [&]() {
        waitGpu();
        const UINT idx = sc->GetCurrentBackBufferIndex();
        alloc->Reset(); list->Reset(alloc, nullptr);
        barrier(bufs[idx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart(); h.ptr += (SIZE_T)idx * inc;
        const float grey[4] = {0.5f, 0.5f, 0.5f, 1.0f};
        list->ClearRenderTargetView(h, grey, 0, nullptr);
        barrier(bufs[idx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        list->Close();
        ID3D12CommandList *l[1] = {list};
        q->ExecuteCommandLists(1, l);
        lastIdx = idx;
        CHECK(SUCCEEDED(sc->Present(0, 0)));
    };
    frame();   // the overlay adopts this swap chain
    writeAll(devPath, "open Cameras advanced");
    for (int i = 0; i < 120 && !(opened && drawn); ++i) {
        frame();
        Sleep(30);
        log = readAll(logPath, logFrom);
        opened = log.find("dev channel opened Cameras (Advanced)") != std::string::npos;
        drawn = log.find("D3D12 renderer ready") != std::string::npos;
    }
    CHECK(opened && drawn);
    frame(); frame();
    waitGpu();

    // Read back the last presented buffer.
    ID3D12Resource *buf = bufs[lastIdx];
    D3D12_RESOURCE_DESC rd = buf->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT64 total = 0;
    dev->GetCopyableFootprints(&rd, 0, 1, 0, &fp, nullptr, nullptr, &total);
    D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd = {}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = total; bd.Height = 1; bd.DepthOrArraySize = 1;
    bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *rb = nullptr;
    CHECK(SUCCEEDED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rb))));
    alloc->Reset(); list->Reset(alloc, nullptr);
    barrier(buf, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst = {}; dst.pResource = rb; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = fp;
    D3D12_TEXTURE_COPY_LOCATION src = {}; src.pResource = buf; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = 0;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    barrier(buf, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
    list->Close();
    ID3D12CommandList *l2[1] = {list};
    q->ExecuteCommandLists(1, l2);
    waitGpu();
    unsigned char *p = nullptr;
    D3D12_RANGE rr = {0, (SIZE_T)total};
    CHECK(SUCCEEDED(rb->Map(0, &rr, (void **)&p)));
    if (p) {
        auto px = [&](UINT x, UINT y) { return p + (size_t)y * fp.Footprint.RowPitch + (size_t)x * 4; };
        auto isPanel = [&](const unsigned char *c) { return abs(c[0] - 30) < 8 && abs(c[1] - 33) < 8 && abs(c[2] - 38) < 8; };   // #1E2126
        const unsigned char *centre = px(W / 2 - 300, H - 200), *left = px(W / 6, H / 2), *right = px(5 * W / 6, H / 2);
        printf("  centre %u %u %u, left %u %u %u, right %u %u %u\n", centre[0], centre[1], centre[2], left[0], left[1], left[2], right[0], right[1], right[2]);
        CHECK(isPanel(centre));
        CHECK(!isPanel(left) && !isPanel(right) && left[0] == right[0]);
        CHECK(savePng(p, fp.Footprint.RowPitch, W, H, argv[3]));
        rb->Unmap(0, nullptr);
    }
    writeAll(devPath, "close");
    for (int i = 0; i < 40 && log.find("dev channel closed the panel") == std::string::npos; ++i) { frame(); Sleep(30); log = readAll(logPath, logFrom); }
    CHECK(log.find("dev channel closed the panel") != std::string::npos);
    CHECK(readAll(argv[2]) == configBefore);   // looking is not changing
    printf(failures ? "FAIL %d panel host checks\n" : "PASS panel host checks (luaopen start, D3D12 draw on the centre third, config unchanged)\n", failures);
    return failures ? 1 : 0;
}
