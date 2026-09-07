/* smoke.c - exercises the whole WheelFfb C ABI against the real wheel, from a
 * native consumer's point of view: it loads the DLL through include/wheelffb.h
 * exactly as a dinput8 proxy would, rather than linking an import library.
 *
 * Deliberately applies NO torque. Every DirectInput call the game path makes -
 * enumerate, open exclusive, CreateEffect(GUID_ConstantForce), SetParameters,
 * CreateEffect(GUID_Sine), the periodic update-strategy probe, PanicStop - is
 * made with a magnitude of zero, so this can run unattended next to a live
 * direct-drive base. Pass --force to add a brief, gentle constant force when
 * someone is there to feel it.
 *
 *   build.bat x86   (or x64)   then   build\x86\smoke.exe
 *
 * Exit code 0 = every stage passed. The DLL's own log carries the detail;
 * this prints one line per stage so a failure names the stage.
 */
#include "../include/wheelffb.h"
#include <stdio.h>
#include <string.h>

/* Enumerating DirectInput here, separately from the DLL, is the point: it is
 * how a consumer with its own input layer gets the GUID it then hands to
 * SetPreferredDeviceGuid. OutRun2006Tweaks-FFB does exactly this. */
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

static BOOL CALLBACK FirstFfbCb(LPCDIDEVICEINSTANCEA inst, LPVOID ctx)
{
    *(GUID*)ctx = inst->guidInstance;
    return DIENUM_STOP;
}

/* First force-feedback device's INSTANCE guid, or zeroes if there is none. */
static int FindFirstFfbInstanceGuid(GUID* out)
{
    LPDIRECTINPUT8A di = NULL;
    GUID found;
    memset(&found, 0, sizeof found);
    if (FAILED(DirectInput8Create(GetModuleHandleW(NULL), DIRECTINPUT_VERSION,
                                  &IID_IDirectInput8A, (LPVOID*)&di, NULL)) || !di)
        return 0;
    di->lpVtbl->EnumDevices(di, DI8DEVCLASS_GAMECTRL, FirstFfbCb, &found,
                            DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK);
    di->lpVtbl->Release(di);
    *out = found;
    return (found.Data1 || found.Data2 || found.Data3);
}

static int failures = 0;

static void stage(const char* name, int ok, const char* detail)
{
    printf("  %-34s %s%s%s\n", name, ok ? "ok" : "FAIL",
           detail && detail[0] ? "  " : "", detail ? detail : "");
    if (!ok) ++failures;
}

/* Something the harness could not set up, as opposed to something the DLL got
 * wrong. Reported loudly and not counted as a failure, because a green run that
 * quietly skipped a stage is worse than a red one. */
static void stage_skip(const char* name, const char* why)
{
    printf("  %-34s SKIP  %s\n", name, why ? why : "");
}

