// Offline check of the D3D9 overlay (dbce_overlay_d3d9) with this machine's D3D9 device on a hidden window: no game, no
// display mode change, nothing shown. A null device is refused. Then, on a pure device with Sega Rally Revo's flags
// (PUREDEVICE | HARDWARE_VERTEXPROCESSING) and afterwards on an ordinary one (the game recreating its device, which with
// the panel closed releases everything held on the first at once): StartD3D9 on the "game" device; while closed nothing
// is drawn; while open the panel is drawn through the Present hook and the game's render target, depth surface,
// viewport, render states and world/view/projection transforms are as it left them; Reset succeeds (it fails while
// any default-pool object is alive, so ImGui's were released first) and the panel is drawn again at the new size.
// Writes build\overlay-d3d9-*.png.
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <wincodec.h>
#include <stdio.h>
#include <string.h>
#include "dbce_panel.h"
#include "dbce_overlay_d3d9.h"
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

static int failures;
static bool g_installed, g_drawing, g_refused;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static void logLine(const char *s)
{
    printf("  log: %s\n", s);
    if (strstr(s, "D3D9 present hook installed")) g_installed = true;
    if (strstr(s, "drawing on D3D9 device")) g_drawing = true;
    if (strstr(s, "no panel")) g_refused = true;
}
static bool ffbOn() { return true; }
static void stopFfb() {}
static void drawPage(dbce::panel::Page page, bool)
{
    if (page == dbce::panel::PAGE_FFB) { dbce::panel::BeginGroup("g"); bool on = true; dbce::panel::OffOn("FFB", &on, "D3D9 overlay check."); dbce::panel::EndGroup(); }
}

static bool savePng(const BYTE *bgrx, UINT pitch, UINT w, UINT h, const wchar_t *path)
{
    IWICImagingFactory *f = nullptr; IWICBitmapEncoder *enc = nullptr; IWICBitmapFrameEncode *fr = nullptr; IWICStream *s = nullptr;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGR;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) &&
              SUCCEEDED(f->CreateStream(&s)) && SUCCEEDED(s->InitializeFromFilename(path, GENERIC_WRITE)) &&
              SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && SUCCEEDED(enc->Initialize(s, WICBitmapEncoderNoCache)) &&
              SUCCEEDED(enc->CreateNewFrame(&fr, nullptr)) && SUCCEEDED(fr->Initialize(nullptr)) && SUCCEEDED(fr->SetSize(w, h)) &&
              SUCCEEDED(fr->SetPixelFormat(&fmt)) && SUCCEEDED(fr->WritePixels(h, pitch, pitch * h, (BYTE *)bgrx)) &&
              SUCCEEDED(fr->Commit()) && SUCCEEDED(enc->Commit());
    if (fr) fr->Release(); if (enc) enc->Release(); if (s) s->Release(); if (f) f->Release();
    return ok;
}

// The back buffer's pixel at (x, y) as R, G, B after Present (the copy swap effect keeps its contents); optionally a PNG.
static bool readPixel(IDirect3DDevice9 *dev, UINT x, UINT y, unsigned char rgb[3], const wchar_t *png)
{
    IDirect3DSurface9 *back = nullptr, *copy = nullptr;
    D3DSURFACE_DESC d = {};
    bool ok = SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) && SUCCEEDED(back->GetDesc(&d)) &&
              SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr)) &&
              SUCCEEDED(dev->GetRenderTargetData(back, copy));
    D3DLOCKED_RECT lr = {};
    if (ok && SUCCEEDED(copy->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
        const BYTE *p = (const BYTE *)lr.pBits + (size_t)y * lr.Pitch + (size_t)x * 4;   // X8R8G8B8: B, G, R, X
        rgb[0] = p[2]; rgb[1] = p[1]; rgb[2] = p[0];
        if (png) ok = savePng((const BYTE *)lr.pBits, (UINT)lr.Pitch, d.Width, d.Height, png);
        copy->UnlockRect();
    } else {
        ok = false;
    }
    if (copy) copy->Release();
    if (back) back->Release();
    printf("  pixel (%u, %u): %u %u %u\n", x, y, rgb[0], rgb[1], rgb[2]);
    return ok;
}

// Distinct, non-identity transforms the game leaves set at Present (ImGui's backend reads and writes all three).
static D3DMATRIX matrix(float a)
{
    D3DMATRIX m = {};
    m._11 = a; m._22 = a + 1; m._33 = a + 2; m._44 = 1; m._41 = a * 10; m._42 = -a;
    return m;
}
static const D3DMATRIX kWorld = matrix(2), kView = matrix(3), kProjection = matrix(5);
static bool sameTransform(IDirect3DDevice9 *dev, D3DTRANSFORMSTATETYPE t, const D3DMATRIX &want)
{
    D3DMATRIX m = {};
    return SUCCEEDED(dev->GetTransform(t, &m)) && memcmp(&m, &want, sizeof m) == 0;
}

