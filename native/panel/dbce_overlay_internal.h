// Private link between the overlay's shared DXGI present hook (dbce_overlay_d3d11.cpp) and its optional D3D12 renderer
// (dbce_overlay_d3d12.cpp, enabled with EnableD3D12). Not for mods to include.
#pragma once
#include <dxgi.h>

namespace dbce { namespace overlay { namespace detail {

void logf(const char *fmt, ...);
void ensureImGui();                 // one ImGui context for the process, whichever renderer draws it
void drawFrame(int w, int h);       // after the renderer's NewFrame: size, time, queued input, the panel, ImGui::Render
// Set by EnableD3D12(); null for D3D11-only mods.
extern bool (*render12)(IDXGISwapChain *sc);    // draw the open panel on a D3D12 swap chain; false if it is not one
extern void (*resize12)(IDXGISwapChain *sc);    // before ResizeBuffers: release back-buffer references
extern void (*probe12)();                       // from the overlay worker: install the D3D12 queue capture

// A hook on a shared vtable slot that survives another overlay hooking the same slot after us. Steam's overlay hooks
// the slots again for each new swap chain and keeps what it finds there, our hook included, as its "original", while
// ours still calls its hook: the two then call each other until the stack overflows (V-Rally 4 crashed this way on
// 2026-10-09; the dump shows ~12,800 nested Steam overlay frames). A hook entered again on the same thread is that loop,
// so from then on ours calls the system's own function, read from the module's file: game -> other overlay -> ours ->
// system. Without another overlay, or with one that hooks once, nothing changes.
struct SlotHook {
    const char *name;
    void *volatile next;     // what the hook calls: the slot's previous value; the system's function once a loop is seen
    void *system;            // the slot's value in the module file, moved to the loaded base (null when unknown)
    volatile long looped;
};
bool PatchSlot(void **vt, int index, void *hook, SlotHook &h);   // installs the hook; resolves h.system
void *Reentered(SlotHook &h);   // in a hook entered again on the same thread: what to call instead (null: none known)
struct Depth {                  // per-thread nesting count for one hook
    int &d;
    explicit Depth(int &x) : d(x) { ++d; }
    ~Depth() { --d; }
    Depth(const Depth &) = delete;
    Depth &operator=(const Depth &) = delete;
};

}}} // namespace dbce::overlay::detail
