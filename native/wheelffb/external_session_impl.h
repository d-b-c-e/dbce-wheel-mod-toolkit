// Experimental external owner adapter. Included once, after legacy definitions.
// Lock order: device gate -> policy lock. Watchdog releases the policy lock
// before TRYING the device gate. No driver call ever holds the policy lock.
#pragma once
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")

static SRWLOCK g_externalPolicyLock=SRWLOCK_INIT;
static external_ffb::Policy g_external;
static HANDLE g_externalProcesses[3]={};
static HWND g_externalOwner=nullptr;
static WNDPROC g_externalOriginal=nullptr;
static HANDLE g_externalThread=nullptr;
static bool g_externalReady=false;
static uint64_t g_externalSerial=0;
static LPDIRECTINPUTEFFECT g_externalEffect=nullptr;
static DWORD g_externalAxes=0;
static bool g_externalAutocentreChanged=false;
static DWORD g_externalPriorAutocentre=DIPROPAUTOCENTER_OFF;
static bool g_externalCloseReaders=false,g_externalCloseAux=false;
static const wchar_t kExternalOwnerProperty[]=L"DbceWheel.ExternalOwner.0.10";
static char g_externalOwnerCookie;
struct ExternalPolicyGuard {
    ExternalPolicyGuard(){AcquireSRWLockExclusive(&g_externalPolicyLock);}
    ~ExternalPolicyGuard(){ReleaseSRWLockExclusive(&g_externalPolicyLock);}
};
static int64_t ExternalQpc(){LARGE_INTEGER v={};return QueryPerformanceCounter(&v)?v.QuadPart:-1;}
static int64_t ExternalFrequency(){LARGE_INTEGER v={};return QueryPerformanceFrequency(&v)?v.QuadPart:0;}
static bool ExternalWindowValid(HWND h) {
    DWORD pid=0;
    return h && IsWindow(h) && GetWindowThreadProcessId(h,&pid) && pid==GetCurrentProcessId() &&
        !(GetWindowLongPtrW(h,GWL_STYLE)&WS_CHILD) && GetAncestor(h,GA_ROOT)==h &&
        GetPropW(h,kExternalOwnerProperty)==&g_externalOwnerCookie;
}
static bool ExternalAlive(HANDLE h){return h && WaitForSingleObject(h,0)==WAIT_TIMEOUT;}
static DWORD ExternalForegroundPid(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid;}
static bool ExternalNonce(uint64_t* out){return BCryptGenRandom(nullptr,(PUCHAR)out,16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0 && (out[0]||out[1]);}
static bool ExternalPinProcess(const ExternalProcessV1& p,HANDLE* out) {
    *out=nullptr;
    // Never silently truncate a 64-bit wire field into a 32-bit HANDLE.
    if(!p.handle || p.handle>static_cast<uint64_t>(UINTPTR_MAX) || p.handle>=static_cast<uint64_t>(UINTPTR_MAX)-15)return false;
    HANDLE source=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(p.handle)),copy=nullptr;
    if(!DuplicateHandle(GetCurrentProcess(),source,GetCurrentProcess(),&copy,
        PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,0))return false;
    FILETIME creation={},exit={},kernel={},user={};
    bool ok=GetProcessId(copy)==p.pid && ExternalAlive(copy) && GetProcessTimes(copy,&creation,&exit,&kernel,&user);
    uint64_t created=(static_cast<uint64_t>(creation.dwHighDateTime)<<32)|creation.dwLowDateTime;
    if(!ok || created!=p.creationFileTime){CloseHandle(copy);return false;}
    *out=copy;return true;
}
static bool ExternalPathMatches(HANDLE target,const wchar_t* expected,unsigned chars) {
    wchar_t actual[32768];DWORD length=32768;
    return QueryFullProcessImageNameW(target,0,actual,&length) && length==chars &&
        CompareStringOrdinal(actual,length,expected,chars,TRUE)==CSTR_EQUAL;
}
static LRESULT CALLBACK ExternalWndProc(HWND,UINT,WPARAM,LPARAM);
static DWORD WINAPI ExternalWatchdog(LPVOID);
static bool ExternalInstallOwner(HWND h) {
    DWORD pid=0;DWORD thread=GetWindowThreadProcessId(h,&pid);
    if(!h || !IsWindow(h) || pid!=GetCurrentProcessId() || thread!=GetCurrentThreadId() ||
        GetAncestor(h,GA_ROOT)!=h || (GetWindowLongPtrW(h,GWL_STYLE)&WS_CHILD) || GetParent(h)==HWND_MESSAGE)return false;
    if(!SetPropW(h,kExternalOwnerProperty,&g_externalOwnerCookie))return false;
    SetLastError(0);
    WNDPROC prior=(WNDPROC)SetWindowLongPtrW(h,GWLP_WNDPROC,(LONG_PTR)ExternalWndProc);
    if(!prior){RemovePropW(h,kExternalOwnerProperty);return false;}
    g_externalOriginal=prior;return true;
}
static bool ExternalStartWatchdog() {
    // External owner is a dedicated process. Pin code until process exit: an
    // in-flight driver call/guard thread must never execute an unloaded DLL.
    HMODULE self=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&ExternalWatchdog),&self))return false;
    g_externalThread=CreateThread(nullptr,0,ExternalWatchdog,nullptr,0,nullptr);
    return g_externalThread!=nullptr;
}
// Narrow injection seam used by offline production-path tests. Defaults below
// are always the real implementations in the DLL; no exported override exists.
struct ExternalPlatform {
    int64_t (*now)()=ExternalQpc;
    int64_t (*frequency)()=ExternalFrequency;
    bool (*installOwner)(HWND)=ExternalInstallOwner;
    bool (*ownerValid)(HWND)=ExternalWindowValid;
    bool (*pinProcess)(const ExternalProcessV1&,HANDLE*)=ExternalPinProcess;
    bool (*pathMatches)(HANDLE,const wchar_t*,unsigned)=ExternalPathMatches;
    bool (*alive)(HANDLE)=ExternalAlive;
    DWORD (*foregroundPid)()=ExternalForegroundPid;
    bool (*nonce)(uint64_t*)=ExternalNonce;
    bool (*startWatchdog)()=ExternalStartWatchdog;
    BOOL (WINAPI *closeHandle)(HANDLE)=CloseHandle;
};
static ExternalPlatform g_externalPlatform;
static bool ExternalOwnerValid(){return g_externalPlatform.ownerValid(g_externalOwner);}
// Called only with policy lock. Nonblocking Win32 identity/window checks do not
// touch DirectInput and cannot race process-handle cleanup under the same lock.
static uint32_t ExternalReason(bool focus) {
    if(!ExternalOwnerValid())return EXT_REASON_WINDOW;
    for(int i=0;i<3;++i)if(g_externalProcesses[i] && !g_externalPlatform.alive(g_externalProcesses[i]))
        return i==0?EXT_REASON_TARGET_EXIT:i==1?EXT_REASON_CLIENT_EXIT:EXT_REASON_PRODUCER_EXIT;
    if(focus && g_externalPlatform.foregroundPid()!=g_external.config.target.pid)return EXT_REASON_FOCUS;
    return EXT_REASON_NONE;
}
static void ExternalObserveLocked() {
    if(g_external.status.session)g_external.observe(g_externalPlatform.now(),ExternalReason(g_external.active()));
}
static void ExternalFault(HRESULT hr) {
    ExternalPolicyGuard policy;g_external.revoke(EXT_REASON_DRIVER);
    g_external.status.state=EXT_FAULTED;g_external.status.driverHResult=hr;
    g_external.status.cancellation=EXT_CANCEL_REQUESTED;
}
static int64_t ExternalReadTime(){return g_externalPlatform.now();}
static void ExternalNoteRead(int slot,bool valid,int64_t started) {
    ExternalPolicyGuard policy;
    if(!g_external.status.session || g_external.status.state==EXT_SESSION_CLOSED)return;
    ExternalObserveLocked(); // A fresh read cannot resurrect an expired lease.
    if(slot<0 || slot>=g_readCount){g_external.wheelRead(false,g_externalPlatform.now());return;}
    if(memcmp(&g_read[slot].guid,g_external.config.wheelGuid,16)==0) {
        // A non-shared reader cannot attest health of the force-owned handle.
        int64_t now=g_externalPlatform.now(),end=0;
        g_external.wheelRead(valid && g_read[slot].isWheel && g_device &&
            IsEqualGUID(g_read[slot].guid,g_activeGuid) && started<=now &&
            g_external.deadline(started,50,end) && now<end,started);
    }
    ExternalObserveLocked();
}
static void ExternalInvalidateReads() {
    ExternalPolicyGuard policy;g_external.wheelRead(false,g_externalPlatform.now());
}
static void ExternalQueueInputClose(bool aux) {
    ExternalPolicyGuard policy;
    if(aux)g_externalCloseAux=true;
    else {g_externalCloseReaders=true;g_external.wheelRead(false,g_externalPlatform.now());}
}
static void ExternalCloseProcessesLocked() {
    for(auto& h:g_externalProcesses)if(h){g_externalPlatform.closeHandle(h);h=nullptr;}
}
// Device gate required. No policy lock is held during any COM call. A stop
// result is API evidence only: status always carries PHYSICAL_UNVERIFIED.
static void ExternalServiceLocked() {
    bool close=false,readers=false,aux=false,closed=false;uint32_t cancel=0;
    {ExternalPolicyGuard policy;ExternalObserveLocked();close=g_external.closePending;cancel=g_external.status.cancellation;
        closed=g_external.status.state==EXT_SESSION_CLOSED;
        readers=g_externalCloseReaders;aux=g_externalCloseAux;g_externalCloseReaders=g_externalCloseAux=false;}
    // Deferred reentrant reader closes cannot call the public guarded wrappers.
    if(readers){for(auto& slot:g_read)if(slot.dev){slot.dev->Unacquire();slot.dev->Release();slot.dev=nullptr;}g_readCount=0;}
    if(aux && g_auxDevice){g_auxDevice->Unacquire();g_auxDevice->Release();g_auxDevice=nullptr;}
    if(closed)return; // Never erase a completed Close's recorded cleanup failure.
    if(cancel!=EXT_CANCEL_REQUESTED && cancel!=EXT_CANCEL_API_FAILED)return;
    HRESULT first=S_OK;
    auto record=[&](HRESULT hr){if(FAILED(hr) && SUCCEEDED(first))first=hr;};
    if(g_externalEffect)record(g_externalEffect->Stop());
    // Entry forbids existing legacy effects. Defense in depth covers every
    // family if a future path accidentally retains one; never start any here.
    if(g_effect)record(g_effect->Stop());
    for(auto& s:g_periodic)if(s.fx)record(s.fx->Stop());
    for(auto& s:g_constantBurst)if(s.fx)record(s.fx->Stop());
    for(auto& s:g_condition)if(s.fx)record(s.fx->Stop());
    if(g_device) {
        record(g_device->SendForceFeedbackCommand(DISFFC_STOPALL));
        record(g_device->SendForceFeedbackCommand(DISFFC_SETACTUATORSOFF));
    }
    if(close) {
        // Shared slots become invalid before the handle is released. Other
        // input-only devices remain available with FFB Off.
        if(g_externalEffect){g_externalEffect->Release();g_externalEffect=nullptr;}
        if(g_device){
            HRESULT unacquire=g_device->Unacquire();record(unacquire);
            if(SUCCEEDED(unacquire) && g_externalAutocentreChanged) {
                DIPROPDWORD prior={};prior.diph.dwSize=sizeof(prior);prior.diph.dwHeaderSize=sizeof(DIPROPHEADER);
                prior.diph.dwHow=DIPH_DEVICE;prior.dwData=g_externalPriorAutocentre;
                record(g_device->SetProperty(DIPROP_AUTOCENTER,&prior.diph));
            }
            g_device->Release();g_device=nullptr;
        }
        g_externalAutocentreChanged=false;
        g_activeGuid={};g_activeAxes=g_activeButtons=0;g_activeName[0]=0;g_externalAxes=0;
        // Restore the captured OEM baseline last, only after Unacquire. It can
        // itself exert torque; API cancellation never means physical zero.
    }
    {ExternalPolicyGuard policy;
        if(FAILED(first))g_external.status.driverHResult=first;
        g_external.status.cancellation=SUCCEEDED(first)?EXT_CANCEL_API_DONE:EXT_CANCEL_API_FAILED;
        // Close can be requested while Stop is blocked. Completing the older
        // non-close cancellation must not erase that later cleanup request.
        if(!close && g_external.closePending)g_external.status.cancellation=EXT_CANCEL_REQUESTED;
        if(FAILED(first)){g_external.status.state=EXT_FAULTED;g_external.status.reason=EXT_REASON_DRIVER;}
        if(close){g_external.status.state=EXT_SESSION_CLOSED;g_external.hasWheelRead=false;
            g_external.closePending=false;ExternalCloseProcessesLocked();}
    }
}
static bool ExternalTryService() {
    if(!g_externalMode.load() || !TryEnterCriticalSection(&g_lock))return false;
    if(g_externalDeviceCalls.load()){LeaveCriticalSection(&g_lock);return false;}
    ++g_externalDeviceCalls;ExternalServiceLocked();--g_externalDeviceCalls;
    LeaveCriticalSection(&g_lock);return true;
}
static void ExternalLegacyCancel(bool close) {
    {ExternalPolicyGuard policy;g_external.revoke(EXT_REASON_LEGACY_CANCEL,close);}
    ExternalTryService();
}
static DWORD WINAPI ExternalWatchdog(LPVOID) {
    for(;;) {
        Sleep(5);
        {ExternalPolicyGuard policy;ExternalObserveLocked();}
        ExternalTryService();
    }
}
static LRESULT CALLBACK ExternalWndProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_CLOSE || msg==WM_QUERYENDSESSION || msg==WM_ENDSESSION || msg==WM_DESTROY || msg==WM_NCDESTROY) {
        {ExternalPolicyGuard policy;g_external.revoke(EXT_REASON_WINDOW,true);}
        ExternalTryService();
        // DefWindowProc handles WM_CLOSE by destroying the HWND. If a driver
        // is still on-stack, defer it; owner pumps/status-polls and reposts Close
        // after cleanup. The OS can still force end-session/destruction.
        if(msg==WM_CLOSE){ExternalPolicyGuard policy;if(g_external.closePending)return 0;}
    }
    if(msg==WM_ACTIVATEAPP) {
        {ExternalPolicyGuard policy;ExternalObserveLocked();}
        ExternalTryService(); // Target foreground is expected while driving.
    }
    WNDPROC prior=g_externalOriginal;
    if(msg==WM_NCDESTROY)RemovePropW(h,kExternalOwnerProperty);
    return prior?CallWindowProcW(prior,h,msg,wp,lp):DefWindowProcW(h,msg,wp,lp);
}
// Device gate required. Always re-observe after a potentially blocking call,
// including a successful call, because its authority may have expired inside it.
static bool ExternalSetupStep(HRESULT hr) {
    if(FAILED(hr)){ExternalFault(hr);return false;}
    ExternalPolicyGuard policy;ExternalObserveLocked();
    return g_external.status.state==EXT_PREPARED && !g_external.closePending;
}
static bool ExternalPulseStep(HRESULT hr,uint64_t generation,int durationMs=0) {
    if(hr!=S_OK){ExternalFault(hr);return false;}
    ExternalPolicyGuard policy;ExternalObserveLocked();
    int remaining=g_external.remainingMs(g_externalPlatform.now(),generation);
    if(remaining<=0 || remaining<durationMs) {
        if(g_external.active())g_external.revoke(EXT_REASON_SOURCE_STALE);
        return false;
    }
    return true;
}
struct ExternalTryDevice {
    bool held;
    ExternalTryDevice():held(TryEnterCriticalSection(&g_lock)!=FALSE){
        if(held && g_externalDeviceCalls.load()){LeaveCriticalSection(&g_lock);held=false;}
        if(held)++g_externalDeviceCalls;
    }
    ~ExternalTryDevice(){if(held){--g_externalDeviceCalls;LeaveCriticalSection(&g_lock);}}
};

