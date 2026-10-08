#include <windows.h>
static BOOL FakeIsWindow(HWND h){return h==(HWND)1;}
static DWORD FakeWindowPid(HWND,DWORD* pid){*pid=GetCurrentProcessId();return 1;}
static LONG_PTR FakeWindowProc(HWND h,int,LONG_PTR){return h==(HWND)1?1:0;}
#define IsWindow FakeIsWindow
#define GetWindowThreadProcessId FakeWindowPid
#undef SetWindowLongPtrW
#define SetWindowLongPtrW FakeWindowProc
#include "../WheelFfb.cpp"
#undef IsWindow
#undef GetWindowThreadProcessId
#undef SetWindowLongPtrW
#include <cassert>
struct FakeForceEffect : IDirectInputEffect {
    int starts=0, stops=0, sets=0, releases=0;
    LONG magnitude=0; bool playing=false;
    HRESULT firstSet=S_OK, firstStart=S_OK;
    DWORD lastFlags=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;playing=false;return 1;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT e,DWORD flags) override {lastFlags=flags;const auto hr=++sets==1?firstSet:S_OK;if(SUCCEEDED(hr)){magnitude=((LPCDICONSTANTFORCE)e->lpvTypeSpecificParams)->lMagnitude;if(flags&DIEP_START)playing=true;}return hr;}
    HRESULT STDMETHODCALLTYPE Start(DWORD,DWORD) override {const auto hr=++starts==1?firstStart:S_OK;if(SUCCEEDED(hr))playing=true;return hr;}
    HRESULT STDMETHODCALLTYPE Stop() override {++stops;playing=false;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD status) override {*status=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE Download() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Unload() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return S_OK;}
};

struct FakeDevice : IDirectInputDevice8A {
    DIJOYSTATE2 state = {};
    int polls=0, reads=0, acquires=0, releases=0, unacquires=0;
    HRESULT pollResult=S_OK, readResult=S_OK, acquireResult=S_OK, formatResult=S_OK, commandResult=S_OK;
    FakeForceEffect force;
    int forceCreates=0, actuatorOns=0;
    bool omitPov=false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;return 1;}
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS caps) override {caps->dwFlags=DIDC_FORCEFEEDBACK;caps->dwAxes=2;caps->dwButtons=128;return S_OK;}
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID,LPDIPROPHEADER) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID,LPCDIPROPHEADER) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Acquire() override {++acquires;return acquireResult;}
    HRESULT STDMETHODCALLTYPE Unacquire() override {++unacquires;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size,LPVOID data) override {
        assert(size==sizeof(state));++reads;
        // Deliberately write even on failure: production must not publish it.
        DWORD saved[4]; memcpy(saved,((DIJOYSTATE2*)data)->rgdwPOV,sizeof(saved));
        memcpy(data,&state,sizeof(state));
        if(omitPov)memcpy(((DIJOYSTATE2*)data)->rgdwPOV,saved,sizeof(saved));
        return readResult;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override {return formatResult;}
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID guid,LPCDIEFFECT,LPDIRECTINPUTEFFECT* out,LPUNKNOWN) override {
        assert(IsEqualGUID(guid,GUID_ConstantForce));++forceCreates;*out=&force;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD cmd) override {
        assert(cmd==DISFFC_SETACTUATORSON);++actuatorOns;return commandResult;
    }
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Poll() override {++polls;return pollResult;}
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR,DWORD,LPDIFILEEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override {return E_NOTIMPL;}
};

struct FakeInput : IDirectInput8A {
    int creates=0;
    FakeDevice* candidate=nullptr;
    FakeDevice* otherCandidate=nullptr;
    bool includeOther=false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 1;}
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid,LPDIRECTINPUTDEVICE8A* out,LPUNKNOWN) override {
        ++creates;auto* d=guid.Data1==2?otherCandidate:candidate;if(!d)return E_NOTIMPL;*out=d;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD,LPDIENUMDEVICESCALLBACKA callback,LPVOID context,DWORD) override {
        DIDEVICEINSTANCEA a{};a.dwSize=sizeof(a);a.guidInstance.Data1=1;strcpy_s(a.tszProductName,"Fake wheel A");callback(&a,context);
        if(includeOther){a.guidInstance.Data1=2;strcpy_s(a.tszProductName,"Fake wheel B");callback(&a,context);}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID,LPCSTR,LPGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR,LPDIACTIONFORMATA,LPDIENUMDEVICESBYSEMANTICSCBA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK,LPDICONFIGUREDEVICESPARAMSA,DWORD,LPVOID) override {return E_NOTIMPL;}
};


