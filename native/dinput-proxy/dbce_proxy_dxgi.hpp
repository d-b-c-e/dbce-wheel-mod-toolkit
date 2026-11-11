// DXGI and D3D11 wrappers for the separate-monitor span (dbce_proxy_span.hpp), on the proxy core's vtable hooks
// (dbce_proxy_core.hpp). C++17, header-only, x64. Moved from DiRT 4 (AD-5); DiRT Rally 2.0, GRID (2019) and V-Rally 4
// carry the same code.
//
// The exe's dxgi/d3d11 creation imports (CreateDXGIFactory, CreateDXGIFactory1, D3D11CreateDeviceAndSwapChain,
// D3D11CreateDevice) are import-slot wrappers like the user32 ones; through what they create, the factory, adapter,
// output and swap-chain methods are wrapped in turn, one original per (method, vtable) in the core's table. Each wrapper
// calls the original and returns its result unchanged and only logs (bounded); IDXGIOutput::GetDesc answers the primary
// monitor's desktop rectangle with the span while the span is on. A wrapper reached through a vtable the table has no
// record for (another hook copied ours) returns E_FAIL instead of guessing another vtable's original.
//
// The game lists the creation imports its exe has, beside the user32 ones:
//   DBCE_PROXY_IMPORT("d3d11.dll", dbce::proxy::dxgi, D3D11CreateDevice), ...
// A game whose swap chain comes from IDXGIFactory2::CreateSwapChainForHwnd (DiRT Rally 2.0, GRID (2019)) also sets
// dxgi::g_wrapCreateSwapChainForHwnd before it patches the imports; DiRT 4 does not, and that method stays its own.
// The vtable hooks use dbce::proxy::kFirstGameHook up to dxgi::kNextHook; a game's own hooks start at dxgi::kNextHook.
#pragma once
#include "dbce_proxy_span.hpp"
#include <dxgi1_2.h>
#include <d3d11.h>

