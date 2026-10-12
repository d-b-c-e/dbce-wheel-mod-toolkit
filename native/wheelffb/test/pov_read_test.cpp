// Exercises the actual exported read/open functions against fake COM devices.
// No system DirectInput object, window, game or hardware is opened.
#include "../WheelFfb.cpp"
#include <cassert>
#include <initializer_list>

struct FakeForceEffect : IDirectInputEffect {
    int starts=0, stops=0, sets=0, releases=0;
    HRESULT firstSet=S_OK, firstStart=S_OK;
    DWORD lastFlags=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;return 1;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT,DWORD flags) override {lastFlags=flags;return ++sets==1?firstSet:S_OK;}
    HRESULT STDMETHODCALLTYPE Start(DWORD,DWORD) override {return ++starts==1?firstStart:S_OK;}
    HRESULT STDMETHODCALLTYPE Stop() override {++stops;return S_OK;}
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
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS) override {return E_NOTIMPL;}
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
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 1;}
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID,LPDIRECTINPUTDEVICE8A* out,LPUNKNOWN) override {
        ++creates;if(!candidate)return E_NOTIMPL;*out=candidate;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD) override {assert(false);return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID,LPCSTR,LPGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR,LPDIACTIONFORMATA,LPDIENUMDEVICESBYSEMANTICSCBA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK,LPDICONFIGUREDEVICESPARAMSA,DWORD,LPVOID) override {return E_NOTIMPL;}
};

struct Snapshot {
    int before=123, axes[8], between=456;
    unsigned char buttons[128], buttonEnd=78;
    int pov[4], after=789;
    void seed(){for(auto& a:axes)a=99;for(auto& b:buttons)b=1;for(auto& p:pov)p=9000;}
    void cleared(){for(int a:axes)assert(a==0);for(auto b:buttons)assert(b==0);for(int p:pov)assert(p==-1);guards();}
    void guards(){assert(before==123 && between==456 && buttonEnd==78 && after==789);}
    int read(int slot=0){return ReadDeviceStateWithPov(slot,axes,8,buttons,128,pov,4);}
};

