/* Loads an actual built WheelFfb.dll through wheelffb.h (every export resolved by name) and checks what its first
 * calls return. Logging is off (DBCE_FFB_LOG=0), so nothing is written either.
 *   noforce_dll_check product <dll>             the product: status "off"; prints version=N. It calls no force or
 *                                               device export at all.
 *   noforce_dll_check noforce <dll> <version>   the no-force build: armed from load before any other call, the same
 *                                               version, and force refused. The only force-capable exports called
 *                                               are ones that build refuses before DirectInput, an effect or a hook.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/wheelffb.h"

static int fail(const char* what) { fprintf(stderr, "FAIL: %s\n", what); return 1; }

int wmain(int argc, wchar_t** argv)
{
    WheelFfbApi api;
    char status[256] = "", why[200] = "";
    int product, version;
    if (argc < 3) return fail("usage: noforce_dll_check product <dll> | noforce <dll> <version>");
    product = wcscmp(argv[1], L"product") == 0;
    if (!product && (wcscmp(argv[1], L"noforce") != 0 || argc < 4)) return fail("unknown mode or missing version");
    SetEnvironmentVariableW(L"DBCE_FFB_LOG", L"0");
    if (!WheelFfb_Load(&api, argv[2])) { fprintf(stderr, "FAIL: load: %s\n", WheelFfb_MissingExport(&api)); return 1; }

    api.GetTestInjectionStatus(status, (int)sizeof status);   /* the first call into the module */
    version = api.GetWheelFfbVersion();
    if (product) {
        if (strcmp(status, "off") != 0) return fail("the product is not off at load");
        printf("version=%d\n", version);
        WheelFfb_Unload(&api);
        return 0;
    }

    if (strncmp(status, "armed: no force (no-force build); ", 34) != 0) return fail("not armed at load");
    if (version != _wtoi(argv[3])) return fail("version differs from the product");
    if (api.InitDirectInput(0) != 0 || api.GetLastHResult() != (int)E_ACCESSDENIED) return fail("InitDirectInput not refused at the gate");
    if (api.ExternalFfbEnter(NULL) != EXT_BUSY) return fail("ExternalFfbEnter not refused at the gate");
    if (api.CreatePeriodicEffect(10) != -1 || api.SetDeviceForcesXY(5000, 0) != 0) return fail("an effect export was not refused");
    if (api.InstallExitGuards() != 0) return fail("InstallExitGuards installed a guard");
    /* Armed: a sample gets past the armed check and is refused only because no read slot has that instance open. */
    if (api.SubmitTestInjection("inject raw axis 0 dev={000000a1-0000-0000-0000-000000000001} value=1 ms=100", why, (int)sizeof why) != 0
        || strstr(why, "not armed") || !strstr(why, "not open")) { fprintf(stderr, "FAIL: injection gate: %s\n", why); return 1; }
    if (api.EnableTestInjection(1) != 1) return fail("EnableTestInjection(1) not accepted");
    api.GetTestInjectionStatus(status, (int)sizeof status);
    if (strncmp(status, "armed: no force (no-force build); ", 34) != 0) return fail("not armed after the checks");
    printf("PASS no-force DLL: armed at load, version %d, force refused at the gate, no guard, injection armed\n", version);
    WheelFfb_Unload(&api);
    return 0;
}
