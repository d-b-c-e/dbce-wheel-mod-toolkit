// UI-2 offline check of dbce_ui.dll, as a non-native mod would use it: the host only loads the DLL (LoadLibrary) and
// presents on a WARP D3D11 swap chain; the DLL installs its own present and input hooks. Then, through the C API only:
// configure the art of rally sample descriptor, open F6 by navigation command, go to the Force feedback page, change
// the strength and the force feedback switch, and read both changes back with dbce_ui_poll_event. The keyboard takes
// the same path (an arrow key posted to the window moves the same focus, and the game never sees it). Pixels read
// back from the presented buffer show the panel open and closed, and a toast.
//   test_ui_host.exe <dbce_ui.dll> <art-of-rally.settings.json> [folder for focus-ffb-strength.bmp]
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "dbce_ui.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "user32.lib")

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

#define API(name) static decltype(&::name) p_##name;
API(dbce_ui_version) API(dbce_ui_configure) API(dbce_ui_set_toggle) API(dbce_ui_set_number) API(dbce_ui_poll_event)
API(dbce_ui_attach_d3d9) API(dbce_ui_render_event_func)
API(dbce_ui_navigate) API(dbce_ui_is_open) API(dbce_ui_state) API(dbce_ui_show_toast) API(dbce_ui_status) API(dbce_ui_poll_log)
#undef API

static ID3D11Device *dev;
static ID3D11DeviceContext *ctx;
static IDXGISwapChain *sc;
static const UINT W = 1920, H = 1080;

static void drainLog()
{
    char line[512];
    while (p_dbce_ui_poll_log(line, sizeof line)) printf("  log: %s\n", line);
}

// One game frame: the back buffer cleared green, then Present (the DLL's hook draws over it).
static bool frame()
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

static bool pixel(UINT x, UINT y, unsigned char rgb[3])
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
// The presented frame as a 24-bit BMP, for looking at the focus box.
static void saveBmp(const char *dir, const char *name)
{
    if (!dir) return;
    ID3D11Texture2D *back = nullptr, *staging = nullptr;
    if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&back)))) return;
    D3D11_TEXTURE2D_DESC d;
    back->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(dev->CreateTexture2D(&d, nullptr, &staging))) {
        ctx->CopyResource(staging, back);
        char path[MAX_PATH];
        sprintf_s(path, "%s\\%s", dir, name);
        FILE *f = nullptr;
        if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m)) && !fopen_s(&f, path, "wb") && f) {
            const UINT row = d.Width * 3, size = row * d.Height;
            BITMAPFILEHEADER fh = {0x4D42, (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + size), 0, 0, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
            BITMAPINFOHEADER ih = {sizeof ih, (LONG)d.Width, (LONG)d.Height, 1, 24, BI_RGB, size};
            fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
            std::string line(row, '\0');
            for (UINT y = d.Height; y-- > 0;) {
                const BYTE *src = (const BYTE *)m.pData + (size_t)y * m.RowPitch;
                for (UINT x = 0; x < d.Width; ++x) { line[x * 3] = (char)src[x * 4 + 2]; line[x * 3 + 1] = (char)src[x * 4 + 1]; line[x * 3 + 2] = (char)src[x * 4]; }
                fwrite(line.data(), 1, row, f);
            }
            fclose(f);
            ctx->Unmap(staging, 0);
            printf("  saved %s\n", path);
        }
        staging->Release();
    }
    back->Release();
}
static bool isGreen(const unsigned char p[3]) { return p[0] < 30 && p[1] > 140 && p[2] < 30; }
static bool isPanel(const unsigned char p[3]) { return p[0] < 60 && p[1] < 60 && p[2] < 70; }

// A navigation command, then a frame to apply it (one command per drawn frame).
static int nav(int command)
{
    const int ok = p_dbce_ui_navigate(command);
    CHECK(frame());
    return ok;
}
static std::string focus()
{
    char f[64] = "?";
    p_dbce_ui_state(nullptr, nullptr, f, sizeof f);
    return f;
}
static int page()
{
    int p = -1;
    p_dbce_ui_state(&p, nullptr, nullptr, 0);
    return p;
}

