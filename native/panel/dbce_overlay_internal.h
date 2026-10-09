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

}}} // namespace dbce::overlay::detail
