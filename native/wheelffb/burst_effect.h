// Internal finite-effect operations, shared by the DirectInput adapter and fake-effect tests.
#pragma once
namespace wheel_burst {
inline bool needs_update(DWORD oldMag, DWORD oldPeriod, bool started, DWORD mag, DWORD period) {
    return (mag > 0 && (oldMag == 0 || !started)) || (mag == 0 && oldMag != 0) ||
        labs((long)mag - (long)oldMag) > 500 ||
        (oldPeriod != 0 && labs((long)period - (long)oldPeriod) * 10 > (long)oldPeriod);
}
inline bool valid(int hz, int ms) { return hz >= 1 && hz <= 100 && ms >= 1 && ms <= 500; }
template<class Effect> HRESULT stop(Effect*& effect) {
    if (!effect) return S_OK;
    HRESULT hr = effect->Stop();
    if (FAILED(hr)) { effect->Release(); effect = nullptr; }
    return hr;
}
template<class Device, class Effect>
HRESULT create(Device* device, Effect*& effect, int hz, int ms) {
    if (!device || !valid(hz, ms)) return E_INVALIDARG;
    DWORD axes[] = { DIJOFS_X }; LONG direction[] = {0};
    DIPERIODIC periodic = {}; periodic.dwPeriod = 1000000 / hz;
    DIEFFECT e = {}; e.dwSize = sizeof e;
    e.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    e.dwDuration = ms * 1000; // The DRIVER expires it; no frame callback/timer required.
    e.dwGain = DI_FFNOMINALMAX; e.dwTriggerButton = DIEB_NOTRIGGER;
    e.cAxes = 1; e.rgdwAxes = axes; e.rglDirection = direction;
    e.cbTypeSpecificParams = sizeof periodic; e.lpvTypeSpecificParams = &periodic;
    HRESULT hr = device->CreateEffect(GUID_Sine, &e, &effect, nullptr);
    if (FAILED(hr) && effect) { effect->Release(); effect = nullptr; }
    return hr; // Created at zero magnitude and NEVER started here.
}
template<class Effect>
HRESULT play(Effect*& effect, int ms, int magnitude, int milliHz) {
    if (!effect || ms < 1 || ms > 500 || magnitude < 0 || magnitude > 10000 || milliHz < 1000 || milliHz > 100000)
        return E_INVALIDARG;
    HRESULT hr = stop(effect); // Each event restarts; no change gate, even at one unit.
    if (FAILED(hr) || magnitude == 0) return hr;
    DIPERIODIC periodic = {}; periodic.dwMagnitude = magnitude; periodic.dwPeriod = (DWORD)(1000000000LL / milliHz);
    DIEFFECT e = {}; e.dwSize = sizeof e; e.dwDuration = ms * 1000;
    e.cbTypeSpecificParams = sizeof periodic; e.lpvTypeSpecificParams = &periodic;
    hr = effect->SetParameters(&e, DIEP_DURATION | DIEP_TYPESPECIFICPARAMS);
    if (SUCCEEDED(hr)) hr = effect->Start(1, 0); // Exactly once. Never reacquire/retry a stale event.
    if (FAILED(hr)) { effect->Stop(); effect->Release(); effect = nullptr; }
    return hr;
}
} // namespace wheel_burst