// As the game would take a key press off its queue, through the DLL's WH_GETMESSAGE hook: true when it got it.
static bool keyReachesGame(HWND hwnd, int vk)
{
    PostMessageW(hwnd, WM_KEYDOWN, (WPARAM)vk, 1);
    MSG m = {};
    if (!PeekMessageW(&m, hwnd, WM_KEYDOWN, WM_KEYDOWN, PM_REMOVE)) return false;
    return m.message == WM_KEYDOWN;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: test_ui_host <dbce_ui.dll> <settings.json>\n"); return 2; }
    std::string json;
    FILE *f = nullptr;
    if (!fopen_s(&f, argv[2], "rb") && f) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) json.append(buf, n);
        fclose(f);
    }
    CHECK(!json.empty());

    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"dbce-ui-host-check";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, (int)W, (int)H, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = W; sd.BufferDesc.Height = H; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    CHECK(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &ctx)));
    if (failures) return 1;

    // The game's own swap chain exists before the mod loads the DLL, as in Unity.
    HMODULE ui = LoadLibraryA(argv[1]);
    CHECK(ui != nullptr);
    if (!ui) return 1;
#define BIND(name) p_##name = (decltype(p_##name))GetProcAddress(ui, #name); CHECK(p_##name != nullptr);
    BIND(dbce_ui_version) BIND(dbce_ui_configure) BIND(dbce_ui_set_toggle) BIND(dbce_ui_set_number) BIND(dbce_ui_poll_event)
    BIND(dbce_ui_attach_d3d9) BIND(dbce_ui_render_event_func)
    BIND(dbce_ui_navigate) BIND(dbce_ui_is_open) BIND(dbce_ui_state) BIND(dbce_ui_show_toast) BIND(dbce_ui_status) BIND(dbce_ui_poll_log)