// One "game" device with these creation flags, from StartD3D9 through Reset. The device is returned still held.
static IDirect3DDevice9 *scenario(IDirect3D9 *d3d, HWND hwnd, DWORD flags, const char *tag, const char *outDir, IDirect3DDevice9 *previous)
{
    printf("%s device (flags 0x%02lx)\n", tag, (unsigned long)flags);
    UINT W = 1920, H = 1080;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_COPY; pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.BackBufferCount = 1;
    pp.BackBufferWidth = W; pp.BackBufferHeight = H; pp.hDeviceWindow = hwnd;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    IDirect3DDevice9 *dev = nullptr;
    CHECK(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &pp, &dev)) && dev);
    if (!dev) return nullptr;
    g_installed = g_drawing = g_refused = false;
    CHECK(dbce::overlay::StartD3D9(dev, logLine));
    CHECK(dbce::overlay::StartD3D9(dev, logLine));   // again: nothing changes
    CHECK(g_installed && !g_refused);
    // The switch came with the panel closed and no frame drawn on this device: the previous device's ImGui objects and
    // reference are gone already, so the test's own reference is its last.
    if (previous) CHECK(previous->Release() == 0);

    // One "game" frame: clear the back buffer green, leave the game's own state set at Present.
    const D3DCOLOR green = D3DCOLOR_XRGB(0, 160, 0);
    auto frame = [&](IDirect3DSurface9 *gameTarget) {
        IDirect3DSurface9 *back = nullptr;
        dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
        dev->SetRenderTarget(0, back);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, green, 1.0f, 0);
        back->Release();
        if (gameTarget) dev->SetRenderTarget(0, gameTarget);
        D3DVIEWPORT9 vp = {8, 8, 64, 32, 0.0f, 1.0f};
        dev->SetViewport(&vp);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
        dev->SetTransform(D3DTS_WORLD, &kWorld);
        dev->SetTransform(D3DTS_VIEW, &kView);
        dev->SetTransform(D3DTS_PROJECTION, &kProjection);
        return SUCCEEDED(dev->Present(nullptr, nullptr, nullptr, nullptr));
    };
    unsigned char px[3] = {};
    const auto isGreen = [&]() { return px[0] < 10 && px[1] > 140 && px[2] < 10; };
    const auto isPanel = [&]() { return px[0] < 60 && px[1] < 60 && px[2] < 70; };   // the panel's background, #1E2126

    // Closed: the window is adopted on the first frame, nothing is drawn.
    CHECK(frame(nullptr));
    CHECK(g_drawing);
    CHECK(readPixel(dev, W / 2 - 300, H / 2, px, nullptr) && isGreen());

    // Open, with the game's own render target bound at Present.
    IDirect3DSurface9 *target = nullptr, *depthBefore = nullptr;
    CHECK(SUCCEEDED(dev->CreateRenderTarget(256, 256, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &target, nullptr)));
    CHECK(SUCCEEDED(dev->GetDepthStencilSurface(&depthBefore)));
    dbce::panel::SetOpen(true);
    for (int i = 0; i < 3; ++i) CHECK(frame(target));
    IDirect3DSurface9 *rt = nullptr, *ds = nullptr;
    D3DVIEWPORT9 vp = {};
    DWORD blend = 0, cull = 0;
    CHECK(SUCCEEDED(dev->GetRenderTarget(0, &rt)) && rt == target);
    CHECK(SUCCEEDED(dev->GetDepthStencilSurface(&ds)) && ds == depthBefore);
    CHECK(SUCCEEDED(dev->GetViewport(&vp)) && vp.X == 8 && vp.Y == 8 && vp.Width == 64 && vp.Height == 32);
    CHECK(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &blend)) && blend == TRUE);
    CHECK(SUCCEEDED(dev->GetRenderState(D3DRS_CULLMODE, &cull)) && cull == D3DCULL_CW);
    CHECK(sameTransform(dev, D3DTS_WORLD, kWorld) && sameTransform(dev, D3DTS_VIEW, kView) && sameTransform(dev, D3DTS_PROJECTION, kProjection));
    if (rt) rt->Release();
    if (ds) ds->Release();
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%hs\\overlay-d3d9-%hs.png", outDir, tag);
    CHECK(readPixel(dev, W / 2 - 300, H / 2, px, path) && isPanel());

    // Reset to a new size: the game releases its default-pool objects, as it must; the overlay released ImGui's.
    target->Release();
    depthBefore->Release();
    W = 2560; H = 1440;
    pp.BackBufferWidth = W; pp.BackBufferHeight = H;
    const HRESULT hr = dev->Reset(&pp);
    printf("  Reset: 0x%08lx\n", (unsigned long)hr);
    CHECK(SUCCEEDED(hr));
    for (int i = 0; i < 2; ++i) CHECK(frame(nullptr));
    swprintf_s(path, L"%hs\\overlay-d3d9-%hs-reset.png", outDir, tag);
    CHECK(readPixel(dev, W / 2 - 300, H / 2, px, path) && isPanel());

    // Closed again: nothing drawn.
    dbce::panel::SetOpen(false);
    CHECK(frame(nullptr));
    CHECK(readPixel(dev, W / 2 - 300, H / 2, px, nullptr) && isGreen());
    return dev;
}

int main(int argc, char **argv)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const char *outDir = argc > 1 ? argv[1] : "build";
    dbce::panel::Config c;
    c.modName = "D3D9 overlay check";
    c.pages[dbce::panel::PAGE_FFB] = true;
    c.drawPage = drawPage;
    c.ffbOn = ffbOn;
    c.stopFfb = stopFfb;
    c.log = logLine;
    dbce::panel::Configure(c);

    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"dbce-d3d9-check";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, 1920, 1080, nullptr, nullptr, wc.hInstance, nullptr);
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(d3d != nullptr);
    if (failures) return 1;
    CHECK(!dbce::overlay::StartD3D9(nullptr, logLine));
    IDirect3DDevice9 *pure = scenario(d3d, hwnd, D3DCREATE_PUREDEVICE | D3DCREATE_HARDWARE_VERTEXPROCESSING, "pure", outDir, nullptr);   // 0x50, as Revo creates it
    IDirect3DDevice9 *hal = scenario(d3d, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, "hal", outDir, pure);   // the game recreating it
    if (hal) hal->Release();
    printf(failures ? "FAIL %d D3D9 overlay checks\n" : "PASS D3D9 overlay check (pure and ordinary device: panel drawn through the Present hook, state kept, Reset)\n", failures);
    return failures ? 1 : 0;
}
