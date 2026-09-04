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

static int failures = 0;

static void stage(const char* name, int ok, const char* detail)
{
    printf("  %-34s %s%s%s\n", name, ok ? "ok" : "FAIL",
           detail && detail[0] ? "  " : "", detail ? detail : "");
    if (!ok) ++failures;
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
    if (win) { ShowWindow(win, SW_SHOWNA); hwnd = (int)(INT_PTR)win; }

    printf("WheelFfb C ABI smoke test (%d-bit)\n", (int)(sizeof(void*) * 8));

    /* A consumer loads the DLL from beside its own module; here that is the exe. */
    if (!WheelFfb_Load(&ffb, L"WheelFfb.dll")) {
        const char* miss = WheelFfb_MissingExport(&ffb);
        printf("  could not load WheelFfb.dll: %s missing\n", miss ? miss : "?");
        return 1;
    }
    stage("load and resolve 31 exports", 1, "");

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

    /* InitDirectInput re-resolves the handle through the process's own
     * top-level windows anyway, so a wrong one here would still be corrected. */
    if (!ffb.InitDirectInput(hwnd)) {
        sprintf_s(detail, sizeof detail, "HRESULT 0x%08X", (unsigned)ffb.GetLastHResult());
        stage("InitDirectInput", 0, detail);
        WheelFfb_Unload(&ffb);
        return 1;
    }
    stage("InitDirectInput", 1, "device opened exclusive, constant force created");

    stage("StartEffect", ffb.StartEffect() != 0, "");

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

    WheelFfb_Unload(&ffb);
    if (win) DestroyWindow(win);

    if (failures) { printf("\n%d stage(s) failed\n", failures); return 1; }
    printf("\nWheelFfb C ABI: all stages passed\n");
    return 0;
}
