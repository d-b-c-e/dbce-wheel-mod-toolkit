// D3D11 overlay for the shared F6 panel: draws dbce::panel on the game's own swap chain.
//
// A worker thread creates a temporary D3D11 device and swap chain on a hidden window to find the DXGI swap-chain
// method table, then patches its Present, Present1 and ResizeBuffers entries (the table is shared by every swap chain of
// that class, including the game's). The first presented frame attaches panel input to the swap chain's window; the
// panel is drawn only while it is open, with the game's render targets restored afterwards. A D3D12 swap chain (no
// D3D11 device) is left alone. Nothing is done from DllMain; call Start once from the mod's initialization.
#pragma once

namespace dbce { namespace overlay {

// Returns false when the worker could not be started; the outcome is logged.
bool StartD3D11(void (*log)(const char *line));

}} // namespace dbce::overlay
