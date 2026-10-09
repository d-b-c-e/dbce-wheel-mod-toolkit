// D3D12 support for the shared F6 panel's overlay (dbce_overlay_d3d11.h): call EnableD3D12() before StartD3D11().
//
// The DXGI present hook is shared; for a D3D12 swap chain the panel is drawn with the game's own present queue, which
// is captured from ID3D12CommandQueue::ExecuteCommandLists (the queue table entry is patched on a temporary queue, as
// the swap-chain entries are). Per back buffer: a command allocator, a render-target view and a fence value, so a
// buffer is reused only after the GPU has finished with it; ResizeBuffers waits for the GPU and releases them first.
#pragma once

namespace dbce { namespace overlay {

void EnableD3D12();

}} // namespace dbce::overlay