int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG","0");
    FakeInput input;FakeDevice wheel,replacement,reader;Snapshot out;
    g_di=&input;g_device=&wheel;g_activeGuid.Data1=1;
    g_auxCount=1;g_aux[0].guid=g_activeGuid;
    assert(OpenReadDevice(0)==0 && g_read[0].isWheel && !g_read[0].dev);
    assert(input.creates==0 && wheel.acquires==0); // Reuse FFB handle, no second device.
    wheel.state.lX=0;wheel.state.lY=65535;wheel.state.lZ=300;
    wheel.state.lRx=400;wheel.state.lRy=500;wheel.state.lRz=600;
    wheel.state.rglSlider[0]=700;wheel.state.rglSlider[1]=800;
    wheel.state.rgbButtons[0]=0x80;wheel.state.rgbButtons[1]=0x7F;wheel.state.rgbButtons[127]=0xFF;
    for(int offset:{0,4500}) {
        for(int i=0;i<4;++i)wheel.state.rgdwPOV[i]=i*9000+offset;
        out.seed();int reads=wheel.reads,polls=wheel.polls;
        assert(out.read()==1 && wheel.reads==reads+1 && wheel.polls==polls+1);
        const int expected[]={0,65535,300,400,500,600,700,800};
        for(int i=0;i<8;++i)assert(out.axes[i]==expected[i]);
        for(int i=0;i<128;++i)assert(out.buttons[i]==(i==0||i==127?1:0));
        for(int i=0;i<4;++i)assert(out.pov[i]==i*9000+offset);
        out.guards();
    }
    const DWORD neutral[]={0xFFFFFFFF,0x0000FFFF,0x1234FFFF,36000};
    memcpy(wheel.state.rgdwPOV,neutral,sizeof(neutral));
    assert(out.read());for(int p:out.pov)assert(p==-1);
    const DWORD boundaries[]={35999,0x80000000,0xFFFF0000,1};
    memcpy(wheel.state.rgdwPOV,boundaries,sizeof(boundaries));
    assert(out.read() && out.pov[0]==35999 && out.pov[1]==-1 && out.pov[2]==-1 && out.pov[3]==1);
    wheel.omitPov=true;assert(out.read());for(int p:out.pov)assert(p==-1);wheel.omitPov=false;
    // Existing Woden managed wrapper uses a different, preserved ABI: 160
    // digital buttons and GetInputCapabilities, not raw POV output.
    assert(GetInputCapabilities()==1);
    unsigned char digital[161];
    for(int hat=0;hat<4;++hat)for(int direction=0;direction<8;++direction) {
        for(auto& angle:wheel.state.rgdwPOV)angle=0xFFFFFFFF;
        wheel.state.rgdwPOV[hat]=direction*4500;
        memset(digital,42,sizeof(digital));int reads=wheel.reads,polls=wheel.polls;
        assert(ReadDeviceState(0,out.axes,digital,160)==1);
        assert(wheel.reads==reads+1 && wheel.polls==polls+1 && wheel.acquires==0);
        for(int i=0;i<128;++i)assert(digital[i]==(i==0||i==127?1:0));
        for(int i=128;i<160;++i)assert(digital[i]==(i==128+hat*8+direction));
        assert(digital[160]==42);
    }
    for(DWORD angle:{0xFFFFFFFFUL,0xFFFFUL,0x1234FFFFUL,36000UL}) {
        for(auto& p:wheel.state.rgdwPOV)p=angle;
        assert(ReadDeviceState(0,out.axes,digital,160));
        for(int i=128;i<160;++i)assert(digital[i]==0);
    }
    wheel.omitPov=true;assert(ReadDeviceState(0,out.axes,digital,160));
    for(int i=128;i<160;++i)assert(digital[i]==0);wheel.omitPov=false;
    for(int count:{-1,0,1,127,128,129,159,160,161,INT_MAX}) {
        memset(digital,42,sizeof(digital));assert(ReadDeviceState(0,out.axes,digital,count));
        int written=count<0?0:count>160?160:count;
        for(int i=written;i<161;++i)assert(digital[i]==42);
    }
    assert(ReadDeviceState(0,out.axes,nullptr,160));
    wheel.readResult=E_FAIL;memset(digital,42,sizeof(digital));
    assert(!ReadDeviceState(0,out.axes,digital,160));
    for(auto button:digital)assert(button==42);wheel.readResult=S_OK;
    wheel.state.rgdwPOV[0]=2249;assert(ReadDeviceState(0,out.axes,digital,160));
    assert(digital[128]==1 && digital[129]==0);
    wheel.state.rgdwPOV[0]=2250;assert(ReadDeviceState(0,out.axes,digital,160));
    assert(digital[128]==0 && digital[129]==1);
    wheel.state.rgdwPOV[0]=35999;assert(ReadDeviceState(0,out.axes,digital,160));
    assert(digital[128]==1 && digital[135]==0);
    // Accepted optional-button extents write no bytes outside the declared count.
    assert(ReadDeviceStateWithPov(0,out.axes,8,nullptr,0,out.pov,4));
    out.seed();assert(ReadDeviceStateWithPov(0,out.axes,8,out.buttons,1,out.pov,4));
    assert(out.buttons[0]==1 && out.buttons[1]==1 && out.buttons[127]==1);out.guards();

    for(int slot:{-1,1,8,INT_MAX}){out.seed();assert(!out.read(slot));out.cleared();}
    const int readsBefore=wheel.reads;
    for(int count:{-1,0,7,9,INT_MAX}) {
        out.seed();assert(!ReadDeviceStateWithPov(0,out.axes,count,out.buttons,128,out.pov,4));
        for(int a:out.axes)assert(a==99);for(auto b:out.buttons)assert(b==0);for(int p:out.pov)assert(p==-1);out.guards();
    }
    for(int count:{-1,0,3,5,INT_MAX}) {
        out.seed();assert(!ReadDeviceStateWithPov(0,out.axes,8,out.buttons,128,out.pov,count));
        for(int p:out.pov)assert(p==9000);for(int a:out.axes)assert(a==0);for(auto b:out.buttons)assert(b==0);out.guards();
    }
    for(int count:{-1,129,INT_MAX}) {
        out.seed();assert(!ReadDeviceStateWithPov(0,out.axes,8,out.buttons,count,out.pov,4));
        for(auto b:out.buttons)assert(b==1);for(int a:out.axes)assert(a==0);for(int p:out.pov)assert(p==-1);out.guards();
    }
    assert(!ReadDeviceStateWithPov(0,nullptr,8,out.buttons,128,out.pov,4));
    assert(!ReadDeviceStateWithPov(0,out.axes,8,nullptr,1,out.pov,4));
    assert(!ReadDeviceStateWithPov(0,out.axes,8,out.buttons,128,nullptr,4));
    assert(wheel.reads==readsBefore);

    // Failed Poll alone may still yield a valid state. Neither it nor a failed
    // GetDeviceState ever reacquires the shared FFB wheel or retries its read.
    wheel.pollResult=DIERR_INPUTLOST;assert(out.read() && wheel.acquires==0);
    for(HRESULT error:{(HRESULT)E_FAIL,(HRESULT)DIERR_INPUTLOST,(HRESULT)DIERR_NOTACQUIRED}) {
        wheel.readResult=error;int reads=wheel.reads;out.seed();
        assert(!out.read() && wheel.reads==reads+1 && wheel.acquires==0);out.cleared();
        // Legacy export retains its historical untouched-on-failure behavior.
        out.seed();assert(!ReadDeviceState(0,out.axes,out.buttons,128));
        assert(out.axes[0]==99 && out.buttons[0]==1 && out.pov[0]==9000);
    }
    wheel.readResult=S_OK;wheel.pollResult=S_OK;
    // Switching FFB device cannot retarget an existing slot; same GUID resumes.
    int oldReads=wheel.reads;g_device=&replacement;g_activeGuid.Data1=2;
    out.seed();assert(!out.read());out.cleared();assert(wheel.reads==oldReads && replacement.reads==0);
    g_device=nullptr;out.seed();assert(!out.read());out.cleared();
    g_device=&wheel;g_activeGuid.Data1=1;assert(out.read());
    CloseReadDevices();assert(!wheel.releases && !wheel.unacquires);
    out.seed();assert(!out.read());out.cleared();

    // Independent reader still uses the same owner DirectInput instance and
    // retains the existing non-FFB reacquisition policy, one state call only.
    g_readCount=1;g_read[0].isWheel=false;g_read[0].dev=&reader;
    reader.state=wheel.state;reader.pollResult=DIERR_INPUTLOST;
    assert(out.read() && reader.polls==2 && reader.reads==1 && reader.acquires==1);
    reader.readResult=DIERR_NOTACQUIRED;out.seed();
    assert(!out.read() && reader.reads==2 && reader.acquires==3);out.cleared();
    CloseReadDevices();assert(reader.releases==1 && reader.unacquires==1 && input.creates==0);
    g_di=nullptr;g_device=nullptr;g_auxCount=0;

    // Production FFB recovery routes, with actual exported calls and fake COM.
    assert(EnsureEffectLock());
    FakeDevice ffb;FakeForceEffect periodic,condition;
    g_device=&ffb;g_effect=&ffb.force;g_effectAxes=1;g_panicked=false;
    g_periodic[0].used=true;g_periodic[0].durationMs=0;g_periodic[0].fx=&periodic;
    g_periodic[0].started=true;g_periodic[0].lastMag=1000;g_periodic[0].lastPeriod=40000;
    g_condition[0].used=true;g_condition[0].fx=&condition;
    g_condition[0].started=true;g_condition[0].lastCoeff=1000;
    ffb.acquireResult=S_FALSE;Aquire();
    assert(g_periodic[0].started && g_condition[0].started);
    ffb.acquireResult=S_OK;Aquire();
    assert(!g_periodic[0].started && !g_condition[0].started);
    g_periodic[0].started=true;g_condition[0].started=true;
    ffb.force.firstSet=DIERR_INPUTLOST;
    assert(SetDeviceForcesXY(100,0)==1 && ffb.unacquires==1 && ffb.force.sets==2);
    assert(!g_periodic[0].started && !g_condition[0].started);
    g_periodic[0].started=true;g_condition[0].started=true;
    periodic.firstSet=DIERR_NOTACQUIRED;g_periodicStrategy=0;
    assert(UpdatePeriodicEffect(0,1800,25000)==1 && periodic.sets==2);
    assert((periodic.lastFlags&DIEP_START) && g_periodic[0].started && !g_condition[0].started);
    g_condition[0].started=true;g_periodic[0].started=true;
    condition.firstSet=DIERR_NOTEXCLUSIVEACQUIRED;
    assert(UpdateConditionEffect(0,1800,1000,0,0)==1 && condition.sets==2);
    assert((condition.lastFlags&DIEP_START) && g_condition[0].started && !g_periodic[0].started);
    g_condition[0].started=true;g_periodic[0].started=true;
    ffb.force.firstStart=DIERR_INPUTLOST;
    assert(StartEffect() && ffb.force.starts==2 && !g_condition[0].started && !g_periodic[0].started);
    // Finite slots are never marked for automatic replay during reacquire.
    g_periodic[1].used=true;g_periodic[1].durationMs=50;g_periodic[1].started=true;
    Aquire();assert(g_periodic[1].started);
    g_effect=nullptr;g_device=nullptr;
    g_periodic[0]={};g_periodic[1]={};g_condition[0]={};

    // The real open/reopen routine must fail closed at every newly guarded
    // stage; actuator-on only follows an acquired, created effect.
    FakeInput opener;g_di=&opener;g_hwnd=(HWND)1;
    g_candidates[0].guid=GUID_ConstantForce;g_candidates[0].axes=1;
    strcpy_s(g_candidates[0].name,"Fake wheel");
    FakeDevice badFormat;badFormat.formatResult=E_FAIL;opener.candidate=&badFormat;
    assert(!OpenCandidate(0) && !g_device && badFormat.acquires==0 && !badFormat.actuatorOns);
    FakeDevice badAcquire;badAcquire.acquireResult=DIERR_NOTACQUIRED;opener.candidate=&badAcquire;
    assert(!OpenCandidate(0) && !g_device && badAcquire.forceCreates==0 && !badAcquire.actuatorOns);
    FakeDevice badActuator;badActuator.commandResult=E_FAIL;opener.candidate=&badActuator;
    assert(!OpenCandidate(0) && !g_device && badActuator.forceCreates==1 && badActuator.actuatorOns==1);
    FakeDevice good;opener.candidate=&good;
    assert(OpenCandidate(0) && g_device==&good && good.forceCreates==1 && good.actuatorOns==1);
    CloseCandidate();assert(!g_device && good.releases==1);
    g_di=nullptr;
    puts("PASS production legacy 160-button/coherent POV and FFB acquire/recovery/open routes with fake COM; no hardware opened");
    return 0; // read_snapshot_test includes this file with main renamed
}
