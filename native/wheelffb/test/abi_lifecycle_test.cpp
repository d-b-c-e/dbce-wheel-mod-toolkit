// Device-free ABI lifecycle exercise against the actual published-source DLL.
#include "../include/wheelffb.h"
#include <cstdio>
#include <cwchar>
int wmain(int argc, wchar_t** argv) {
    if (argc!=2) { std::fprintf(stderr,"usage: actual_dll_lifecycle.exe absolute-DLL-path\n"); return 2; }
    if (!(std::wcslen(argv[1])>3 && argv[1][1]==L':' && (argv[1][2]==L'\\' || argv[1][2]==L'/'))) return 2;
    SetEnvironmentVariableA("DBCE_FFB_LOG","0");
    static_assert(sizeof(BOOL)==4,"Win32 BOOL ABI");
    for (int iteration=0; iteration<32; ++iteration) {
        WheelFfbApi api={};
        api.module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!api.module) { std::fprintf(stderr,"load failed %lu\n",GetLastError()); return 3; }
        int exports=0;
#define RESOLVE(ret,name,args) api.name=(WheelFfb_##name##_t)GetProcAddress(api.module,#name); if(!api.name) {std::fprintf(stderr,"missing %s\n",#name); return 4;} ++exports;
        WHEELFFB_API_LIST(RESOLVE)
#undef RESOLVE
        if (exports!=55 || api.GetWheelFfbVersion()!=1000) return 5;
        (void)api.GetLastHResult();
        api.FreeDirectInput(); // Empty cleanup remains idempotent.
        api.SetHoldTimeoutMs(1); // Starts the production worker, with no device.
        Sleep(2);
        api.SetHoldTimeoutMs(0);
        api.FreeDirectInput(); // Must join before code is unloaded.
        api.FreeDirectInput();
        api.SetHoldTimeoutMs(1); // Restart after cleanup, still without a device.
        Sleep(2);
        api.FreeDirectInput();
        if(!FreeLibrary(api.module)) return 6;
    }
    std::puts("PASS: actual DLL 55-export cdecl resolution/version, empty cleanup, worker stop/restart and 32 unload cycles; no enumeration, InitDirectInput or force calls");
    return 0;
}
