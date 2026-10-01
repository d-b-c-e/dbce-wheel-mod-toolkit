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
    InitializeCriticalSection(&g_lock); g_lockInit=true;
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
    // Real production worker lifecycle with no devices/effects and an uninitialized lock.
    DeleteCriticalSection(&g_lock); g_lockInit=false;
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
    if (!CheckDelayedWorkerShutdown()) return 1;
    CheckSharedWatchdogState();
    std::puts("finite burst and production focus/watchdog fake-effect checks passed; no hardware acquired");
}
