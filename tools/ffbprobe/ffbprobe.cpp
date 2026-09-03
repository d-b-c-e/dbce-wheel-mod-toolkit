// Probes how the attached FFB wheel accepts constant-force parameter encodings.
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
static LPDIRECTINPUT8 di; static LPDIRECTINPUTDEVICE8 dev; static char devName[260];
static BOOL CALLBACK EnumCb(LPCDIDEVICEINSTANCE inst, void*) {
    LPDIRECTINPUTDEVICE8 c; if (FAILED(di->CreateDevice(inst->guidInstance, &c, nullptr))) return DIENUM_CONTINUE;
    DIDEVCAPS caps = { sizeof(DIDEVCAPS) }; c->GetCapabilities(&caps);
    if (!(caps.dwFlags & DIDC_FORCEFEEDBACK)) { c->Release(); return DIENUM_CONTINUE; }
    dev = c; strncpy_s(devName, inst->tszProductName, _TRUNCATE); return DIENUM_STOP;
}
static void Try(const char* label, LONG cdir0, LONG cdir1, bool sendDir, LONG udir0, LONG udir1, LONG mag, DWORD dwFlagsCoord) {
    DWORD axes[2] = { DIJOFS_X, DIJOFS_Y }; LONG cdir[2] = { cdir0, cdir1 };
    DICONSTANTFORCE cf = {}; DIEFFECT e = {}; e.dwSize = sizeof(e); e.dwFlags = dwFlagsCoord | DIEFF_OBJECTOFFSETS;
    e.dwDuration = INFINITE; e.dwGain = DI_FFNOMINALMAX; e.dwTriggerButton = DIEB_NOTRIGGER; e.cAxes = 2; e.rgdwAxes = axes; e.rglDirection = cdir;
    e.cbTypeSpecificParams = sizeof(cf); e.lpvTypeSpecificParams = &cf;
    LPDIRECTINPUTEFFECT eff = nullptr; HRESULT hr = dev->CreateEffect(GUID_ConstantForce, &e, &eff, nullptr);
    printf("%-44s create=0x%08lX", label, hr); if (FAILED(hr) || !eff) { printf("\n"); return; }
    hr = eff->Start(1, 0); printf(" start=0x%08lX", hr);
    LONG udir[2] = { udir0, udir1 }; DICONSTANTFORCE ucf = {}; ucf.lMagnitude = mag;
    DIEFFECT u = {}; u.dwSize = sizeof(u); u.dwFlags = dwFlagsCoord | DIEFF_OBJECTOFFSETS; u.cAxes = 2; u.rglDirection = udir;
    u.cbTypeSpecificParams = sizeof(ucf); u.lpvTypeSpecificParams = &ucf;
    DWORD f = DIEP_TYPESPECIFICPARAMS | DIEP_START | (sendDir ? DIEP_DIRECTION : 0);
    int ok = 0, bad = 0; HRESULT last = 0;
    for (int i = 0; i < 40; i++) { ucf.lMagnitude = (i % 20 < 10) ? mag : -mag; hr = eff->SetParameters(&u, f); if (SUCCEEDED(hr)) ok++; else { bad++; last = hr; } Sleep(25); }
    printf(" updates ok=%d bad=%d%s", ok, bad, bad ? " last=" : ""); if (bad) printf("0x%08lX", last);
    ucf.lMagnitude = 0; eff->SetParameters(&u, f); eff->Stop(); eff->Release(); printf("\n");
}
static void Reacquire(const char* label, bool exclusive) {
    LONG cdir[2] = { 1, 0 }; DWORD axes[2] = { DIJOFS_X, DIJOFS_Y }; DICONSTANTFORCE cf = {};
    DIEFFECT e = {}; e.dwSize = sizeof(e); e.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS; e.dwDuration = INFINITE; e.dwGain = DI_FFNOMINALMAX;
    e.dwTriggerButton = DIEB_NOTRIGGER; e.cAxes = 2; e.rgdwAxes = axes; e.rglDirection = cdir; e.cbTypeSpecificParams = sizeof(cf); e.lpvTypeSpecificParams = &cf;
    HWND w = CreateWindowExA(0, "STATIC", "ffbprobe", WS_OVERLAPPED, 0, 0, 10, 10, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    dev->Unacquire(); HRESULT hr = dev->SetCooperativeLevel(w, (exclusive ? DISCL_EXCLUSIVE : DISCL_NONEXCLUSIVE) | DISCL_BACKGROUND);
    printf("%-44s coop=0x%08lX", label, hr); hr = dev->Acquire(); printf(" acq=0x%08lX", hr);
    LPDIRECTINPUTEFFECT eff = nullptr; hr = dev->CreateEffect(GUID_ConstantForce, &e, &eff, nullptr); printf(" create=0x%08lX", hr); if (!eff) { printf("\n"); return; }
    LONG udir[2] = { 1, 0 }; DICONSTANTFORCE ucf = {}; DIEFFECT u = {}; u.dwSize = sizeof(u); u.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS; u.cAxes = 2; u.rglDirection = udir; u.cbTypeSpecificParams = sizeof(ucf); u.lpvTypeSpecificParams = &ucf;
    DWORD f = DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START;
    ucf.lMagnitude = 1500; hr = eff->SetParameters(&u, f); printf(" upd1=0x%08lX", hr); Sleep(300);
    dev->Unacquire(); Sleep(200); hr = dev->Acquire(); printf(" reacq=0x%08lX", hr);
    ucf.lMagnitude = -1500; hr = eff->SetParameters(&u, f); printf(" upd2=0x%08lX", hr);
    if (FAILED(hr)) { HRESULT dl = eff->Download(); printf(" download=0x%08lX", dl); hr = eff->SetParameters(&u, f); printf(" upd3=0x%08lX", hr); }
    if (FAILED(hr)) { eff->Release(); eff = nullptr; hr = dev->CreateEffect(GUID_ConstantForce, &e, &eff, nullptr); printf(" recreate=0x%08lX", hr); if (eff) { hr = eff->SetParameters(&u, f); printf(" upd4=0x%08lX", hr); } }
    Sleep(300); if (eff) { ucf.lMagnitude = 0; eff->SetParameters(&u, f); eff->Stop(); eff->Release(); } printf("\n"); DestroyWindow(w);
}
int main() {
    if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&di, nullptr))) { printf("DirectInput8Create failed\n"); return 1; }
    di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumCb, nullptr, DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK);
    if (!dev) { printf("no FFB device\n"); return 1; }
    printf("device: %s\n", devName);
    dev->SetDataFormat(&c_dfDIJoystick2);
    HRESULT hr = dev->SetCooperativeLevel(GetConsoleWindow(), DISCL_EXCLUSIVE | DISCL_BACKGROUND); printf("coop=0x%08lX ", hr);
    DIPROPDWORD ac = {}; ac.diph.dwSize = sizeof(ac); ac.diph.dwHeaderSize = sizeof(ac.diph); ac.diph.dwHow = DIPH_DEVICE; ac.dwData = DIPROPAUTOCENTER_OFF; dev->SetProperty(DIPROP_AUTOCENTER, &ac.diph);
    hr = dev->Acquire(); printf("acquire=0x%08lX\n", hr);
    const LONG M = 1500;
    Try("A create{0,0} upd dir{x,0} mag|x|  (fix1)",  0, 0, true,  M, 0,  M, DIEFF_CARTESIAN);
    Try("B create{1,0} upd dir{1,0} mag +/-x (fix3)", 1, 0, true,  1, 0,  M, DIEFF_CARTESIAN);
    Try("C create{0,0} upd dir{1,0} mag +/-x",        0, 0, true,  1, 0,  M, DIEFF_CARTESIAN);
    Try("D create{10000,0} upd dir{10000,0} mag +/-x",10000,0,true,10000,0, M, DIEFF_CARTESIAN);
    Try("E create{1,0} no DIEP_DIRECTION, mag +/-x",  1, 0, false, 1, 0,  M, DIEFF_CARTESIAN);
    Try("F polar create{0,0} upd dir{0,0} mag +/-x",  0, 0, true,  0, 0,  M, DIEFF_POLAR);
    Try("G create{x,0}=M upd dir{+/-x,0} mag|x| (orig-ish)", M, 0, true, M, 0, M, DIEFF_CARTESIAN);
    Reacquire("H exclusive|background, unacq/reacq mid-run", true); Reacquire("I nonexclusive|background, unacq/reacq mid-run", false); dev->Unacquire(); dev->Release(); di->Release(); return 0;
}


