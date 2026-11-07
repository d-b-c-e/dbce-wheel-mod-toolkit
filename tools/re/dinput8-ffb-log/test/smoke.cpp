// Smoke test without touching the hardware: the FF device is never acquired, so CreateEffect only creates (not
// downloads) a zero-magnitude effect; Start and SetParameters(DIEP_START) must be swallowed by the muted proxy.
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
static IDirectInput8W *di;
static GUID ffGuid; static bool found;
static BOOL CALLBACK onDev(LPCDIDEVICEINSTANCEW d, LPVOID) { IDirectInputDevice8W *dev; if (FAILED(di->CreateDevice(d->guidInstance, &dev, nullptr))) return DIENUM_CONTINUE; DIDEVCAPS c = {sizeof c}; dev->GetCapabilities(&c); dev->Release(); if (c.dwFlags & DIDC_FORCEFEEDBACK) { ffGuid = d->guidInstance; found = true; return DIENUM_STOP; } return DIENUM_CONTINUE; }
int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA(argv[1]);
    auto create = (decltype(&DirectInput8Create))GetProcAddress(m, "DirectInput8Create");
    if (FAILED(create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void **)&di, nullptr))) { printf("FAIL create\n"); return 1; }
    di->EnumDevices(DI8DEVCLASS_GAMECTRL, onDev, nullptr, DIEDFL_ATTACHEDONLY);
    if (!found) { printf("no FF device; exports and enumeration only\n"); di->Release(); return 0; }
    IDirectInputDevice8W *dev = nullptr; di->CreateDevice(ffGuid, &dev, nullptr);
    dev->SetDataFormat(&c_dfDIJoystick);
    DWORD axes[1] = {DIJOFS_X}; LONG dir[1] = {0}; DICONSTANTFORCE cf = {0};
    DIEFFECT e = {sizeof e}; e.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS; e.dwDuration = INFINITE; e.dwGain = DI_FFNOMINALMAX;
    e.dwTriggerButton = DIEB_NOTRIGGER; e.cAxes = 1; e.rgdwAxes = axes; e.rglDirection = dir; e.cbTypeSpecificParams = sizeof cf; e.lpvTypeSpecificParams = &cf;
    IDirectInputEffect *fx = nullptr; HRESULT hr = dev->CreateEffect(GUID_ConstantForce, &e, &fx, nullptr);
    printf("CreateEffect 0x%08lx\n", hr);
    if (fx) {
        printf("Start 0x%08lx (expect 0 = swallowed)\n", fx->Start(1, 0));
        printf("SetParameters+START 0x%08lx\n", fx->SetParameters(&e, DIEP_TYPESPECIFICPARAMS | DIEP_START));
        fx->Release();
    }
    dev->Release(); di->Release();
    printf("ok pid %lu\n", GetCurrentProcessId());
    return 0;
}