#undef BIND
    if (failures) return 1;
    CHECK(p_dbce_ui_version() == DBCE_UI_API_VERSION);
    for (int i = 0; i < 200 && !(p_dbce_ui_status() & DBCE_UI_STATUS_HOOKED); ++i) { Sleep(25); frame(); }
    CHECK(p_dbce_ui_status() & DBCE_UI_STATUS_HOOKED);
    CHECK(!p_dbce_ui_navigate(DBCE_UI_NAV_OPEN));   // nothing to navigate before the mod configures

    char error[256] = "";
    CHECK(!p_dbce_ui_configure("{\"settings\":{\"product\":\"x\",\"pages\":{\"ffb\":[{\"id\":\"a\",\"kind\":\"knob\"}]}}}", -1, nullptr, 0, error, sizeof error));
    printf("  refused: %s\n", error);
    CHECK(strstr(error, "knob") != nullptr);
    CHECK(p_dbce_ui_configure(json.c_str(), (int)json.size(), nullptr, DBCE_UI_FFB_STOP, error, sizeof error));
    CHECK(p_dbce_ui_status() & DBCE_UI_STATUS_CONFIGURED);
    CHECK(p_dbce_ui_set_number("ffb.strength", 60));   // the mod pushes its saved value: not reported back
    for (int i = 0; i < 3; ++i) CHECK(frame());
    CHECK(p_dbce_ui_status() & DBCE_UI_STATUS_DRAWING);
    unsigned char px[3] = {};
    const UINT edgeX = W / 2 - 470;   // just inside the panel's left edge (960 wide, centred), in its padding
    CHECK(pixel(edgeX, H / 2, px) && isGreen(px));
    CHECK(!p_dbce_ui_is_open());
    CHECK(!p_dbce_ui_navigate(DBCE_UI_NAV_DOWN));   // closed: only Open applies

    // Open F6 by command: the panel is drawn, the focus is on the tab row of the first page (Controls).
    CHECK(nav(DBCE_UI_NAV_OPEN));
    CHECK(p_dbce_ui_is_open());
    CHECK(pixel(edgeX, H / 2, px) && isPanel(px));
    CHECK(page() == DBCE_UI_PAGE_CONTROLS && focus().empty());
    // Right: the Force feedback page. Down twice: its first control (the switch), then Strength.
    CHECK(nav(DBCE_UI_NAV_RIGHT) && page() == DBCE_UI_PAGE_FFB);
    CHECK(nav(DBCE_UI_NAV_DOWN));
    CHECK(frame());
    printf("  focus: %s\n", focus().c_str());
    CHECK(focus() == "ffb.enabled");
    CHECK(nav(DBCE_UI_NAV_DOWN));
    CHECK(frame());
    CHECK(focus() == "ffb.strength");
    saveBmp(argc > 3 ? argv[3] : nullptr, "focus-ffb-strength.bmp");
    // Right: Strength goes up one step (5%) from the value the mod pushed, and the mod reads the change.
    CHECK(nav(DBCE_UI_NAV_RIGHT));
    int type = 0, toggle = -1, choice = -1;
    double number = 0;
    char id[64] = "";
    CHECK(p_dbce_ui_poll_event(&type, id, sizeof id, &toggle, &choice, &number));
    printf("  event: type %d id %s number %.1f\n", type, id, number);
    CHECK(type == DBCE_UI_EVENT_CHANGED && !strcmp(id, "ffb.strength") && number == 65.0);
    CHECK(!p_dbce_ui_poll_event(&type, id, sizeof id, &toggle, &choice, &number));

    // The keyboard takes the same path: Up moves the same focus back to the switch, and the game never sees the key.
    CHECK(!keyReachesGame(hwnd, VK_UP));
    CHECK(frame());
    CHECK(frame());
    CHECK(focus() == "ffb.enabled");
    // Select flips the switch (on by default): the mod reads it off.
    CHECK(nav(DBCE_UI_NAV_SELECT));
    CHECK(p_dbce_ui_poll_event(&type, id, sizeof id, &toggle, &choice, &number));
    printf("  event: type %d id %s toggle %d\n", type, id, toggle);
    CHECK(type == DBCE_UI_EVENT_CHANGED && !strcmp(id, "ffb.enabled") && toggle == 0);
    // F8 asks the mod to stop force feedback; the game does not see it either.
    CHECK(!keyReachesGame(hwnd, VK_F8));
    CHECK(p_dbce_ui_poll_event(&type, id, sizeof id, &toggle, &choice, &number));
    CHECK(type == DBCE_UI_EVENT_ACTION && !strcmp(id, "panel.stop-ffb"));

    // Back closes (no binding is waiting); then a toast is drawn with the panel closed, and the game's picture is back.
    CHECK(nav(DBCE_UI_NAV_BACK));
    CHECK(!p_dbce_ui_is_open());
    p_dbce_ui_show_toast("Bonnet camera", nullptr, 3000);
    CHECK(frame());
    CHECK(frame());   // ImGui sizes a new auto-sized window on its first frame and shows it from the second
    CHECK(pixel(edgeX, H / 2, px) && isGreen(px));
    CHECK(pixel(W / 2, H - 70, px) && isPanel(px));   // the toast's dark box above the bottom edge, centred
    CHECK(keyReachesGame(hwnd, VK_UP));                 // closed: the arrows are the game's again

    // D3D9, as a Unity D3D9 game would hand it over: one of its textures, taken on the render thread by the render
    // event (GL.IssuePluginEvent). The panel is drawn on it and the input hook follows its window; the detach event
    // lets go of it before the game frees it. Skipped (not failed) when this machine offers no D3D9 device right now.
    IDirect3D9 *d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    HWND hwnd9 = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, 0, 0, 1280, 720, nullptr, nullptr, wc.hInstance, nullptr);
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_COPY; pp.BackBufferFormat = D3DFMT_X8R8G8B8; pp.BackBufferCount = 1;
    pp.BackBufferWidth = 1280; pp.BackBufferHeight = 720; pp.hDeviceWindow = hwnd9;
    IDirect3DDevice9 *dev9 = nullptr;
    const HRESULT hr9 = d3d9 ? d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd9, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev9) : E_FAIL;
    if (FAILED(hr9) || !dev9) printf("SKIP D3D9 leg: no D3D9 device on this machine now (0x%08lx)\n", (unsigned long)hr9);
    else {
        const auto frame9 = [&]() {
            dev9->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 160, 0), 1.0f, 0);
            dev9->BeginScene(); dev9->EndScene();
            return SUCCEEDED(dev9->Present(nullptr, nullptr, nullptr, nullptr));
        };
        const auto pixel9 = [&](UINT x, UINT y, unsigned char rgb[3]) {
            IDirect3DSurface9 *back = nullptr, *copy = nullptr;
            D3DSURFACE_DESC d = {};
            bool ok = SUCCEEDED(dev9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) && SUCCEEDED(back->GetDesc(&d)) &&
                      SUCCEEDED(dev9->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr)) &&
                      SUCCEEDED(dev9->GetRenderTargetData(back, copy));
            D3DLOCKED_RECT lr = {};
            if (ok && SUCCEEDED(copy->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                const BYTE *p = (const BYTE *)lr.pBits + (size_t)y * lr.Pitch + (size_t)x * 4;   // B, G, R, X
                rgb[0] = p[2]; rgb[1] = p[1]; rgb[2] = p[0];
                copy->UnlockRect();
            } else ok = false;
            if (copy) copy->Release();
            if (back) back->Release();
            printf("  D3D9 pixel (%u, %u): %u %u %u\n", x, y, rgb[0], rgb[1], rgb[2]);
            return ok;
        };
        IDirect3DTexture9 *tex = nullptr;
        CHECK(SUCCEEDED(dev9->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr)) && tex);
        CHECK(p_dbce_ui_attach_d3d9(tex));
        CHECK(!(p_dbce_ui_status() & DBCE_UI_STATUS_D3D9));   // taken only by the render event
        const auto renderEvent = (void (__stdcall *)(int))p_dbce_ui_render_event_func();
        renderEvent(DBCE_UI_RENDER_ATTACH_D3D9);
        CHECK(p_dbce_ui_status() & DBCE_UI_STATUS_D3D9);
        CHECK(frame9());
        CHECK(p_dbce_ui_navigate(DBCE_UI_NAV_OPEN));
        CHECK(frame9());
        CHECK(frame9());
        CHECK(pixel9(170, 360, px) && isPanel(px));   // just inside the panel's left edge (960 wide at 1280x720)
        CHECK(!keyReachesGame(hwnd9, VK_DOWN));       // the arrows went to the panel on the D3D9 window
        CHECK(frame9());
        CHECK(frame9());
        printf("  D3D9 focus: %s\n", focus().c_str());
        CHECK(focus() == "ffb.enabled");              // reopened on the tab row of the same page, then Down
        CHECK(p_dbce_ui_navigate(DBCE_UI_NAV_CLOSE));
        CHECK(frame9());
        CHECK(pixel9(170, 360, px) && isGreen(px));
        renderEvent(DBCE_UI_RENDER_DETACH_D3D9);
        CHECK(!(p_dbce_ui_status() & DBCE_UI_STATUS_D3D9));
        tex->Release();
        CHECK(dev9->Release() == 0);                  // let go: the game's reference was the device's last
    }
    if (d3d9) d3d9->Release();
    drainLog();

    sc->Release(); ctx->Release(); dev->Release();
    if (failures) printf("FAIL %d dbce_ui host checks\n", failures);
    else printf("PASS dbce_ui host check (%s: loaded, hooked, opened and changed by navigation, read back)\n", sizeof(void *) == 8 ? "x64" : "x86");
    return failures ? 1 : 0;
}