int main(int argc, char** argv)
{
    WheelFfbApi ffb;
    WNDCLASSW wc;
    HWND win;
    char buf[260];
    char detail[512];
    int i, count, allCount, slotRoad, slotSlip, version, hwnd = 0;
    int applyForce = (argc > 1 && strcmp(argv[1], "--force") == 0);

    /* A real top-level window, because exclusive acquisition is granted
     * against one. A console process has none of its own - conhost owns the
     * console - so without this the whole run happens on a merely-acquired
     * device, which is not the state a game is in. */
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"DbceWheelFfbSmoke";
    RegisterClassW(&wc);
    win = CreateWindowExW(0, L"DbceWheelFfbSmoke", L"WheelFfb smoke", WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, 320, 200, NULL, NULL, wc.hInstance, NULL);
    if (win) {
        /* Exclusive DirectInput access is granted against a FOREGROUND window.
         * Ask for it and pump briefly; if the desktop declines - it often does
         * for a console-launched process - the run says so rather than failing
         * the DLL for it. */
        ShowWindow(win, SW_SHOW);
        SetForegroundWindow(win);
        {
            MSG msg; int spins;
            for (spins = 0; spins < 50; ++spins) {
                while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                if (GetForegroundWindow() == win) break;
                Sleep(10);
            }
            /* Becoming the foreground window and the desktop having finished
             * activating it are not the same instant. Acquire() returns
             * E_ACCESSDENIED in the gap, so settle before touching DirectInput
             * - a game has been running for many frames by the time it gets
             * here, and this is the harness standing in for that. */
            for (spins = 0; spins < 30; ++spins) {
                while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                Sleep(10);
            }
        }
        hwnd = (int)(INT_PTR)win;
    }

    printf("WheelFfb C ABI smoke test (%d-bit)\n", (int)(sizeof(void*) * 8));

    /* A consumer loads the DLL from beside its own module; here that is the exe. */
    if (!WheelFfb_Load(&ffb, L"WheelFfb.dll")) {
        const char* miss = WheelFfb_MissingExport(&ffb);
        printf("  could not load WheelFfb.dll: %s missing\n", miss ? miss : "?");
        return 1;
    }
    stage("load and resolve every export", 1, "");

    version = ffb.GetWheelFfbVersion();
    sprintf_s(detail, sizeof detail, "version %d", version);
    stage("GetWheelFfbVersion", version >= 200, detail);

    ffb.SetLogPath("smoke-ffb.log");
    stage("SetLogPath", 1, "smoke-ffb.log");

    count = ffb.EnumerateDevices();
    sprintf_s(detail, sizeof detail, "%d force-feedback device(s)", count);
    stage("EnumerateDevices", count > 0, detail);
    for (i = 0; i < count; ++i) {
        int axes = 0, buttons = 0;
        buf[0] = 0;
        ffb.GetDeviceName(i, buf, sizeof buf);
        ffb.GetDeviceInfo(i, &axes, &buttons);
        printf("       [%d] %-32s %d axes, %d buttons\n", i, buf, axes, buttons);
    }

    allCount = ffb.EnumerateAllDevices();
    sprintf_s(detail, sizeof detail, "%d controller(s) total", allCount);
    stage("EnumerateAllDevices", allCount >= count, detail);
    for (i = 0; i < allCount; ++i) {
        int axes = 0, buttons = 0, hasFfb = 0;
        buf[0] = 0;
        ffb.GetAnyDeviceName(i, buf, sizeof buf);
        ffb.GetAnyDeviceInfo(i, &axes, &buttons, &hasFfb);
        printf("       [%d] %-32s %d axes, %d buttons, ffb=%d\n", i, buf, axes, buttons, hasFfb);
    }

    if (count <= 0) {
        printf("\n  no force-feedback device attached - the rest needs one\n");
        WheelFfb_Unload(&ffb);
        return failures ? 1 : 0;
    }

    /* GetDeviceGuid must hand back the SAME instance GUID DirectInput reports.
     * A managed mod has no DirectInput layer of its own, so this export is its
     * only route to a value SetPreferredDeviceGuid will accept - and a wrong
     * one would silently select the wrong half of a Fanatec base. */
    {
        GUID fromDi, fromDll;
        memset(&fromDll, 0, sizeof fromDll);
        int got = ffb.GetDeviceGuid(0, &fromDll);
        stage("GetDeviceGuid(0)", got == 1, "");
        if (got && FindFirstFfbInstanceGuid(&fromDi)) {
            int same = memcmp(&fromDi, &fromDll, sizeof(GUID)) == 0;
            sprintf_s(detail, sizeof detail, "%08lX-%04X-%04X", fromDll.Data1, fromDll.Data2, fromDll.Data3);
            stage("  matches DirectInput's own answer", same, detail);
        }
        stage("GetDeviceGuid rejects a bad index", ffb.GetDeviceGuid(999, &fromDll) == 0, "");
        if (allCount > 0) {
            GUID any; memset(&any, 0, sizeof any);
            stage("GetAnyDeviceGuid(0)", ffb.GetAnyDeviceGuid(0, &any) == 1, "");
        }
    }

    /* The OutRun2006Tweaks-FFB sequence: its remap layer has already chosen a
     * device, so force feedback opens its own exclusive handle on that exact
     * instance rather than guessing by name. A GUID with no actuator, or one
     * that is not attached, must fall back rather than fail. */
    {
        GUID wheel;
        /* Via the DLL's own export, which is how a managed consumer gets one -
         * and it keeps this process to a single extra DirectInput instance
         * (rule 1: one per process). The cross-check above already proved the
         * value matches what DirectInput itself reports. */
        if (ffb.GetDeviceGuid(0, &wheel)) {
            unsigned char bogus[16];
            memset(bogus, 0xAB, sizeof bogus);
            ffb.SetPreferredDeviceGuid(bogus);
            ffb.SetStrictDeviceSelection(1);
            stage("strict missing GUID refuses fallback", ffb.InitDirectInput(hwnd) == 0, "no wheel opened");
            ffb.SetStrictDeviceSelection(0);
            ffb.SetPreferredDeviceGuid(&wheel);
            sprintf_s(detail, sizeof detail, "%08lX-%04X-%04X", wheel.Data1, wheel.Data2, wheel.Data3);
            stage("SetPreferredDeviceGuid (real instance)", 1, detail);
        } else {
            stage("SetPreferredDeviceGuid (no FFB device to name)", 1, "skipped");
        }
    }

    /* InitDirectInput re-resolves the handle through the process's own
     * top-level windows anyway, so a wrong one here would still be corrected. */
    if (!ffb.InitDirectInput(hwnd)) {
        sprintf_s(detail, sizeof detail, "HRESULT 0x%08X", (unsigned)ffb.GetLastHResult());
        stage("InitDirectInput", 0, detail);
        WheelFfb_Unload(&ffb);
        return 1;
    }
    stage("InitDirectInput", 1, "device opened exclusive, constant force created");

    if (ffb.StartEffect()) {
        stage("StartEffect", 1, "");
    } else {
        unsigned hr = (unsigned)ffb.GetLastHResult();
        int noPriority = (hr == 0x80070005u || hr == 0x80040205u) && (GetForegroundWindow() != win);
        if (noPriority) {
            stage_skip("StartEffect",
                "this window never reached the foreground, so DirectInput refuses "
                "exclusive access (0x80070005). Not a DLL fault; run it with the "
                "window in front to cover this stage.");
        } else {
            sprintf_s(detail, sizeof detail, "HRESULT 0x%08X", hr);
            stage("StartEffect", 0, detail);
        }
    }

    /* Zero-magnitude updates still travel the full SetParameters path. */
    for (i = 0; i < 10; ++i) {
        if (!ffb.SetDeviceForcesXY(0, 0)) {
            sprintf_s(detail, sizeof detail, "update %d, HRESULT 0x%08X", i, (unsigned)ffb.GetLastHResult());
            stage("SetDeviceForcesXY(0,0) x10", 0, detail);
            break;
        }
    }
    if (i == 10) stage("SetDeviceForcesXY(0,0) x10", 1, "no update rejected");

    slotRoad = ffb.CreatePeriodicEffect(25);
    sprintf_s(detail, sizeof detail, "slot %d", slotRoad);
    stage("CreatePeriodicEffect(25 Hz)", slotRoad >= 0, detail);

    slotSlip = ffb.CreatePeriodicEffect(40);
    sprintf_s(detail, sizeof detail, "slot %d", slotSlip);
    stage("CreatePeriodicEffect(40 Hz)", slotSlip >= 0, detail);
    stage("two periodic slots are distinct", slotRoad != slotSlip, "");

    if (slotRoad >= 0) {
        /* Silent, but the update-strategy probe and the change gate both run. */
        int ok = ffb.UpdatePeriodicEffect(slotRoad, 0, 25000);
        ok &= ffb.UpdatePeriodicEffect(slotRoad, 0, 31000);   /* period moved >10% */
        stage("UpdatePeriodicEffect (silent)", ok != 0, "");
    }

    /* Condition effects. A damper is what a base that oscillates actually
     * wants, and it is a different DirectInput family from a periodic. -1 is a
     * legitimate answer from a driver that has none, so this reports rather
     * than fails when the wheel cannot do one. */
    {
        int damper = ffb.CreateConditionEffect(1);
        sprintf_s(detail, sizeof detail, "slot %d", damper);
        stage("CreateConditionEffect(damper)", 1, damper >= 0 ? detail : "not offered by this driver");
        if (damper >= 0) {
            int ok = ffb.UpdateConditionEffect(damper, 0, 0, 0, 0);          /* silent */
            if (applyForce) ok &= ffb.UpdateConditionEffect(damper, 2000, 10000, 0, 0);      /* a real, gentle damper */
            ok &= ffb.UpdateConditionEffect(damper, 0, 0, 0, 0);             /* off again */
            stage("  update, then silence", ok != 0, "");
        }
        int spring = ffb.CreateConditionEffect(0);
        stage("CreateConditionEffect(spring)", 1, spring >= 0 ? "created" : "not offered by this driver");
        stage("unknown condition type is refused", ffb.CreateConditionEffect(99) == -1, "");
        ffb.ReleaseConditionEffects();
        stage("ReleaseConditionEffects", 1, "");
    }

    if (applyForce) {
        printf("       applying a brief gentle force (--force)\n");
        for (i = 0; i < 60; ++i) { ffb.SetDeviceForcesXY(1500, 0); Sleep(16); }
        for (i = 0; i < 60; ++i) { ffb.SetDeviceForcesXY(-1500, 0); Sleep(16); }
        if (slotRoad >= 0) { ffb.UpdatePeriodicEffect(slotRoad, 2000, 25000); Sleep(500); ffb.UpdatePeriodicEffect(slotRoad, 0, 25000); }
        ffb.SetDeviceForcesXY(0, 0);
    }

    ffb.ZeroForces();
    stage("ZeroForces", 1, "");

    ffb.ReleasePeriodicEffects();
    stage("ReleasePeriodicEffects", 1, "");

    /* PanicStop is the exit path: everything off, autocentre restored last. */
    ffb.PanicStop();
    stage("PanicStop", 1, "actuators off, autocentre restored");

    ffb.FreeDirectInput();
    stage("FreeDirectInput", 1, "");

    stage("reinitialise after PanicStop", ffb.InitDirectInput(hwnd) != 0, "");
    slotRoad = ffb.CreatePeriodicEffect(25);
    stage("periodic resumes after panic", slotRoad >= 0 && ffb.UpdatePeriodicEffect(slotRoad, 0, 25000) != 0, "silent");
    ffb.ZeroForces(); ffb.PanicStop(); ffb.FreeDirectInput();

    WheelFfb_Unload(&ffb);
    if (win) DestroyWindow(win);

    if (failures) { printf("\n%d stage(s) failed\n", failures); return 1; }
    printf("\nWheelFfb C ABI: all stages passed\n");
    return 0;
}
