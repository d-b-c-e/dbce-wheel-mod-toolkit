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
    bool implicitRestart=false; int implicitStarts=0;
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
        if (implicitRestart && playing()) implicitStarts++;
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

struct ShutdownContext {
    HANDLE outputStarted=nullptr, outputReturned=nullptr;
    HANDLE ready=nullptr, mayExit=nullptr, freeStarted=nullptr, freeReturned=nullptr;
    volatile LONG workerAlive=0, earlyRelease=0, sawStop=0;
};
struct ShutdownEffect : FakeEffect {
    ShutdownContext* context;
    volatile LONG releaseCalls=0, stopCalls=0;
    explicit ShutdownEffect(ShutdownContext* value) : context(value) {}
    HRESULT STDMETHODCALLTYPE Stop() override { InterlockedIncrement(&stopCalls); return FakeEffect::Stop(); }
    ULONG STDMETHODCALLTYPE Release() override {
        if (InterlockedCompareExchange(&context->workerAlive,0,0))
            InterlockedExchange(&context->earlyRelease,1);
        InterlockedIncrement(&releaseCalls);
        return FakeEffect::Release();
    }
};
static void ShutdownRequire(bool passed, const char* message) {
    if (!passed) { std::fprintf(stderr,"shutdown fixture error: %s\n",message); std::exit(2); }
}
static DWORD WINAPI DelayedFakeWatchdog(LPVOID value) {
    ShutdownContext* context=static_cast<ShutdownContext*>(value);
    InterlockedExchange(&context->workerAlive,1);
    SetEvent(context->ready);
    WaitForSingleObject(context->mayExit,INFINITE);
    // A pending worker must be able to take the effect lock while shutdown joins.
    EnterCriticalSection(&g_lock);
    LeaveCriticalSection(&g_lock);
    InterlockedExchange(&context->sawStop,g_stopThreads?1:0);
    InterlockedExchange(&context->workerAlive,0);
    return 0;
}
static DWORD WINAPI FreeOnCallerThread(LPVOID value) {
    ShutdownContext* context=static_cast<ShutdownContext*>(value);
    SetEvent(context->freeStarted);
    FreeDirectInput();
    SetEvent(context->freeReturned);
    return 0;
}
static DWORD WINAPI OutputOnCallerThread(LPVOID value) {
    auto* context=static_cast<ShutdownContext*>(value);
    SetEvent(context->outputStarted);
    StartEffect();
    SetEvent(context->outputReturned);
    return 0;
}
static bool CheckDelayedWorkerShutdown() {
    assert(EnsureEffectLock());
    ShutdownContext context;
    context.outputStarted=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    context.outputReturned=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    context.ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    context.mayExit=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    context.freeStarted=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    context.freeReturned=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    ShutdownRequire(context.ready && context.mayExit && context.freeStarted && context.freeReturned,"events");
    ShutdownEffect constant(&context),periodic(&context),condition(&context);
    g_effect=&constant;
    g_periodic[0].used=true; g_periodic[0].fx=&periodic;
    g_condition[0].used=true; g_condition[0].fx=&condition;
    g_stopThreads=false;
    g_holdThread=CreateThread(nullptr,0,DelayedFakeWatchdog,&context,0,nullptr);
    ShutdownRequire(g_holdThread!=nullptr,"worker creation");
    HANDLE observer=nullptr;
    ShutdownRequire(DuplicateHandle(GetCurrentProcess(),g_holdThread,GetCurrentProcess(),&observer,
        0,FALSE,DUPLICATE_SAME_ACCESS)!=FALSE,"worker observer");
    ShutdownRequire(WaitForSingleObject(context.ready,5000)==WAIT_OBJECT_0,"worker ready");
    HANDLE caller=CreateThread(nullptr,0,FreeOnCallerThread,&context,0,nullptr);
    ShutdownRequire(caller!=nullptr,"caller creation");
    ShutdownRequire(WaitForSingleObject(context.freeStarted,5000)==WAIT_OBJECT_0,"free started");
    // Hold the worker beyond the old 500 ms limit. Never load a real device or DLL.
    bool returnedEarly=WaitForSingleObject(context.freeReturned,650)==WAIT_OBJECT_0;
    bool releasedEarly=InterlockedCompareExchange(&context.earlyRelease,0,0)!=0;
    bool stoppedBeforeExit=constant.stopCalls>0 && periodic.stopCalls>0 && condition.stopCalls>0;
    HANDLE output=CreateThread(nullptr,0,OutputOnCallerThread,&context,0,nullptr);
    ShutdownRequire(output && WaitForSingleObject(context.outputStarted,5000)==WAIT_OBJECT_0,"output caller");
    bool outputBlocked=WaitForSingleObject(context.outputReturned,100)==WAIT_TIMEOUT;
    SetEvent(context.mayExit);
    ShutdownRequire(WaitForSingleObject(observer,5000)==WAIT_OBJECT_0,"worker exit");
    ShutdownRequire(WaitForSingleObject(caller,5000)==WAIT_OBJECT_0,"free completion");
    ShutdownRequire(WaitForSingleObject(output,5000)==WAIT_OBJECT_0,"output resumed after cleanup");
    CloseHandle(output); CloseHandle(context.outputStarted); CloseHandle(context.outputReturned);
    bool releasedOnce=constant.releaseCalls==1 && periodic.releaseCalls==1 && condition.releaseCalls==1;
    bool stopped=context.sawStop==1 && g_holdThread==nullptr;
    FreeDirectInput(); // No worker: repeated cleanup must not release an effect twice.
    bool idempotent=constant.releaseCalls==1 && periodic.releaseCalls==1 && condition.releaseCalls==1;
    CloseHandle(observer); CloseHandle(caller);
    CloseHandle(context.ready); CloseHandle(context.mayExit);
    CloseHandle(context.freeStarted); CloseHandle(context.freeReturned);
    if (returnedEarly || releasedEarly || !stoppedBeforeExit || !outputBlocked || !releasedOnce || !stopped || !idempotent) {
        std::fprintf(stderr,"delayed-worker shutdown FAILED (requires effects stopped before exit): returnedEarly=%d releasedEarly=%d releasedOnce=%d stopped=%d idempotent=%d\n",
            returnedEarly,releasedEarly,releasedOnce,stopped,idempotent);
        return false;
    }
    // Real production worker lifecycle with no devices/effects and the lifetime lock.
    
    SetHoldTimeoutMs(1);
    ShutdownRequire(g_holdThread!=nullptr,"watchdog restart");
    FreeDirectInput();
    ShutdownRequire(g_holdThread==nullptr,"restarted worker joined");
    std::puts("stop-before-join, delayed-worker resource-order, repeated cleanup and watchdog restart passed; fake effects only");
    return true;
}
static DWORD WINAPI SharedStateWorker(LPVOID) {
    for (int i=0; i<100; ++i) {
        g_lastHr.store(E_FAIL);
        ApplyHoldTimeout(GetTickCount64());
        Log("fake shared-state worker %d",i);
    }
    return 0;
}
static void CheckSharedWatchdogState() {
    // Exercise the independent logger lock and atomic state without any device.
    { std::lock_guard<std::recursive_mutex> guard(g_logMutex);
      g_logging=true; g_logChecked=true; }
    SetLogPath("..\\build\\fake-watchdog-log-a.txt");
    HANDLE worker=CreateThread(nullptr,0,SharedStateWorker,nullptr,0,nullptr);
    ShutdownRequire(worker!=nullptr,"shared-state worker");
    for (int i=0; i<50; ++i) {
        SetHoldTimeoutMs(0);
        SetLogPath(i%2 ? "..\\build\\fake-watchdog-log-a.txt" : "..\\build\\fake-watchdog-log-b.txt");
        (void)GetLastHResult();
    }
    ShutdownRequire(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0,"shared-state worker joined");
    CloseHandle(worker);
    { std::lock_guard<std::recursive_mutex> guard(g_logMutex);
      g_logging=false; }
    std::puts("concurrent timeout/HRESULT/log-path exercise passed; fake worker only");
}
struct LockPublicationContext {
    HANDLE go=nullptr, readerReady=nullptr, publishedSeen=nullptr;
    std::atomic<bool> stop{false};
    std::atomic<int> initialFalseReads{0}, readyReads{0};
};
static DWORD WINAPI PublicationReader(LPVOID value) {
    auto* c=static_cast<LockPublicationContext*>(value);
    if (!g_lockInit.load(std::memory_order_acquire) && !ApplyHoldTimeout(GetTickCount64())) ++c->initialFalseReads;
    SetEvent(c->readerReady);
    WaitForSingleObject(c->go,INFINITE);
    while (!c->stop.load()) {
        ApplyHoldTimeout(GetTickCount64());
        if (g_lockInit.load(std::memory_order_acquire)) { ++c->readyReads; SetEvent(c->publishedSeen); }
        SwitchToThread();
    }
    return 0;
}
static DWORD WINAPI PublicationInitializer(LPVOID value) {
    auto* c=static_cast<LockPublicationContext*>(value);
    WaitForSingleObject(c->go,INFINITE);
    for (int i=0; i<100; ++i) {
        ShutdownRequire(EnsureEffectLock(),"concurrent lock initialization");
        ReleasePeriodicEffects(); ReleaseConditionEffects(); FreeDirectInput();
    }
    return 0;
}
static void CheckConcurrentLockPublication() {
    ShutdownRequire(!g_lockInit.load(),"fresh process lock not initialized");
    LockPublicationContext c;
    c.go=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    c.readerReady=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    c.publishedSeen=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    ShutdownRequire(c.go && c.readerReady && c.publishedSeen,"publication events");
    g_holdMs.store(1);
    HANDLE reader=CreateThread(nullptr,0,PublicationReader,&c,0,nullptr);
    ShutdownRequire(reader && WaitForSingleObject(c.readerReady,5000)==WAIT_OBJECT_0,"pre-init reader ready");
    HANDLE initializers[4]={};
    for (auto& thread:initializers) {
        thread=CreateThread(nullptr,0,PublicationInitializer,&c,0,nullptr);
        ShutdownRequire(thread!=nullptr,"initializer creation");
    }
    SetEvent(c.go);
    ShutdownRequire(WaitForMultipleObjects(4,initializers,TRUE,10000)==WAIT_OBJECT_0,"concurrent init/release completed");
    ShutdownRequire(WaitForSingleObject(c.publishedSeen,5000)==WAIT_OBJECT_0,"reader observed published readiness");
    c.stop.store(true);
    ShutdownRequire(WaitForSingleObject(reader,5000)==WAIT_OBJECT_0,"publication reader joined");
    ShutdownRequire(c.initialFalseReads==1 && c.readyReads>0 && g_lockInit.load(),"readiness publication observed");
    for(auto thread:initializers) CloseHandle(thread);
    CloseHandle(reader); CloseHandle(c.go); CloseHandle(c.readerReady); CloseHandle(c.publishedSeen);
    std::puts("concurrent first initialization/publication and 400 empty releases passed; no device initialization");
}
static void CheckPreInitWatchdog() {
    ShutdownRequire(!g_lockInit.load(),"pre-init fresh process");
    SetHoldTimeoutMs(0);
    ShutdownRequire(!g_lockInit.load() && !g_holdThread,"disabled timeout stays low demand");
    SetHoldTimeoutMs(1);
    ShutdownRequire(g_lockInit.load(std::memory_order_acquire) && g_holdThread,"lock published before pre-init worker");
    Sleep(60);
    FreeDirectInput(); FreeDirectInput();
    ShutdownRequire(!g_holdThread && g_lockInit.load(),"cleanup keeps lifetime lock");
    SetHoldTimeoutMs(1); FreeDirectInput();
    ShutdownRequire(!g_holdThread,"pre-init worker restart joined");
    std::puts("pre-device watchdog initialization, disabled low-demand path and restart passed; no devices");
}
int main(int argc, char** argv) {
    SetEnvironmentVariableA("DBCE_FFB_LOG", "0");
    if(argc==2 && std::strcmp(argv[1],"--pre-init-worker")==0) { CheckPreInitWatchdog(); return 0; }
    CheckConcurrentLockPublication();
    for (int ms : {1,50,500}) {
        FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr;
        assert(SUCCEEDED(wheel_burst::create(&d,fx,25,ms)) && d.effect.starts==0 && !d.effect.playing());
        d.effect.stopResult=DIERR_NOTDOWNLOADED; // Idle new handle on this driver.
        assert(SUCCEEDED(wheel_burst::play(fx,ms,1,25000)) && d.effect.magnitude==1);
        assert(d.effect.stops==1);
        d.effect.stopResult=S_OK;
        assert(d.effect.flags==(DIEP_DURATION|DIEP_TYPESPECIFICPARAMS) && d.effect.playing());
        d.effect.now=ms*1000-1; assert(d.effect.playing());
        d.effect.now++; assert(!d.effect.playing()); // No toolkit callback needed to expire.
        assert(SUCCEEDED(wheel_burst::play(fx,ms,20,25000)) && d.effect.starts==2);
        assert(SUCCEEDED(wheel_burst::play(fx,ms,20,25000)) && d.effect.starts==3); // Identical event restarts.
        assert(d.effect.stops==3); // Stop before each parameter update.
        assert(SUCCEEDED(wheel_burst::play(fx,ms,0,25000)) && !d.effect.playing() && d.effect.starts==3);
        assert(SUCCEEDED(wheel_burst::stop(fx)) && SUCCEEDED(wheel_burst::stop(fx)));
    }
    for(HRESULT error : {(HRESULT)DIERR_NOTACQUIRED,(HRESULT)DIERR_INPUTLOST,(HRESULT)DIERR_NOTEXCLUSIVEACQUIRED,(HRESULT)E_HANDLE,(HRESULT)DIERR_NOTDOWNLOADED,(HRESULT)E_FAIL}) {
        for(int step=0;step<2;step++) {
            FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr; wheel_burst::create(&d,fx,25,50);
            if(step==0)d.effect.setResult=error; if(step==1)d.effect.startResult=error;
            assert(wheel_burst::play(fx,50,20,25000)==error && fx==nullptr);
            assert(d.effect.starts==(step==1?1:0) && d.effect.releases==1 && !d.effect.playing());
            d.effect.now=999999; assert(!d.effect.playing()); // Dropped event never replays after recovery.
        }
    }
    for(HRESULT error : {(HRESULT)DIERR_NOTACQUIRED,(HRESULT)DIERR_INPUTLOST,(HRESULT)DIERR_NOTEXCLUSIVEACQUIRED,(HRESULT)E_HANDLE,(HRESULT)E_FAIL}) {
        FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr; wheel_burst::create(&d,fx,25,50);
        d.effect.stopResult=error;
        assert(wheel_burst::play(fx,50,20,25000)==error && fx==nullptr);
        assert(d.effect.sets==0 && d.effect.starts==0 && d.effect.releases==1);
    }
    {
        FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr; wheel_burst::create(&d,fx,25,50);
        d.effect.stopResult=DIERR_NOTDOWNLOADED;
        assert(wheel_burst::play(fx,50,20,25000)==S_OK && d.effect.stops==1 && d.effect.playing());
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
    {
        FakeDevice d; LPDIRECTINPUTEFFECT fx=nullptr; wheel_burst::create(&d,fx,25,120);
        assert(wheel_burst::play(fx,120,500,25000)==S_OK && d.effect.playing());
        d.effect.implicitRestart=true; d.effect.mismatch=true;
        assert(wheel_burst::play(fx,120,2000,6250,9000,120)==DIERR_UNSUPPORTED && !fx);
        assert(d.effect.implicitStarts==0 && d.effect.starts==1 && d.effect.releases==1);
    }
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
    assert(EnsureEffectLock());
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
    ReleasePeriodics(); ReleaseConditions();
    if (!CheckDelayedWorkerShutdown()) return 1;
    CheckSharedWatchdogState();
    std::puts("finite burst and production focus/watchdog fake-effect checks passed; no hardware acquired");
}
