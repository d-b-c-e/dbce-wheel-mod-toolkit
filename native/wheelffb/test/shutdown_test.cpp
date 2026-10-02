// Compile the production adapter into this test: no native device is ever opened.
#ifdef WHEEL_LEGACY_SHUTDOWN_BASELINE
#include "legacy/WheelFfb.cpp"
#else
#include "../WheelFfb.cpp"
#endif
#include <cassert>
#include <initializer_list>
#include <type_traits>
#pragma warning(push)
#pragma warning(disable:4505) // Header-only loader helpers are not executed in this fake fixture.
#include "../include/wheelffb.h"
#pragma warning(pop)
#define CHECK_SIGNATURE(ret, name, args) static_assert(std::is_same<decltype(&name), ret (__cdecl*) args>::value, #name " ABI changed");
WHEELFFB_API_LIST(CHECK_SIGNATURE)
#undef CHECK_SIGNATURE
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
struct ShutdownContext {
    HANDLE outputStarted=nullptr, outputReturned=nullptr;
    HANDLE ready=nullptr, mayExit=nullptr, freeStarted=nullptr, freeReturned=nullptr;
    volatile LONG workerAlive=0, earlyRelease=0, sawStop=0;
};
static unsigned shutdownChecks=0;
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
    ++shutdownChecks;
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
#ifndef WHEEL_LEGACY_SHUTDOWN_BASELINE
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
static void CheckZeroAndTimeoutFamilies() {
    InitializeCriticalSection(&g_lock); g_lockInit=true;
    FakeEffect constant, periodic, condition;
    g_effect=&constant; g_periodic[0].fx=&periodic; g_periodic[0].used=true;
    g_periodic[0].started=true; g_periodic[0].lastMag=100;
    g_condition[0].fx=&condition; g_condition[0].used=true;
    g_condition[0].started=true; g_condition[0].lastCoeff=100;
    g_lastForce=100; g_lastUpdateTick=100; g_holdMs=150;
    ShutdownRequire(!ApplyHoldTimeout(200),"fresh commands retained");
    ShutdownRequire(ApplyHoldTimeout(251),"stale commands silenced");
    ShutdownRequire(constant.stops>0 && periodic.stops>0 && condition.stops>0,"all families stopped");
    ShutdownRequire(constant.releases==0 && periodic.releases==0 && condition.releases==0,"zero does not release effects");
    ShutdownRequire(!g_periodic[0].started && !g_condition[0].started && g_lastForce==0,"resume requires a new explicit update");
    ShutdownRequire(!ApplyHoldTimeout(1000),"zero is idempotent");
    FreeDirectInput(); DeleteCriticalSection(&g_lock); g_lockInit=false;
    std::puts("all-family watchdog zero, no-release and restart-state checks passed; fake effects only");
}
static void CheckLowDemandRestart() {
    InitializeCriticalSection(&g_lock); g_lockInit=true;
    FakeEffect periodic,condition;
    // Sentinel is never dereferenced: these retained fake slots need no device methods.
    g_device=reinterpret_cast<LPDIRECTINPUTDEVICE8>(1);
    g_periodic[0].fx=&periodic;g_periodic[0].used=true;g_periodic[0].started=true;
    g_periodic[0].lastMag=100;g_periodic[0].lastPeriod=40000;
    g_condition[0].fx=&condition;g_condition[0].used=true;g_condition[0].started=true;g_condition[0].lastCoeff=100;
    for(int demand : {1,100,499,500}) {
        ZeroForces(); int psets=periodic.sets,csets=condition.sets;
        ShutdownRequire(UpdatePeriodicEffect(0,demand,25000)==1 && g_periodic[0].started && periodic.sets==psets+1 &&
            (periodic.flags&DIEP_START),"stopped periodic low demand explicitly restarts");
        ShutdownRequire(UpdateConditionEffect(0,demand,0,0,0)==1 && g_condition[0].started && condition.sets==csets+1 &&
            (condition.flags&DIEP_START),"stopped condition low demand explicitly restarts");
        psets=periodic.sets;csets=condition.sets;
        ShutdownRequire(UpdatePeriodicEffect(0,demand,25000)==1 && periodic.sets==psets,"running periodic unchanged filter retained");
        ShutdownRequire(UpdateConditionEffect(0,demand,0,0,0)==1 && condition.sets==csets,"running condition unchanged filter retained");
    }
    g_device=nullptr;FreeDirectInput();DeleteCriticalSection(&g_lock);g_lockInit=false;
    std::puts("low-demand restart 1/100/499/500 and running unchanged filters passed; fake effects only");
}
static void CheckEarlyWatchdogReadiness() {
    static_assert(std::is_same<decltype(g_lockInit),std::atomic<bool>>::value,"readiness must be synchronized");
    ShutdownRequire(!g_lockInit,"readiness initially false");
    SetHoldTimeoutMs(1);ShutdownRequire(g_holdThread!=nullptr,"early watchdog starts without a device");
    Sleep(80); // Allow a worker read while readiness is false.
    InitializeCriticalSection(&g_lock);g_lockInit=true;
    Sleep(80); // Allow a worker read after synchronized publication.
    FreeDirectInput();ShutdownRequire(g_holdThread==nullptr,"early watchdog joined before lock destruction");
    g_lockInit=false;DeleteCriticalSection(&g_lock);
    std::puts("early watchdog readiness publication and joined teardown passed; no device initialization");
}
#endif
int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG", "0");
    if (!CheckDelayedWorkerShutdown()) return 1;
#ifndef WHEEL_LEGACY_SHUTDOWN_BASELINE
    CheckSharedWatchdogState();
    CheckZeroAndTimeoutFamilies();
    CheckLowDemandRestart();
    CheckEarlyWatchdogReadiness();
    ShutdownRequire(GetInputCapabilities()==1 && GetWheelFfbVersion()==502,"HAT capability and distinct native version");
#endif
    printf("PASS HAT39 shutdown: %u explicit checks plus 39 compile-time Cdecl signatures; no devices acquired\n",shutdownChecks);
}