namespace dbce { namespace proxy { namespace dxgi {

enum : int {
    H_ENUMADAPTERS = kFirstGameHook, H_ENUMADAPTERS1, H_CREATESWAP, H_ENUMOUTPUTS, H_OUTDESC, H_RESIZEBUF, H_RESIZETGT, H_CONTOUT,
    H_SETFS, H_CREATESWAPHWND, kNextHook
};
static_assert(kNextHook <= kMaxHooks, "the core's hook table is too small");

// Wrap IDXGIFactory2::CreateSwapChainForHwnd on every factory as well (off unless the game sets it at process attach).
inline bool g_wrapCreateSwapChainForHwnd;

DBCE_PROXY_ORIG(CreateDXGIFactory) DBCE_PROXY_ORIG(CreateDXGIFactory1) DBCE_PROXY_ORIG(D3D11CreateDeviceAndSwapChain)
DBCE_PROXY_ORIG(D3D11CreateDevice)

#define DBCE_PROXY_CALL_ORIG(hook, type, self, ...)      \
    auto fn_ = (type)::dbce::proxy::orig(hook, self);    \
    if (!fn_) return E_FAIL;                             \
    HRESULT hr = fn_(self, __VA_ARGS__);                 \
    ::dbce::proxy::KeepLastError keep

inline volatile LONG n_out, n_rb, n_rt, n_co, n_fs, n_ea, n_cs, n_fac, n_dev, n_ddev;

inline HRESULT STDMETHODCALLTYPE w_OutputGetDesc(IDXGIOutput *self, DXGI_OUTPUT_DESC *d)
{
    DBCE_PROXY_CALL_ORIG(H_OUTDESC, HRESULT(STDMETHODCALLTYPE *)(IDXGIOutput *, DXGI_OUTPUT_DESC *), self, d);
    if (SUCCEEDED(hr) && d && span::active() && span::SpanDesktop(span::g_primary, span::g_span, d->DesktopCoordinates) && sample(span::n_spanDesc, 10))
        log("span: IDXGIOutput::GetDesc(%ls) answered the span", d->DeviceName);
    if (SUCCEEDED(hr) && d && sample(n_out, 60))
        log("IDXGIOutput::GetDesc(%p) = %ls desktop (%ld,%ld)-(%ld,%ld) attached %d", (void *)self, d->DeviceName,
            d->DesktopCoordinates.left, d->DesktopCoordinates.top, d->DesktopCoordinates.right, d->DesktopCoordinates.bottom, d->AttachedToDesktop);
    return hr;
}
inline void hookOutput(IDXGIOutput *o) { if (o) patch(H_OUTDESC, o, 7, (void *)&w_OutputGetDesc); }

inline HRESULT STDMETHODCALLTYPE w_EnumOutputs(IDXGIAdapter *self, UINT i, IDXGIOutput **out)
{
    DBCE_PROXY_CALL_ORIG(H_ENUMOUTPUTS, HRESULT(STDMETHODCALLTYPE *)(IDXGIAdapter *, UINT, IDXGIOutput **), self, i, out);
    if (sample(n_ea, 40)) log("IDXGIAdapter::EnumOutputs(%u) = 0x%08lx", i, hr);
    if (SUCCEEDED(hr) && out && *out) hookOutput(*out);
    return hr;
}
inline void hookAdapter(IDXGIAdapter *a) { if (a) patch(H_ENUMOUTPUTS, a, 7, (void *)&w_EnumOutputs); }

inline HRESULT STDMETHODCALLTYPE w_ResizeBuffers(IDXGISwapChain *self, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT flags)
{
    DBCE_PROXY_CALL_ORIG(H_RESIZEBUF, HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT), self, n, w, h, f, flags);
    if (sample(n_rb, 60)) log("IDXGISwapChain::ResizeBuffers(%u, %ux%u, fmt %d, 0x%x) = 0x%08lx", n, w, h, (int)f, flags, hr);
    return hr;
}
inline HRESULT STDMETHODCALLTYPE w_ResizeTarget(IDXGISwapChain *self, const DXGI_MODE_DESC *m)
{
    DBCE_PROXY_CALL_ORIG(H_RESIZETGT, HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, const DXGI_MODE_DESC *), self, m);
    if (sample(n_rt, 40)) log("IDXGISwapChain::ResizeTarget(%ux%u @%u/%u) = 0x%08lx", m ? m->Width : 0, m ? m->Height : 0,
                              m ? m->RefreshRate.Numerator : 0, m ? m->RefreshRate.Denominator : 0, hr);
    return hr;
}
inline HRESULT STDMETHODCALLTYPE w_GetContainingOutput(IDXGISwapChain *self, IDXGIOutput **out)
{
    DBCE_PROXY_CALL_ORIG(H_CONTOUT, HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, IDXGIOutput **), self, out);
    if (sample(n_co, 40)) log("IDXGISwapChain::GetContainingOutput = 0x%08lx (%p)", hr, out ? (void *)*out : nullptr);
    if (SUCCEEDED(hr) && out && *out) hookOutput(*out);
    return hr;
}
inline HRESULT STDMETHODCALLTYPE w_SetFullscreenState(IDXGISwapChain *self, BOOL fs, IDXGIOutput *o)
{
    DBCE_PROXY_CALL_ORIG(H_SETFS, HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, BOOL, IDXGIOutput *), self, fs, o);
    if (sample(n_fs, 40)) log("IDXGISwapChain::SetFullscreenState(%d, %p) = 0x%08lx", fs, (void *)o, hr);
    return hr;
}
inline void hookSwapChain(IDXGISwapChain *s)
{
    if (!s) return;
    patch(H_SETFS, s, 10, (void *)&w_SetFullscreenState);
    patch(H_RESIZEBUF, s, 13, (void *)&w_ResizeBuffers);
    patch(H_RESIZETGT, s, 14, (void *)&w_ResizeTarget);
    patch(H_CONTOUT, s, 15, (void *)&w_GetContainingOutput);
}
inline void logSwapDesc(const char *who, const DXGI_SWAP_CHAIN_DESC *d)
{
    if (d) log("%s: swap chain %ux%u fmt %d, %u buffers, windowed %d, window %p, effect %d, flags 0x%x", who, d->BufferDesc.Width,
               d->BufferDesc.Height, (int)d->BufferDesc.Format, d->BufferCount, d->Windowed, (void *)d->OutputWindow, (int)d->SwapEffect, d->Flags);
}
inline HRESULT STDMETHODCALLTYPE w_CreateSwapChain(IDXGIFactory *self, IUnknown *dev, DXGI_SWAP_CHAIN_DESC *d, IDXGISwapChain **out)
{
    if (sample(n_cs, 20)) { KeepLastError before; logSwapDesc("IDXGIFactory::CreateSwapChain", d); }
    DBCE_PROXY_CALL_ORIG(H_CREATESWAP, HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory *, IUnknown *, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **), self, dev, d, out);
    if (SUCCEEDED(hr) && out && *out) hookSwapChain(*out);
    return hr;
}
inline HRESULT STDMETHODCALLTYPE w_EnumAdapters(IDXGIFactory *self, UINT i, IDXGIAdapter **out)
{
    DBCE_PROXY_CALL_ORIG(H_ENUMADAPTERS, HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory *, UINT, IDXGIAdapter **), self, i, out);
    if (SUCCEEDED(hr) && out && *out) hookAdapter(*out);
    return hr;
}
inline HRESULT STDMETHODCALLTYPE w_EnumAdapters1(IDXGIFactory1 *self, UINT i, IDXGIAdapter1 **out)
{
    DBCE_PROXY_CALL_ORIG(H_ENUMADAPTERS1, HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory1 *, UINT, IDXGIAdapter1 **), self, i, out);
    if (SUCCEEDED(hr) && out && *out) hookAdapter(*out);
    return hr;
}
inline volatile LONG n_csh;
inline HRESULT STDMETHODCALLTYPE w_CreateSwapChainForHwnd(IDXGIFactory2 *self, IUnknown *dev, HWND h, const DXGI_SWAP_CHAIN_DESC1 *d,
                                                          const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fs, IDXGIOutput *o, IDXGISwapChain1 **out)
{
    if (sample(n_csh, 20) && d) {
        KeepLastError before;
        log("IDXGIFactory2::CreateSwapChainForHwnd(window %p): %ux%u fmt %d, %u buffers, effect %d, flags 0x%x, fullscreen desc %s windowed %d",
            (void *)h, d->Width, d->Height, (int)d->Format, d->BufferCount, (int)d->SwapEffect, d->Flags, fs ? "yes" : "no", fs ? fs->Windowed : -1);
    }
    DBCE_PROXY_CALL_ORIG(H_CREATESWAPHWND, HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory2 *, IUnknown *, HWND, const DXGI_SWAP_CHAIN_DESC1 *,
                         const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *, IDXGIOutput *, IDXGISwapChain1 **), self, dev, h, d, fs, o, out);
    if (SUCCEEDED(hr) && out && *out) hookSwapChain(*out);
    return hr;
}
// The factory functions may hand out any interface the caller asked for (IUnknown, IDXGIObject, IDXGIFactory,
// IDXGIFactory1, ...). Each method table is wrapped only through an interface pointer obtained by QueryInterface for
// exactly the interface that declares that method (Astra's review of 3dbbc94).
inline void hookFactory(IUnknown *u)
{
    if (!u) return;
    IDXGIFactory2 *f2 = nullptr;
    if (g_wrapCreateSwapChainForHwnd && SUCCEEDED(u->QueryInterface(__uuidof(IDXGIFactory2), (void **)&f2)) && f2) {
        patch(H_CREATESWAPHWND, f2, 15, (void *)&w_CreateSwapChainForHwnd);
        f2->Release();
    }
    IDXGIFactory *f = nullptr;
    if (SUCCEEDED(u->QueryInterface(__uuidof(IDXGIFactory), (void **)&f)) && f) {
        patch(H_ENUMADAPTERS, f, 7, (void *)&w_EnumAdapters);
        patch(H_CREATESWAP, f, 10, (void *)&w_CreateSwapChain);
        f->Release();
    }
    IDXGIFactory1 *f1 = nullptr;
    if (SUCCEEDED(u->QueryInterface(__uuidof(IDXGIFactory1), (void **)&f1)) && f1) {
        patch(H_ENUMADAPTERS1, f1, 12, (void *)&w_EnumAdapters1);
        f1->Release();
    }
}

