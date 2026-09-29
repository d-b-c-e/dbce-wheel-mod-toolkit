// Compile the production adapter into this test: no native device is ever opened.
#include <windows.h>
static DWORD testForegroundPid = GetCurrentProcessId();
static HWND TestForegroundWindow() { return (HWND)1; }
static DWORD TestWindowProcess(HWND, DWORD* pid) { *pid=testForegroundPid; return 1; }
#define GetForegroundWindow TestForegroundWindow
#define GetWindowThreadProcessId TestWindowProcess
#include "../WheelFfb.cpp"
#undef GetForegroundWindow
#undef GetWindowThreadProcessId
#include <cassert>
#include <initializer_list>
struct FakeEffect : IDirectInputEffect {
    int starts=0, stops=0, releases=0, sets=0; DWORD duration=0, magnitude=0, flags=0;
    ULONGLONG now=0, deadline=0; bool active=false;
    HRESULT setResult=S_OK, startResult=S_OK, stopResult=S_OK, readResult=S_OK;
    DIPERIODIC savedPeriodic={}; DIENVELOPE savedEnvelope={}; bool hasEnvelope=false, mismatch=false, sineOnly=false;
    bool playing() const { return active && now < deadline; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {releases++; active=false; return 0;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID) override {return S_OK;}
    HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT e,DWORD f) override {
        assert(f==(DIEP_DURATION|DIEP_TYPESPECIFICPARAMS|DIEP_ENVELOPE));
        e->dwDuration=duration; *(DIPERIODIC*)e->lpvTypeSpecificParams=savedPeriodic;
        if(hasEnvelope) *e->lpEnvelope=savedEnvelope; else e->lpEnvelope=nullptr;
        if(mismatch) ((DIPERIODIC*)e->lpvTypeSpecificParams)->dwPhase++;
        return readResult;
    }
    HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT e,DWORD f) override {
        if (sineOnly && (f & DIEP_ENVELOPE)) return DIERR_UNSUPPORTED;
        sets++; flags=f; duration=e->dwDuration;
        savedPeriodic=*(DIPERIODIC*)e->lpvTypeSpecificParams;
        hasEnvelope=e->lpEnvelope!=nullptr; if(hasEnvelope) savedEnvelope=*e->lpEnvelope;
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
    // Shaped production helper verifies copied phase/envelope before a single non-solo start.
    FakeDevice shaped; LPDIRECTINPUTEFFECT shapedFx=nullptr;
    wheel_burst::create(&shaped,shapedFx,25,120);
    assert(wheel_burst::play(shapedFx,120,1952,6250,9000,120)==S_OK);
    assert(shaped.effect.hasEnvelope && shaped.effect.savedPeriodic.dwPhase==9000 && shaped.effect.savedPeriodic.dwPeriod==160000);
    assert(shaped.effect.savedEnvelope.dwAttackLevel==1952 && shaped.effect.savedEnvelope.dwAttackTime==0);
    assert(shaped.effect.savedEnvelope.dwFadeLevel==0 && shaped.effect.savedEnvelope.dwFadeTime==120000);
    shaped.effect.now=119999; assert(shaped.effect.playing()); shaped.effect.now++; assert(!shaped.effect.playing());
    assert(wheel_burst::play(shapedFx,120,500,25000,0,0,true)==S_OK);
    assert(!shaped.effect.hasEnvelope && shaped.effect.savedPeriodic.dwPhase==0 && shaped.effect.savedPeriodic.dwPeriod==40000);
    for(int stage=0;stage<4;stage++) for(HRESULT code : {(HRESULT)E_FAIL,(HRESULT)DI_TRUNCATED}) {
        FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr; wheel_burst::create(&d,fx,25,120);
        if(stage==0)d.effect.setResult=code; if(stage==1)d.effect.readResult=code;
        if(stage==2)d.effect.startResult=code; if(stage==3)d.effect.mismatch=true;
        assert(wheel_burst::play(fx,120,2000,6250,9000,120)!=S_OK && !fx && d.effect.releases==1);
        assert(d.effect.starts==(stage==2?1:0));
    }
    FakeDevice sineOnly; sineOnly.effect.sineOnly=true; LPDIRECTINPUTEFFECT sineFx=nullptr;
    wheel_burst::create(&sineOnly,sineFx,25,120);
    assert(wheel_burst::play(sineFx,120,500,25000)==S_OK);
    assert(wheel_burst::play(sineFx,120,2000,6250,9000,120)==DIERR_UNSUPPORTED && !sineFx);
    wheel_burst::create(&sineOnly,sineFx,25,120);
    assert(wheel_burst::play(sineFx,120,500,25000)==S_OK && !(sineOnly.effect.flags&DIEP_ENVELOPE));
    // Exercise production watchdog and window procedure against fake effects.
    InitializeCriticalSection(&g_lock); g_lockInit=true;
    FakeEffect periodic,continuous,condition;
    g_periodic[0].used=true; g_periodic[0].durationMs=50; g_periodic[0].fx=&periodic;
    g_periodic[0].started=true; g_periodic[0].lastMag=20;
    g_periodic[1].used=true; g_periodic[1].durationMs=0; g_periodic[1].fx=&continuous;
    g_periodic[1].started=true; g_periodic[1].lastMag=20;
    g_condition[0].used=true; g_condition[0].fx=&condition; g_condition[0].started=true; g_condition[0].lastCoeff=10;
    MarkPersistentEffectsStopped();
    assert(!g_condition[0].started && g_condition[0].lastCoeff==10);
    assert(!g_periodic[1].started && g_periodic[1].lastMag==20);
    // Finite collisions are not persistent; reacquisition must not replay them.
    assert(g_periodic[0].started);
    g_condition[0].started=true;
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
    // Actual exported route: metadata survives rejection, same slot accepts legacy.
    FakeEffect exported; g_periodic[0].fx=&exported; g_periodic[0].durationMs=120;
    g_device=(LPDIRECTINPUTDEVICE8)1; // Never dereferenced: existing fake effect only.
    assert(PlayShapedPeriodicBurst(0,1952,6250,9000,120)==1);
    int sets=exported.sets;
    assert(PlayShapedPeriodicBurst(0,2000,6250,-1,120)==0);
    assert(PlayShapedPeriodicBurst(0,2000,6250,36000,120)==0);
    assert(PlayShapedPeriodicBurst(0,2000,6250,9000,0)==0);
    assert(PlayShapedPeriodicBurst(0,2000,6250,9000,121)==0 && exported.sets==sets);
    testForegroundPid=0; assert(PlayShapedPeriodicBurst(0,2000,6250,9000,120)==0);
    assert(PlayShapedPeriodicBurst(0,0,6250,9000,120)==1); testForegroundPid=GetCurrentProcessId();
    assert(g_periodic[0].envelopeApplied);
    assert(PlayPeriodicBurst(0,500,25000)==1 && (exported.flags&DIEP_ENVELOPE) && !exported.hasEnvelope && exported.savedPeriodic.dwPhase==0);
    exported.mismatch=true; assert(PlayShapedPeriodicBurst(0,2000,6250,9000,120)==0);
    assert(g_periodic[0].used && g_periodic[0].durationMs==120 && !g_periodic[0].fx && !g_periodic[0].started);
    assert(!g_periodic[0].envelopeApplied);
    FakeEffect recovered; recovered.sineOnly=true; g_periodic[0].fx=&recovered;
    assert(PlayPeriodicBurst(0,500,25000)==1 && !(recovered.flags&DIEP_ENVELOPE));
    g_device=nullptr;
    ReleasePeriodics(); ReleaseConditions(); DeleteCriticalSection(&g_lock); g_lockInit=false;
    std::puts("finite burst and production focus/watchdog fake-effect checks passed; no hardware acquired");
}


