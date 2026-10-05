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
HRESULT play(Effect*& effect, int ms, int magnitude, int milliHz, int phase = 0, int fadeMs = 0, bool clearEnvelope = false) {
    if (!effect || ms < 1 || ms > 500 || magnitude < 0 || magnitude > 10000 || milliHz < 1000 || milliHz > 100000 || phase < 0 || phase > 35999 || fadeMs < 0 || fadeMs > ms)
        return E_INVALIDARG;
    if (magnitude == 0) return stop(effect);
    /* Stop before updating: SetParameters may implicitly restart a playing
     * effect, which would expose unverified shaped parameters to hardware.
     * Moza reports NOTDOWNLOADED for an idle/new handle; that means nothing is
     * playing and is safe to treat as an already-stopped effect. */
    HRESULT hr = effect->Stop();
    if (hr == DIERR_NOTDOWNLOADED) hr = S_OK;
    if (FAILED(hr)) { effect->Release(); effect = nullptr; return hr; }
    DIPERIODIC periodic = {}; periodic.dwMagnitude = magnitude; periodic.dwPeriod = (DWORD)(1000000000LL / milliHz);
    periodic.dwPhase = phase;
    DIENVELOPE envelope = {}; envelope.dwSize = sizeof envelope;
    envelope.dwAttackLevel = magnitude; envelope.dwFadeTime = fadeMs * 1000;
    DIEFFECT e = {}; e.dwSize = sizeof e; e.dwDuration = ms * 1000;
    e.lpEnvelope = fadeMs ? &envelope : nullptr; // Legacy explicitly clears a previous shape.
    e.cbTypeSpecificParams = sizeof periodic; e.lpvTypeSpecificParams = &periodic;
    const DWORD flags = DIEP_DURATION | DIEP_TYPESPECIFICPARAMS | ((fadeMs || clearEnvelope) ? DIEP_ENVELOPE : 0);
    hr = effect->SetParameters(&e, flags);
    if (fadeMs && hr == S_OK) {
        // Parameter echo is an API check, not proof of physical force. Refuse
        // unsupported/rounded shaping instead of silently playing another cue.
        DIPERIODIC actual = {}; DIENVELOPE actualEnvelope = {}; actualEnvelope.dwSize = sizeof actualEnvelope;
        DIEFFECT read = {}; read.dwSize = sizeof read;
        read.cbTypeSpecificParams = sizeof actual; read.lpvTypeSpecificParams = &actual; read.lpEnvelope = &actualEnvelope;
        hr = effect->GetParameters(&read, flags);
        if (hr == S_OK && (read.dwDuration != e.dwDuration || read.lpEnvelope == nullptr ||
            actual.dwMagnitude != periodic.dwMagnitude || actual.dwPeriod != periodic.dwPeriod ||
            actual.dwPhase != periodic.dwPhase || actual.lOffset != 0 ||
            actualEnvelope.dwAttackLevel != envelope.dwAttackLevel || actualEnvelope.dwAttackTime != 0 ||
            actualEnvelope.dwFadeLevel != 0 || actualEnvelope.dwFadeTime != envelope.dwFadeTime)) hr = DIERR_UNSUPPORTED;
    }
    if (fadeMs && hr != S_OK) { effect->Stop(); effect->Release(); effect = nullptr; return hr; }
    if (SUCCEEDED(hr)) hr = effect->Start(1, 0); // Exactly once. Never reacquire/retry a stale event.
    if (FAILED(hr) || (fadeMs && hr != S_OK)) { effect->Stop(); effect->Release(); effect = nullptr; }
    return hr;
}
} // namespace wheel_burst