// ---- the creation imports -------------------------------------------------------------------------------------------
inline HRESULT WINAPI w_CreateDXGIFactory(REFIID riid, void **out)
{
    HRESULT hr = o_CreateDXGIFactory(riid, out);
    KeepLastError keep;
    if (sample(n_fac, 10)) log("CreateDXGIFactory = 0x%08lx", hr);
    if (SUCCEEDED(hr) && out && *out) hookFactory((IUnknown *)*out);
    return hr;
}
inline HRESULT WINAPI w_CreateDXGIFactory1(REFIID riid, void **out)
{
    HRESULT hr = o_CreateDXGIFactory1(riid, out);
    KeepLastError keep;
    if (sample(n_fac, 10)) log("CreateDXGIFactory1 = 0x%08lx", hr);
    if (SUCCEEDED(hr) && out && *out) hookFactory((IUnknown *)*out);
    return hr;
}
inline HRESULT WINAPI w_D3D11CreateDeviceAndSwapChain(IDXGIAdapter *a, D3D_DRIVER_TYPE t, HMODULE sw, UINT flags, const D3D_FEATURE_LEVEL *fl,
                                                      UINT nfl, UINT sdk, const DXGI_SWAP_CHAIN_DESC *d, IDXGISwapChain **sc,
                                                      ID3D11Device **dev, D3D_FEATURE_LEVEL *got, ID3D11DeviceContext **ctx)
{
    if (sample(n_dev, 10)) { KeepLastError before; log("D3D11CreateDeviceAndSwapChain(adapter %p, driver %d, flags 0x%x)", (void *)a, (int)t, flags); logSwapDesc("  requested", d); }
    HRESULT hr = o_D3D11CreateDeviceAndSwapChain(a, t, sw, flags, fl, nfl, sdk, d, sc, dev, got, ctx);
    KeepLastError keep;
    if (sample(n_dev, 10)) log("D3D11CreateDeviceAndSwapChain = 0x%08lx, swap chain %p", hr, sc ? (void *)*sc : nullptr);
    if (SUCCEEDED(hr) && sc && *sc) hookSwapChain(*sc);
    return hr;
}
// A game that imports only D3D11CreateDevice (DiRT 4) gets its swap chain from the device's own factory. The adapter and
// the factory are reached through the device (IDXGIDevice -> GetAdapter -> GetParent) and wrapped like the imported routes.
inline HRESULT WINAPI w_D3D11CreateDevice(IDXGIAdapter *a, D3D_DRIVER_TYPE t, HMODULE sw, UINT flags, const D3D_FEATURE_LEVEL *fl,
                                          UINT nfl, UINT sdk, ID3D11Device **dev, D3D_FEATURE_LEVEL *got, ID3D11DeviceContext **ctx)
{
    HRESULT hr = o_D3D11CreateDevice(a, t, sw, flags, fl, nfl, sdk, dev, got, ctx);
    KeepLastError keep;
    if (sample(n_ddev, 10)) log("D3D11CreateDevice(adapter %p, driver %d, flags 0x%x) = 0x%08lx", (void *)a, (int)t, flags, hr);
    if (SUCCEEDED(hr) && dev && *dev) {
        IDXGIDevice *dx = nullptr;
        if (SUCCEEDED((*dev)->QueryInterface(__uuidof(IDXGIDevice), (void **)&dx)) && dx) {
            IDXGIAdapter *ad = nullptr;
            if (SUCCEEDED(dx->GetAdapter(&ad)) && ad) {
                hookAdapter(ad);
                IUnknown *factory = nullptr;
                if (SUCCEEDED(ad->GetParent(__uuidof(IDXGIFactory), (void **)&factory)) && factory) { hookFactory(factory); factory->Release(); }
                ad->Release();
            }
            dx->Release();
        }
    }
    return hr;
}

#undef DBCE_PROXY_CALL_ORIG

}}} // namespace dbce::proxy::dxgi
