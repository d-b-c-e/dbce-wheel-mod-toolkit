// Production constant-force path with bounded COM fakes and an injected clock.
// No DirectInput instance, real device, DLL loading or force output is used.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static ULONGLONG testTick=1000;
static DWORD TestTick32() { return static_cast<DWORD>(testTick); }
static ULONGLONG TestTick64() { return testTick; }
#define GetTickCount TestTick32
#define GetTickCount64 TestTick64
#include "../WheelFfb.cpp"
#undef GetTickCount
#undef GetTickCount64
#include <initializer_list>

static void Require(bool ok,const char* message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(2); }
}
struct Effect : IDirectInputEffect {
    HRESULT result=S_OK; int sets=0,releases=0,stops=0; LONG lastMagnitude=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;return 0;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT effect,DWORD) override {
        ++sets; lastMagnitude=static_cast<DICONSTANTFORCE*>(effect->lpvTypeSpecificParams)->lMagnitude;
        return result;
    }
    HRESULT STDMETHODCALLTYPE Start(DWORD,DWORD) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Stop() override {++stops;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD status) override {*status=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE Download() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Unload() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return E_NOTIMPL;}
};
struct Device : IDirectInputDevice8A {
    Effect replacement; HRESULT acquireResult=DIERR_INPUTLOST,createResult=E_FAIL;
    int acquires=0,unacquires=0,creates=0; bool singleAxis=false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 0;}
    HRESULT STDMETHODCALLTYPE Acquire() override {++acquires;return acquireResult;}
    HRESULT STDMETHODCALLTYPE Unacquire() override {++unacquires;return S_OK;}
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID guid,LPCDIEFFECT effect,LPDIRECTINPUTEFFECT* out,LPUNKNOWN) override {
        ++creates; Require(IsEqualGUID(guid,GUID_ConstantForce),"only constant effect creation");
        HRESULT hr=singleAxis&&effect->cAxes==2?E_FAIL:createResult;
        *out=SUCCEEDED(hr)?&replacement:nullptr;return hr;
    }
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID,LPDIPROPHEADER) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID,LPCDIPROPHEADER) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD,LPVOID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Poll() override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR,DWORD,LPDIFILEEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override {return E_NOTIMPL;}
};
static void Attach(Device& device,Effect& effect) {
    Require(EnsureEffectLock(),"effect lock");g_device=&device;g_effect=&effect;g_effectAxes=2;
}
static void CheckLoss(HRESULT error,ULONGLONG start,HRESULT acquire=S_OK) {
    Device device;Effect effect;effect.result=error;device.acquireResult=acquire;Attach(device,effect);
    testTick=start;
    for(int i=0;i<100;i++) Require(SetDeviceForcesXY(1000,0)==0,"lost update fails");
    std::printf("persistent loss: acquire=%d unacquire=%d\n",device.acquires,device.unacquires);
    Require(device.acquires==1 && device.unacquires==1,"one throttled acquisition under 100 same-tick calls");
    testTick=start+250;SetDeviceForcesXY(1000,0);Require(device.acquires==1,"250 ms boundary remains throttled");
    testTick=start+251;SetDeviceForcesXY(1000,0);Require(device.acquires==2,"251 ms permits retry");
    testTick=start+251+0x100000000ULL;SetDeviceForcesXY(1000,0);
    Require(device.acquires==3,"full DWORD tick period does not alias elapsed 64-bit throttle");
    FreeDirectInput();
}
static void CheckRecreation(bool acceptFirstRetry) {
    Device device;Effect original;original.result=E_HANDLE;Attach(device,original);testTick=1000;
    Require(SetDeviceForcesXY(4000,0)==0,"failed recreation reports failure");
    Require(device.creates==2 && !g_effect && original.releases==1,"both axes rejected; old effect released once");
    int attempts=device.creates;testTick=2000;
    Require(SetDeviceForcesXY(-2000,0)==0 && device.creates==attempts,"1000 ms heal boundary remains throttled");
    testTick=2001;
    if(!acceptFirstRetry) {
        Require(SetDeviceForcesXY(-2000,0)==0 && device.creates==attempts+2,"failed pending recreation retries after throttle");
        attempts=device.creates;
        for(int i=0;i<100;i++) Require(SetDeviceForcesXY(-2000,0)==0 && device.creates==attempts,"persistent creation failure remains bounded");
        testTick=3002;
    }
    device.acquireResult=S_OK;device.createResult=S_OK;device.singleAxis=true;
    int recovered=SetDeviceForcesXY(-2000,0);
    std::printf("recreation recovery: result=%d creates=%d\n",recovered,device.creates);
    Require(recovered==1 && g_effect==&device.replacement && g_effectAxes==1,"later caller recovers through single-axis fallback");
    Require(device.replacement.lastMagnitude==0,"recreated effect starts ramp at zero");
    testTick+=250;Require(SetDeviceForcesXY(-2000,0)==1 && device.replacement.lastMagnitude==-2000,"only fresh request used after ramp");
    FreeDirectInput();
}
static void CheckCancellation(bool panic,ULONGLONG start) {
    Device device;Effect original;original.result=E_HANDLE;Attach(device,original);testTick=start;
    Require(SetDeviceForcesXY(4000,0)==0 && !g_effect,"pending failed recreation");
    if(panic) PanicStop();else FreeDirectInput();
    int attempts=device.creates;device.createResult=S_OK;testTick=start+4000;
    Require(SetDeviceForcesXY(4000,0)==0 && device.creates==attempts,"deliberate panic/shutdown cancels pending recovery");
    FreeDirectInput();
}
int main(int argc,char** argv) {
    SetEnvironmentVariableA("DBCE_FFB_LOG","0");
    Require(argc==2,"mode required");
    if(!std::strcmp(argv[1],"loss")) CheckLoss(DIERR_INPUTLOST,1000,DIERR_INPUTLOST);
    else if(!std::strcmp(argv[1],"recreate")) CheckRecreation(true);
    else if(!std::strcmp(argv[1],"recreate-reject")) CheckRecreation(false);
    else if(!std::strcmp(argv[1],"boundaries")) {
        for(auto error:{(HRESULT)DIERR_INPUTLOST,(HRESULT)DIERR_NOTACQUIRED,(HRESULT)DIERR_NOTEXCLUSIVEACQUIRED})
            for(auto acquire:{error,(HRESULT)S_OK})
                for(auto start:{0ULL,0xffffffffULL-100,0x100000000ULL+100,0x10000000000ULL}) CheckLoss(error,start,acquire);
    } else if(!std::strcmp(argv[1],"cancel")) {
        CheckCancellation(false,1000);CheckCancellation(true,7000);
    } else Require(false,"unknown mode");
    std::puts("PASS constant recovery fake-device checks; no hardware");return 0;
}
