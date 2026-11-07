// Smoke test for the proxy: loads build\d3d11.dll by path, creates a WARP device and swap chain on a hidden window,
// and sets one then two viewports per frame for 12 frames. The proxy should log one stable pattern per change.
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA(argc > 1 ? argv[1] : "..\\build\\d3d11.dll");
    if (!m) { printf("load failed %lu\n", GetLastError()); return 1; }
    auto create = (decltype(&D3D11CreateDeviceAndSwapChain))GetProcAddress(m, "D3D11CreateDeviceAndSwapChain");
    WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "d3d11logsmoke"; wc.hInstance = GetModuleHandleA(nullptr);
    RegisterClassA(&wc);
    HWND w = CreateWindowExA(0, wc.lpszClassName, "smoke", WS_OVERLAPPEDWINDOW, 0, 0, 320, 180, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC sd = {}; sd.BufferCount = 1; sd.BufferDesc.Width = 320; sd.BufferDesc.Height = 180;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = w;
    sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    IDXGISwapChain *sc = nullptr; ID3D11Device *dev = nullptr; ID3D11DeviceContext *ctx = nullptr;
    HRESULT hr = create(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &ctx);
    if (FAILED(hr)) { printf("create failed 0x%08lx\n", hr); return 1; }
    ID3D11Texture2D *bb = nullptr; sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bb);
    ID3D11RenderTargetView *rtv = nullptr; dev->CreateRenderTargetView(bb, nullptr, &rtv);
    D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = 64; bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer *cb = nullptr; dev->CreateBuffer(&bd, nullptr, &cb);
    D3D11_TEXTURE2D_DESC td = {}; td.Width = 64; td.Height = 32; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *rt = nullptr; dev->CreateTexture2D(&td, nullptr, &rt);
    for (int f = 0; f < 12; f++) {
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT one = {0, 0, 320, 180, 0, 1}, two[2] = {{0, 0, 320, 90, 0, 1}, {0, 90, 320, 90, 0, 1}};
        if (f < 6) ctx->RSSetViewports(1, &one);
        else { ctx->RSSetViewports(1, &two[0]); ctx->RSSetViewports(1, &two[1]); }
        D3D11_MAPPED_SUBRESOURCE ms; if (SUCCEEDED(ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) { float *p = (float *)ms.pData; for (int i = 0; i < 16; i++) p[i] = (float)(f * 100 + i); ctx->Unmap(cb, 0); }
        ctx->VSSetConstantBuffers(0, 1, &cb);
        float clear[4] = {0, 0, 0, 1}; ctx->ClearRenderTargetView(rtv, clear); ctx->Draw(3, 0);
        sc->Present(0, 0);
    }
    void **vt = *(void ***)ctx;
    static const int slots[] = {7, 12, 13, 14, 20, 21, 33, 44};
    for (int i : slots) {
        HMODULE owner = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)vt[i], &owner);
        printf("slot %d -> %s\n", i, owner == m ? "proxy" : "system");
    }
    printf("ok pid %lu\n", GetCurrentProcessId());
    return 0;
}
