// Compile the production adapter into this test: no native device is ever opened.
#include "../WheelFfb.cpp"
#include <cassert>
#include <initializer_list>
struct FakeEffect : IDirectInputEffect {
    int starts=0, stops=0, releases=0, sets=0; DWORD duration=0, magnitude=0, flags=0;
    ULONGLONG now=0, deadline=0; bool active=false;
    HRESULT setResult=S_OK, startResult=S_OK, stopResult=S_OK;
    bool playing() const { return active && now < deadline; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {releases++; active=false; return 0;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT,DWORD) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT e,DWORD f) override {
        sets++; flags=f; duration=e->dwDuration;
        magnitude=((DIPERIODIC*)e->lpvTypeSpecificParams)->dwMagnitude;
        return setResult;
    }
    HRESULT STDMETHODCALLTYPE Start(DWORD iterations,DWORD f) override {
        assert(iterations==1 && f==0); starts++;
        if (SUCCEEDED(startResult)) {active=true;deadline=now+duration;}
        return startResult;
    }
    HRESULT STDMETHODCALLTYPE Stop() override {stops++; if(SUCCEEDED(stopResult))active=false; return stopResult;}
    HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Download() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Unload() override {return S_OK;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return S_OK;}
};
struct FakeDevice {
    FakeEffect effect; int creates=0; HRESULT result=S_OK;
    HRESULT CreateEffect(REFGUID guid,LPCDIEFFECT e,LPDIRECTINPUTEFFECT* out,LPUNKNOWN) {
        creates++; assert(IsEqualGUID(guid,GUID_Sine)); assert(e->dwDuration>=1000 && e->dwDuration<=500000);
        assert(e->cAxes==1 && e->dwTriggerButton==DIEB_NOTRIGGER);
        assert(((DIPERIODIC*)e->lpvTypeSpecificParams)->dwMagnitude==0);
        effect.duration=e->dwDuration; *out=SUCCEEDED(result)?&effect:nullptr; return result;
    }
};
int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG", "0");
    for (int ms : {1,50,500}) {
        FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr;
        assert(SUCCEEDED(wheel_burst::create(&d,fx,25,ms)) && d.effect.starts==0 && !d.effect.playing());
        assert(SUCCEEDED(wheel_burst::play(fx,ms,1,25000)) && d.effect.magnitude==1);
        assert(d.effect.flags==(DIEP_DURATION|DIEP_TYPESPECIFICPARAMS) && d.effect.playing());
        d.effect.now=ms*1000-1; assert(d.effect.playing());
        d.effect.now++; assert(!d.effect.playing()); // No toolkit callback needed to expire.
        assert(SUCCEEDED(wheel_burst::play(fx,ms,20,25000)) && d.effect.starts==2);
        assert(SUCCEEDED(wheel_burst::play(fx,ms,20,25000)) && d.effect.starts==3); // Identical event restarts.
        assert(SUCCEEDED(wheel_burst::play(fx,ms,0,25000)) && !d.effect.playing() && d.effect.starts==3);
        assert(SUCCEEDED(wheel_burst::stop(fx)) && SUCCEEDED(wheel_burst::stop(fx)));
    }
    for(HRESULT error : {(HRESULT)DIERR_NOTACQUIRED,(HRESULT)DIERR_INPUTLOST,(HRESULT)DIERR_NOTEXCLUSIVEACQUIRED,(HRESULT)E_HANDLE,(HRESULT)DIERR_NOTDOWNLOADED,(HRESULT)E_FAIL}) {
        for(int step=0;step<3;step++) {
            FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr; wheel_burst::create(&d,fx,25,50);
            if(step==0)d.effect.stopResult=error; if(step==1)d.effect.setResult=error; if(step==2)d.effect.startResult=error;
            assert(wheel_burst::play(fx,50,20,25000)==error && fx==nullptr);
            assert(d.effect.starts==(step==2?1:0) && d.effect.releases==1 && !d.effect.playing());
            d.effect.now=999999; assert(!d.effect.playing()); // Dropped event never replays after recovery.
        }
    }
    FakeDevice unsupported; unsupported.result=DIERR_UNSUPPORTED; LPDIRECTINPUTEFFECT nofx=nullptr;
    assert(wheel_burst::create(&unsupported,nofx,25,50)==DIERR_UNSUPPORTED && nofx==nullptr);
    for(int ms : {0,501,-1}) assert(wheel_burst::create(&unsupported,nofx,25,ms)==E_INVALIDARG);
    for(int hz : {0,101,-1}) assert(wheel_burst::create(&unsupported,nofx,hz,50)==E_INVALIDARG);
    for(DWORD mag=1;mag<=500;mag++) assert(wheel_burst::needs_update(0,40000,true,mag,40000));
    assert(!wheel_burst::needs_update(1000,40000,true,1200,40000));
    assert(wheel_burst::needs_update(1000,40000,false,1000,40000));
    // Exercise production watchdog and window procedure against fake effects.
    InitializeCriticalSection(&g_lock); g_lockInit=true;
    FakeEffect periodic,condition;
    g_periodic[0].used=true; g_periodic[0].durationMs=50; g_periodic[0].fx=&periodic;
    g_periodic[0].started=true; g_periodic[0].lastMag=20;
    g_condition[0].used=true; g_condition[0].fx=&condition; g_condition[0].started=true; g_condition[0].lastCoeff=10;
    g_lastForce=0; g_lastUpdateTick=100; g_holdMs=100;
    assert(!ApplyHoldTimeout(200)); assert(ApplyHoldTimeout(201));
    assert(periodic.stops==1 && condition.stops==1 && !g_periodic[0].started && !g_condition[0].started);
    assert(!ApplyHoldTimeout(300));
    g_periodic[0].started=true;g_periodic[0].lastMag=1;
    GuardWndProc(nullptr,WM_ACTIVATEAPP,FALSE,0);
    assert(periodic.stops==2 && condition.stops==2 && g_periodic[0].lastMag==0);
    int starts=periodic.starts; GuardWndProc(nullptr,WM_ACTIVATEAPP,TRUE,0); assert(periodic.starts==starts);
    assert(StopPeriodicBurst(0)==1 && StopPeriodicBurst(0)==1);
    assert(StopPeriodicBurst(1)==0 && StopPeriodicBurst(-1)==0);
    ReleasePeriodics(); ReleaseConditions(); DeleteCriticalSection(&g_lock); g_lockInit=false;
    std::puts("finite burst and production focus/watchdog fake-effect checks passed; no hardware acquired");
}