extern "C" {
__declspec(dllexport) int ExternalFfbEnter(HWND ownerWindow) {
    // Serialize against legacy exports so none can pass its external-mode check
    // and then open legacy state while this latch flips. Enter never waits on g_lock.
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    // Enter must run on the window thread before concurrent toolkit use. Once
    // opted in, no shutdown function ever clears this process-lifetime latch.
    if(g_externalMode.load())return g_externalReady && ownerWindow==g_externalOwner && ExternalOwnerValid()?EXT_OK:EXT_WRONG_MODE;
    if(g_device || g_effect || g_auxDevice || g_readCount || g_holdThread || g_guardOrigProc)return EXT_BUSY;
    for(const auto& s:g_periodic)if(s.used || s.fx)return EXT_BUSY;
    for(const auto& s:g_constantBurst)if(s.used || s.fx)return EXT_BUSY;
    for(const auto& s:g_condition)if(s.used || s.fx)return EXT_BUSY;
    // The effect lock is module-lifetime (INIT_ONCE); publish it before the owner hook.
    if(!EnsureEffectLock())return EXT_UNAVAILABLE;
    int64_t frequency=g_externalPlatform.frequency(),check=0;
    if(!external_ffb::Policy::ticks(frequency,250,check) || !g_externalPlatform.installOwner(ownerWindow))return EXT_INVALID;
    g_externalOwner=ownerWindow;g_hwnd=ownerWindow;
    g_external.status.cbSize=sizeof(ExternalFfbStatusV1);g_external.status.version=1;
    g_external.status.flags=EXT_MODE_ENTERED|EXT_PHYSICAL_UNVERIFIED;g_external.status.qpcFrequency=frequency;
    g_externalMode.store(true);
    g_externalReady=g_externalPlatform.startWatchdog();
    return g_externalReady?EXT_OK:EXT_UNAVAILABLE;
}
__declspec(dllexport) int ExternalFfbOpen(const ExternalFfbConfigV1* config,const wchar_t* targetImagePath,
    unsigned int pathChars,unsigned long long* session) {
    if(session)*session=0;
    if(!g_externalMode.load() || !g_externalReady)return EXT_WRONG_MODE;
    if(!config)return EXT_INVALID;
    const ExternalFfbConfigV1 snapshot=*config;config=&snapshot;
    if(!session || !targetImagePath || !pathChars || pathChars>32767 ||
        !external_ffb::Policy::validConfig(*config) || config->target.pid==GetCurrentProcessId())return EXT_INVALID;
    for(unsigned i=0;i<pathChars;++i)if(!targetImagePath[i])return EXT_INVALID;
    // Numeric handle validation precedes platform conversion, including fakes.
    for(const auto* p:{&config->target,&config->client,&config->producer})
        if(p->handle>static_cast<uint64_t>(UINTPTR_MAX) || p->handle>=static_cast<uint64_t>(UINTPTR_MAX)-15)return EXT_INVALID;
    ExternalTryDevice device;if(!device.held)return EXT_BUSY;
    {ExternalPolicyGuard policy;
        if(g_device || g_readCount || g_auxDevice || g_externalCloseReaders || g_externalCloseAux ||
            (g_external.status.session && g_external.status.state!=EXT_SESSION_CLOSED))return EXT_BUSY;
        if(!ExternalOwnerValid())return EXT_INELIGIBLE;
    }
    HANDLE pinned[3]={};const ExternalProcessV1* processes[]={&config->target,&config->client,&config->producer};
    bool valid=true;for(int i=0;i<3;++i)if(!g_externalPlatform.pinProcess(*processes[i],&pinned[i])){valid=false;break;}
    if(valid)valid=g_externalPlatform.pathMatches(pinned[0],targetImagePath,pathChars);
    if(!valid){for(auto h:pinned)if(h)g_externalPlatform.closeHandle(h);return EXT_INVALID;}
    {ExternalPolicyGuard policy;
        g_external.prepare(*config,++g_externalSerial,g_externalPlatform.frequency());
        for(int i=0;i<3;++i)g_externalProcesses[i]=pinned[i];
        *session=g_external.status.session;ExternalObserveLocked();
    }
    // All device creation is strict GUID. No index/name/default fallback, no
    // second DirectInput instance, and no reacquisition after polling failure.
    GUID guid;memcpy(&guid,config->wheelGuid,16);
    HRESULT hr=S_OK;
    bool ok=ExternalSetupStep(EnsureDirectInput()?S_OK:E_FAIL);
    if(ok){hr=g_di->CreateDevice(guid,&g_device,nullptr);ok=ExternalSetupStep(SUCCEEDED(hr) && !g_device?E_FAIL:hr);}
    DIDEVCAPS caps={};caps.dwSize=sizeof(caps);
    DIDEVICEINSTANCE info={};info.dwSize=sizeof(info);
    if(ok)ok=ExternalSetupStep(g_device->GetDeviceInfo(&info));
    if(ok && (!IsEqualGUID(info.guidInstance,guid) || ContainsNoCase(info.tszProductName,"vjoy") ||
        ContainsNoCase(info.tszProductName,"vigem") || ContainsNoCase(info.tszProductName,"xoutput") ||
        ContainsNoCase(info.tszProductName,"vxbox") || ContainsNoCase(info.tszProductName,"virtual"))) {ExternalFault(DIERR_UNSUPPORTED);ok=false;}
    if(ok)ok=ExternalSetupStep(g_device->GetCapabilities(&caps));
    if(ok && (!(caps.dwFlags&DIDC_FORCEFEEDBACK) || !caps.dwAxes)){ExternalFault(DIERR_UNSUPPORTED);ok=false;}
    if(ok)ok=ExternalSetupStep(g_device->SetDataFormat(&c_dfDIJoystick2));
    if(ok)ok=ExternalSetupStep(g_device->SetCooperativeLevel(g_externalOwner,DISCL_EXCLUSIVE|DISCL_BACKGROUND));
    DIPROPDWORD autocentre={};autocentre.diph.dwSize=sizeof(autocentre);autocentre.diph.dwHeaderSize=sizeof(DIPROPHEADER);
    autocentre.diph.dwHow=DIPH_DEVICE;
    if(ok){hr=g_device->GetProperty(DIPROP_AUTOCENTER,&autocentre.diph);
        ok=ExternalSetupStep(hr==S_OK?S_OK:FAILED(hr)?hr:DIERR_UNSUPPORTED);}
    if(ok && autocentre.dwData!=DIPROPAUTOCENTER_OFF && autocentre.dwData!=DIPROPAUTOCENTER_ON){ExternalFault(DIERR_UNSUPPORTED);ok=false;}
    if(ok) {
        g_externalPriorAutocentre=autocentre.dwData;g_externalAutocentreChanged=true;
        autocentre.dwData=DIPROPAUTOCENTER_OFF;
        ok=ExternalSetupStep(g_device->SetProperty(DIPROP_AUTOCENTER,&autocentre.diph));
    }
    DIPROPRANGE range={};range.diph.dwSize=sizeof(range);range.diph.dwHeaderSize=sizeof(DIPROPHEADER);
    range.diph.dwHow=DIPH_DEVICE;range.lMin=0;range.lMax=65535;
    if(ok)ok=ExternalSetupStep(g_device->SetProperty(DIPROP_RANGE,&range.diph));
    if(ok)ok=ExternalSetupStep(g_device->Acquire());
    if(ok)ok=ExternalSetupStep(g_device->SendForceFeedbackCommand(DISFFC_STOPALL));
    if(ok)ok=ExternalSetupStep(g_device->SendForceFeedbackCommand(DISFFC_SETACTUATORSOFF));
    if(ok) {
        // One X axis avoids speculative driver retries. The same signed-
        // magnitude/absolute-direction encoding as legacy constant force.
        DWORD axes[1]={DIJOFS_X};LONG direction[1]={0};DICONSTANTFORCE constant={};
        DIEFFECT e={};e.dwSize=sizeof(e);e.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS;
        e.dwDuration=config->pulseMaxMs*1000;e.dwGain=10000;e.dwTriggerButton=DIEB_NOTRIGGER;
        e.cAxes=1;e.rgdwAxes=axes;e.rglDirection=direction;e.cbTypeSpecificParams=sizeof(constant);e.lpvTypeSpecificParams=&constant;
        hr=g_device->CreateEffect(GUID_ConstantForce,&e,&g_externalEffect,nullptr);
        ok=ExternalSetupStep(hr==S_OK && g_externalEffect?S_OK:FAILED(hr)?hr:E_FAIL);
    }
    if(ok) {
        g_activeGuid=guid;g_activeAxes=caps.dwAxes;g_activeButtons=caps.dwButtons;g_externalAxes=1;
        strncpy_s(g_activeName,sizeof(g_activeName),info.tszProductName,_TRUNCATE);return EXT_OK;
    }
    {ExternalPolicyGuard policy;g_external.revoke(EXT_REASON_DRIVER,true);}
    ExternalServiceLocked();return EXT_FAULT;
}
__declspec(dllexport) int ExternalFfbArm(unsigned long long session,ExternalFfbArmTicketV1* ticket) {
    if(!g_externalMode.load())return EXT_WRONG_MODE;
    if(!ticket || ticket->cbSize!=sizeof(*ticket) || ticket->version!=1)return EXT_INVALID;
    uint64_t nonce[2];if(!g_externalPlatform.nonce(nonce))return EXT_UNAVAILABLE;
    ExternalPolicyGuard policy;
    if(!session || session!=g_external.status.session)return EXT_WRONG_SESSION;
    ExternalObserveLocked();uint32_t reason=ExternalReason(true);
    if(reason){g_external.revoke(reason);return EXT_INELIGIBLE;}
    int result=g_external.arm(g_externalPlatform.now(),++g_externalSerial,nonce[0],nonce[1]);
    if(result==EXT_WAITING)*ticket=g_external.ticket;
    return result;
}
__declspec(dllexport) int ExternalFfbRenewClient(unsigned long long session,const ExternalFfbClientFrameV1* frame) {
    if(!g_externalMode.load())return EXT_WRONG_MODE;
    if(!frame)return EXT_INVALID;
    const ExternalFfbClientFrameV1 snapshot=*frame;frame=&snapshot;
    int result;
    {ExternalPolicyGuard policy;
        if(!session || session!=g_external.status.session)return EXT_WRONG_SESSION;
        ExternalObserveLocked();result=g_external.client(*frame,g_externalPlatform.now());
    }
    ExternalTryService();return result;
}
__declspec(dllexport) int ExternalFfbSubmitConstant(unsigned long long session,const ExternalFfbForceFrameV1* frame) {
    if(!g_externalMode.load())return EXT_WRONG_MODE;
    if(!frame)return EXT_INVALID;
    const ExternalFfbForceFrameV1 snapshot=*frame;frame=&snapshot;
    ExternalTryDevice device;if(!device.held)return EXT_BUSY;
    int result=EXT_STALE;uint64_t generation=0;int duration=0;
    {ExternalPolicyGuard policy;
        if(!session || session!=g_external.status.session)return EXT_WRONG_SESSION;
        ExternalObserveLocked();result=g_external.source(*frame,g_externalPlatform.now());
        generation=g_external.status.generation;
        if(result==EXT_OK)duration=g_external.remainingMs(g_externalPlatform.now(),generation);
    }
    if(result!=EXT_OK){ExternalServiceLocked();return result;}
    if(!g_externalEffect || !g_device || duration<=0){ExternalFault(E_FAIL);ExternalServiceLocked();return EXT_FAULT;}
    // One accepted source frame permits at most one finite pulse. Nothing
    // queues/replays it on a client refresh, retry, focus return or re-arm.
    bool ok=ExternalPulseStep(g_externalEffect->Stop(),generation);
    if(ok && frame->signedMagnitude==0)return EXT_CANCELED;
    LONG direction[1]={frame->signedMagnitude<0?-frame->signedMagnitude:frame->signedMagnitude};
    DICONSTANTFORCE constant={frame->signedMagnitude};DIEFFECT e={};e.dwSize=sizeof(e);
    e.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS;e.cAxes=g_externalAxes;e.rglDirection=direction;
    e.dwDuration=duration*1000;e.dwTriggerButton=DIEB_NOTRIGGER;e.dwStartDelay=0;
    e.cbTypeSpecificParams=sizeof(constant);e.lpvTypeSpecificParams=&constant;
    const DWORD schedule=DIEP_DURATION|DIEP_TRIGGERBUTTON|DIEP_STARTDELAY;
    if(ok)ok=ExternalPulseStep(g_externalEffect->SetParameters(&e,DIEP_DIRECTION|DIEP_TYPESPECIFICPARAMS|schedule),generation,duration);
    DIEFFECT read={};read.dwSize=sizeof(read);
    if(ok){HRESULT hr=g_externalEffect->GetParameters(&read,schedule);
        ok=ExternalPulseStep(hr==S_OK && (read.dwDuration!=e.dwDuration || read.dwTriggerButton!=DIEB_NOTRIGGER || read.dwStartDelay!=0)?DIERR_UNSUPPORTED:hr,generation,duration);}
    if(ok)ok=ExternalPulseStep(g_device->SendForceFeedbackCommand(DISFFC_SETACTUATORSON),generation,duration);
    // Recheck immediately before Start as well as after its return. No
    // reacquire, retry or stale pulse recreation on any failure.
    if(ok)ok=ExternalPulseStep(S_OK,generation,duration);
    if(ok) {
        {ExternalPolicyGuard policy;g_external.status.lastStartQpc=g_externalPlatform.now();}
        ok=ExternalPulseStep(g_externalEffect->Start(1,0),generation);
    }
    if(!ok){ExternalServiceLocked();ExternalPolicyGuard policy;
        return g_external.status.reason==EXT_REASON_DRIVER?EXT_FAULT:EXT_INELIGIBLE;}
    return EXT_APPLIED;
}
__declspec(dllexport) int ExternalFfbDisarm(unsigned long long session,unsigned int reason) {
    if(!g_externalMode.load())return EXT_WRONG_MODE;
    if(reason!=EXT_REASON_STOP && reason!=EXT_REASON_INPUT && reason!=EXT_REASON_SOURCE_INVALID && reason!=EXT_REASON_CLIENT_OFF)return EXT_INVALID;
    {ExternalPolicyGuard policy;if(!session || session!=g_external.status.session)return EXT_WRONG_SESSION;g_external.revoke(reason);}
    if(!ExternalTryService())return EXT_PENDING;
    ExternalPolicyGuard policy;return g_external.status.cancellation==EXT_CANCEL_API_DONE?EXT_CANCELED:EXT_FAULT;
}
__declspec(dllexport) int ExternalFfbClose(unsigned long long session) {
    if(!g_externalMode.load())return EXT_WRONG_MODE;
    {ExternalPolicyGuard policy;
        if(!session || session!=g_external.status.session)return EXT_WRONG_SESSION;
        if(g_external.status.state==EXT_SESSION_CLOSED)return g_external.status.cancellation==EXT_CANCEL_API_DONE?EXT_CLOSED:EXT_FAULT;
        g_external.revoke(EXT_REASON_CLOSE,true);
    }
    if(!ExternalTryService())return EXT_PENDING;
    ExternalPolicyGuard policy;
    if(g_external.status.state!=EXT_SESSION_CLOSED)return EXT_PENDING;
    return g_external.status.cancellation==EXT_CANCEL_API_DONE?EXT_CLOSED:EXT_FAULT;
}
__declspec(dllexport) int ExternalFfbGetStatus(unsigned long long session,ExternalFfbStatusV1* status) {
    if(!g_externalMode.load())return EXT_WRONG_MODE;
    if(!status || status->cbSize!=sizeof(*status) || status->version!=1)return EXT_INVALID;
    ExternalPolicyGuard policy;
    if(session!=g_external.status.session)return EXT_WRONG_SESSION;
    ExternalObserveLocked();*status=g_external.status;
    status->flags=EXT_MODE_ENTERED|EXT_PHYSICAL_UNVERIFIED;
    if(ExternalOwnerValid())status->flags|=EXT_OWNER_VALID;
    if(g_externalDeviceCalls.load())status->flags|=EXT_DRIVER_BUSY;
    if(g_external.closePending)status->flags|=EXT_CLOSE_PENDING;
    return EXT_OK; // Read-only: never waits behind driver cancellation.
}
} // extern C
