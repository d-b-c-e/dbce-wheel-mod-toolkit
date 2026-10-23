// D3D9 overlay for the shared F6 panel: draws dbce::panel on the game's own IDirect3DDevice9.
//
// D3D9 gives every device its own copy of the method table, on the heap (checked on Windows 11, x86 and x64), so a probe
// device cannot reach the game's the way the shared DXGI table does for D3D11: the mod passes the game's device once it
// exists. Its Present and Reset entries are replaced in that device's table with the same loop-safe slot hook as D3D11
// (dbce_overlay_internal.h); no code bytes are written. The first Present attaches panel input to the device's window.
// Present draws only while the panel is open (or the status line is shown) and the device is not lost: between its own
// BeginScene/EndScene, on the back buffer, then the game's render targets, depth surface and viewport are put back
// (ImGui's state block restores the rest). Reset releases ImGui's default-pool objects first, so the game's Reset can
// succeed; they are recreated on the next drawn frame. A pure device (D3DCREATE_PUREDEVICE) is used when it answers
// Get* calls, as the Windows 10/11 runtime does (checked on x86 and x64), and refused when it does not.
// Not covered: PresentEx/ResetEx (D3D9Ex) and IDirect3DSwapChain9::Present; a recreated device needs another call.
// Also link dbce_overlay_d3d11.cpp and imgui_impl_dx11.cpp: the shared hook core lives there, as for D3D12.
#pragma once

struct IDirect3DDevice9;

namespace dbce { namespace overlay {

// On the thread that renders with the device (it is asked for its state), not from DllMain. False (logged) for a null
// device, a pure device that cannot read its state back, or when its entries could not be replaced. Calling it again
// for the same device changes nothing; a new device (the game recreated it) becomes the one drawn on.
bool StartD3D9(IDirect3DDevice9 *device, void (*log)(const char *line));

}} // namespace dbce::overlay
