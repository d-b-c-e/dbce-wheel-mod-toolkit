// Private link between the overlay's shared DXGI present hook (dbce_overlay_d3d11.cpp) and its optional D3D12 renderer
// (dbce_overlay_d3d12.cpp, enabled with EnableD3D12). Not for mods to include.
#pragma once
#include <windows.h>
#include <dxgi.h>

namespace dbce { namespace overlay { namespace detail {

void logf(const char *fmt, ...);
void ensureImGui();                 // one ImGui context for the process, whichever renderer draws it
void drawFrame(int w, int h);       // after the renderer's NewFrame: size, time, queued input, the panel, ImGui::Render
// Set by EnableD3D12(); null for D3D11-only mods.
extern bool (*render12)(IDXGISwapChain *sc);    // draw the open panel on a D3D12 swap chain; false if it is not one
extern void (*resize12)(IDXGISwapChain *sc);    // before ResizeBuffers: release back-buffer references
extern void (*probe12)();                       // from the overlay worker: install the D3D12 queue capture

// A hook on a shared vtable slot that survives another overlay taking ours as its "original". Steam's overlay patches
// the first bytes of the system function (dxgi's Present) and, when the game creates its swap chain after ours is in
// the slot, keeps our hook as what it calls next, while ours calls the system function, which jumps to Steam: the two
// call each other until the stack overflows (V-Rally 4, 2026-10-09: ~12,800 nested Steam and dbce frames in the dumps).
// A hook entered again on the same thread is that loop. From then on ours calls the system function itself: directly
// when its bytes are as in the module file, otherwise with the file's bytes put back for the length of the call (the
// other overlay's bytes restored after; one such call at a time). The chain becomes game -> other overlay -> ours ->
// system. Should the loop still go on, the further nested calls are skipped rather than overflow. Without another
// overlay, or with one that does not take ours as its original, nothing changes.
struct SlotHook {
    const char *name;
    void *volatile next;          // what the hook calls until a loop is seen: the slot's previous value
    void *system;                 // the slot's value before anyone patched it (null when unknown)
    volatile long looped;         // set once the loop was seen and the fields below are ready
    volatile long preparing, skipNoted;
    int span;                     // how many first bytes of the system function the other overlay changed (0: none)
    unsigned char original[32];   // the system function's first bytes as in the module file
    INIT_ONCE once;               // per hook: one system call with restored bytes at a time
    CRITICAL_SECTION cs;
};
bool PatchSlot(void **vt, int index, void *hook, SlotHook &h);   // installs the hook; resolves h.system
bool Reentry(SlotHook &h, int depth);   // first thing in each hook (depth before this call): false = skip this call
class SystemCall {                      // once h.looped: the system function to call (null: skip), bytes put back
public:
    explicit SystemCall(SlotHook &h);
    ~SystemCall();
    void *fn;
private:
    SlotHook &h_;
    bool locked_, swapped_;
    unsigned char saved_[32];
    SystemCall(const SystemCall &) = delete;
    SystemCall &operator=(const SystemCall &) = delete;
};
struct Depth {                  // per-thread nesting count for one hook
    int &d;
    explicit Depth(int &x) : d(x) { ++d; }
    ~Depth() { --d; }
    Depth(const Depth &) = delete;
    Depth &operator=(const Depth &) = delete;
};

}}} // namespace dbce::overlay::detail