static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 SetEnvironmentVariableA("DBCE_FFB_LOG","0");
 FakeInput input; FakeDevice reader,force; GUID selected={1},other={2};
 g_di=&input;g_auxCount=1;g_aux[0].guid=selected;g_candidates[0].guid=selected;
 input.candidate=&reader;CHECK(OpenReadDevice(0)==0);CHECK(!g_read[0].isWheel && g_read[0].dev==&reader);
 input.candidate=&force;CHECK(OpenCandidate(0)==1);CHECK(g_read[0].dev==&reader && !g_read[0].isWheel);
 int axes[8];unsigned char buttons[128];CHECK(ReadDeviceState(0,axes,buttons,128)==1);
 CHECK(reader.reads==1 && force.reads==0);
 // Same strict identity reuses the handle without an extra open.
 SetPreferredDeviceGuid(&selected);SetStrictDeviceSelection(1);
 int createsBefore=input.creates;
 CHECK(InitDirectInput(1)==1);CHECK(IsEqualGUID(g_activeGuid,selected));CHECK(input.creates==createsBefore);
 printf("CONTROL same GUID: retained handle reused without new open\n");
 CHECK(SetDeviceForcesXY(5000,0)==1 && force.force.playing && force.force.magnitude==5000);
 // Mismatched retained identity must stop/release A, not claim B or switch to it.
 CHECK(g_lockInit);SetHoldTimeoutMs(500);CHECK(g_holdThread!=nullptr);
 const HANDLE watchdog=g_holdThread;
 CHECK(InstallExitGuards()==1);const WNDPROC guard=g_guardOrigProc;
 FakeForceEffect periodic,condition,impact;
 periodic.playing=true;condition.playing=true;impact.playing=true;
 g_constantBurst[0].fx=&impact;g_constantBurst[0].used=true;g_constantBurst[0].started=true;g_constantBurst[0].lastMag=3000;
 g_periodic[0].fx=&periodic;g_periodic[0].started=true;g_periodic[0].lastMag=1500;
 g_condition[0].fx=&condition;g_condition[0].started=true;g_condition[0].lastCoeff=2000;
 SetPreferredDeviceGuid(&other);
 const int stops=force.force.stops,releases=force.releases;
 CHECK(InitDirectInput(1)==0);CHECK(g_device==nullptr && g_effect==nullptr);
 CHECK(IsEqualGUID(g_activeGuid,GUID{}) && g_lastHr==DIERR_NOTFOUND);
 CHECK(force.force.stops>stops && !force.force.playing && force.releases==releases+1 && g_holdThread==watchdog && g_lastForce==0);
 CHECK(g_di==&input && !g_stopThreads && g_holdMs==500);
 CHECK(g_guardHwnd==(HWND)1 && g_guardOrigProc==guard && guard!=nullptr);
 CHECK(periodic.stops==1 && periodic.releases==1 && !periodic.playing && g_periodic[0].fx==nullptr);
 CHECK(impact.stops==1 && impact.releases==1 && !impact.playing && !g_constantBurst[0].used && g_constantBurst[0].fx==nullptr);
 CHECK(condition.stops==1 && condition.releases==1 && !condition.playing && g_condition[0].fx==nullptr);
 CHECK(input.creates==createsBefore && g_readCount==1 && g_read[0].dev==&reader && reader.releases==0);
 CHECK(ReadDeviceState(0,axes,buttons,128)==1);
 // Reopen A only through an explicit valid request, then clear the preference.
 SetPreferredDeviceGuid(&selected);CHECK(InitDirectInput(1)==1);CHECK(IsEqualGUID(g_activeGuid,selected));
 CHECK(g_holdThread==watchdog && !g_stopThreads);
 CHECK(g_guardHwnd==(HWND)1 && g_guardOrigProc==guard);
 SetPreferredDeviceGuid(nullptr);
 CHECK(!g_preferredGuidSet);CHECK(InitDirectInput(1)==0);CHECK(g_device==nullptr && IsEqualGUID(g_activeGuid,GUID{}));
 printf("PASS strict missing/mismatch: old actuator stopped/released, identity cleared, no implicit reopen\n");
 // Target absent: no fallback acquisition. Target present but fails format: no
 // acquisition of A on the next candidate. Probe enumeration itself may open
 // read-only temporary objects; the effect/acquire counters distinguish them.
 const int acquiredA=force.acquires, effectsA=force.forceCreates;
 SetPreferredDeviceGuid(&other);
 CHECK(InitDirectInput(1)==0 && g_device==nullptr);CHECK(force.acquires==acquiredA && force.forceCreates==effectsA);
 FakeDevice target;target.formatResult=E_FAIL;input.otherCandidate=&target;input.includeOther=true;
 CHECK(InitDirectInput(1)==0 && g_device==nullptr);CHECK(target.releases>=2);
 CHECK(force.acquires==acquiredA && force.forceCreates==effectsA && target.acquires==0);
 // Non-strict retained reuse preserves the original compatibility contract.
 input.includeOther=false;SetPreferredDeviceGuid(&selected);CHECK(InitDirectInput(1)==1);
 SetStrictDeviceSelection(0);SetPreferredDeviceGuid(&other);
 CHECK(InitDirectInput(1)==1 && IsEqualGUID(g_activeGuid,selected));
 SetStrictDeviceSelection(1);SetPreferredDeviceGuid(&selected);
 // The explicit post-init worker route starts with the already-published lock.
 CHECK(g_lockInit);SetHoldTimeoutMs(500);CHECK(g_holdThread!=nullptr);FreeDirectInput();CHECK(g_holdThread==nullptr && g_lockInit);
 CHECK(g_guardOrigProc==nullptr && g_guardHwnd==nullptr);
 CHECK(g_readCount==1 && g_read[0].dev==&reader && reader.releases==0);
 CHECK(ReadDeviceState(0,axes,buttons,128)==1);CloseReadDevices();CHECK(reader.releases==1 && g_readCount==0);
 printf("PASS reader-first: separate handle retained through force init and FreeDirectInput\n");
 // Force-first readers borrow only the matching force handle and survive as dormant slots.
 g_di=&input;input.candidate=&force;CHECK(OpenCandidate(0)==1);int before=input.creates;
 CHECK(OpenReadDevice(0)==0);CHECK(g_read[0].isWheel && g_read[0].dev==nullptr && input.creates==before);
 CHECK(ReadDeviceState(0,axes,buttons,128)==1);
 SetPreferredDeviceGuid(&other);CHECK(InitDirectInput(1)==0 && g_device==nullptr);CHECK(g_readCount==1);
 CHECK(ReadDeviceState(0,axes,buttons,128)==0);
 g_di=&input;input.includeOther=true;target.formatResult=S_OK;
 CHECK(InitDirectInput(1)==1 && IsEqualGUID(g_activeGuid,other));
 CHECK(ReadDeviceState(0,axes,buttons,128)==0); // reader A must never read new B
 SetPreferredDeviceGuid(&selected);CHECK(InitDirectInput(1)==0 && g_device==nullptr);
 CHECK(InitDirectInput(1)==1);CHECK(ReadDeviceState(0,axes,buttons,128)==1);
 FreeDirectInput();CloseReadDevices();CHECK(g_readCount==0);
 printf("PASS force-first: same-GUID borrowed slot dormant after free, resumes same GUID\n");
 printf("%d retained-identity source checks; fake COM/window only, no system DirectInput/device opened\n",checks);
 return 0;
}
