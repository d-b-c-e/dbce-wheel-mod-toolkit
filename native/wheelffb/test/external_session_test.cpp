// Production exports, COM driver and platform replaced with deterministic fakes.
// No system DI, controller, HWND, game, virtual pad or physical output is used.
#include "../WheelFfb.cpp"
#include <cassert>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
static std::atomic<int64_t> fakeNow(1000);
static bool fakeOwner=true, fakePath=true;
static bool fakeAlive[3]={true,true,true};
static DWORD fakeFocus=42;
static GUID fakeGuid={1,2,3,{4,5,6,7,8,9,10,11}};
enum Stage {Info,Caps,Format,Coop,GetAuto,SetAuto,Range,AcquireDevice,UnacquireDevice,ReleaseDevice,
 PollDevice,ReadDevice,CreateFx,StopFx,SetFx,GetFx,StartFx,ReleaseFx,StopAll,ActOff,ActOn};
static std::function<HRESULT(Stage)> driverHook;
static std::vector<Stage> operations;
static HRESULT step(Stage s){operations.push_back(s);return driverHook?driverHook(s):S_OK;}
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);abort();}}while(0)
struct ExternalFakeEffect:IDirectInputEffect {
 int starts=0,stops=0,releases=0,sets=0;DWORD duration=0;LONG magnitude=0,direction=0;bool mismatch=false,badTrigger=false,badDelay=false;
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override{return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return 1;}
 ULONG STDMETHODCALLTYPE Release()override{++releases;step(ReleaseFx);return 1;}
 HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT e,DWORD flags)override{
  ++sets;assert(flags==(DIEP_DIRECTION|DIEP_TYPESPECIFICPARAMS|DIEP_DURATION|DIEP_TRIGGERBUTTON|DIEP_STARTDELAY));
  assert(e->dwTriggerButton==DIEB_NOTRIGGER && e->dwStartDelay==0);
  assert(e->cAxes==1 && e->dwDuration>0 && e->dwDuration<=50000);
  duration=e->dwDuration;magnitude=((DICONSTANTFORCE*)e->lpvTypeSpecificParams)->lMagnitude;direction=e->rglDirection[0];
  assert(direction==labs(magnitude));return step(SetFx);
 }
 HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT e,DWORD f)override{assert(f==(DIEP_DURATION|DIEP_TRIGGERBUTTON|DIEP_STARTDELAY));e->dwDuration=mismatch?INFINITE:duration;e->dwTriggerButton=badTrigger?0:DIEB_NOTRIGGER;e->dwStartDelay=badDelay?1000:0;return step(GetFx);}
 HRESULT STDMETHODCALLTYPE Start(DWORD iterations,DWORD flags)override{assert(iterations==1 && flags==0);++starts;return step(StartFx);}
 HRESULT STDMETHODCALLTYPE Stop()override{++stops;return step(StopFx);}
 HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE Download()override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE Unload()override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE)override{return E_NOTIMPL;}
};
static ExternalFakeEffect fakeEffect;
struct ExternalFakeDevice : IDirectInputDevice8A {
    DIJOYSTATE2 state = {};
    int polls=0, reads=0, acquires=0, releases=0, unacquires=0;
    HRESULT pollResult=S_OK, readResult=S_OK;
    bool omitPov=false; DWORD autocentre=1; bool acquired=false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;step(ReleaseDevice);return 1;}
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS c) override {c->dwFlags=DIDC_FORCEFEEDBACK;c->dwAxes=8;c->dwButtons=128;return step(Caps);}
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID,LPDIPROPHEADER p) override {((DIPROPDWORD*)p)->dwData=autocentre;return step(GetAuto);}
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID prop,LPCDIPROPHEADER p) override {if(&prop==&DIPROP_AUTOCENTER){assert(!acquired);autocentre=((DIPROPDWORD*)p)->dwData;return step(SetAuto);}return step(Range);}
    HRESULT STDMETHODCALLTYPE Acquire() override {++acquires;acquired=true;return step(AcquireDevice);}
    HRESULT STDMETHODCALLTYPE Unacquire() override {++unacquires;HRESULT hr=step(UnacquireDevice);if(SUCCEEDED(hr))acquired=false;return hr;}
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size,LPVOID data) override {
        assert(size==sizeof(state));++reads;
        // Deliberately write even on failure: production must not publish it.
        DWORD saved[4]; memcpy(saved,((DIJOYSTATE2*)data)->rgdwPOV,sizeof(saved));
        memcpy(data,&state,sizeof(state));
        if(omitPov)memcpy(((DIJOYSTATE2*)data)->rgdwPOV,saved,sizeof(saved));
        HRESULT hr=step(ReadDevice);return FAILED(hr)?hr:readResult;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override {return step(Format);}
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd,DWORD) override {assert(hwnd==(HWND)100);return step(Coop);}
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA i) override {i->guidInstance=fakeGuid;strcpy_s(i->tszProductName,"Fake physical wheel");return step(Info);}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID g,LPCDIEFFECT e,LPDIRECTINPUTEFFECT* out,LPUNKNOWN) override {assert(IsEqualGUID(g,GUID_ConstantForce) && e->dwDuration<=50000 && e->dwTriggerButton==DIEB_NOTRIGGER && e->cAxes==1 && ((DICONSTANTFORCE*)e->lpvTypeSpecificParams)->lMagnitude==0);fakeEffect.duration=e->dwDuration;*out=&fakeEffect;return step(CreateFx);}
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD cmd) override {assert(cmd==DISFFC_STOPALL || cmd==DISFFC_SETACTUATORSOFF || cmd==DISFFC_SETACTUATORSON);return step(cmd==DISFFC_STOPALL?StopAll:cmd==DISFFC_SETACTUATORSOFF?ActOff:ActOn);}
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Poll() override {++polls;HRESULT hr=step(PollDevice);return FAILED(hr)?hr:pollResult;}
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR,DWORD,LPDIFILEEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override {return E_NOTIMPL;}
};

static ExternalFakeDevice fakeWheel;
struct ExternalFakeInput : IDirectInput8A {
    int creates=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 1;}
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid,LPDIRECTINPUTDEVICE8A* out,LPUNKNOWN) override {++creates;assert(IsEqualGUID(guid,fakeGuid));*out=&fakeWheel;return S_OK;}
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD) override {assert(false);return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID,LPCSTR,LPGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR,LPDIACTIONFORMATA,LPDIENUMDEVICESBYSEMANTICSCBA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK,LPDICONFIGUREDEVICESPARAMSA,DWORD,LPVOID) override {return E_NOTIMPL;}
};

static int64_t fakeQpc(){return fakeNow.load();}
static int64_t fakeFrequency(){return 1000;}
static int forwardedMessages=0;
static LRESULT CALLBACK fakeOriginal(HWND,UINT,WPARAM,LPARAM){++forwardedMessages;return 123;}
static bool fakeInstall(HWND h){if(h!=(HWND)100 || !fakeOwner)return false;g_externalOriginal=fakeOriginal;return true;}
static bool fakeOwnerValid(HWND h){return h==(HWND)100 && fakeOwner;}
static bool fakePin(const ExternalProcessV1& p,HANDLE* h){if(p.creationFileTime!=99 || p.handle<1 || p.handle>3)return false;*h=(HANDLE)(uintptr_t)p.handle;return true;}
static bool fakePathMatches(HANDLE,const wchar_t* path,unsigned count){return fakePath && std::wstring(path,count)==L"C:\\Game.exe";}
static bool fakeProcessAlive(HANDLE h){return fakeAlive[(uintptr_t)h-1];}
static DWORD fakeForeground(){return fakeFocus;}
static bool fakeRandom(uint64_t* p){p[0]=123;p[1]=456;return true;}
static bool fakeThread(){return true;}
static BOOL WINAPI fakeClose(HANDLE){return TRUE;}
static ExternalFakeInput fakeInput;
static uint64_t session=0;
static ExternalFfbArmTicketV1 ticket={};
static ExternalFfbConfigV1 config() {
    ExternalFfbConfigV1 c={};c.cbSize=sizeof(c);c.version=1;
    c.target={1,99,42,0};c.client={2,99,43,0};c.producer={3,99,44,0};memcpy(c.wheelGuid,&fakeGuid,16);
    c.maxMagnitude=5000;c.sourceMaxAgeMs=150;c.clientMaxAgeMs=250;c.pulseMaxMs=20;return c;
}
static ExternalFfbStatusV1 status() {
    ExternalFfbStatusV1 s={};s.cbSize=sizeof(s);s.version=1;
    CHECK(ExternalFfbGetStatus(session,&s)==EXT_OK);CHECK(s.flags&EXT_PHYSICAL_UNVERIFIED);return s;
}
static void readWheel() {
    int axes[8],pov[4];unsigned char buttons[128];
    CHECK(ReadDeviceStateWithPov(0,axes,8,buttons,128,pov,4)==1);
}
static void reset(DWORD prior=1) {
    driverHook={};fakeOwner=fakePath=true;fakeFocus=42;for(auto& p:fakeAlive)p=true;
    if(session){CHECK(ExternalFfbClose(session)==EXT_CLOSED);CloseReadDevices();}
    fakeNow=1000;fakeWheel.acquired=false;fakeWheel.autocentre=prior;fakeWheel.pollResult=fakeWheel.readResult=S_OK;
    fakeEffect.mismatch=false;operations.clear();g_di=&fakeInput;
    auto c=config();CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&session)==EXT_OK);
    CHECK(fakeEffect.starts>=0 && fakeWheel.autocentre==0 && fakeWheel.acquired);
    g_auxCount=1;g_aux[0].guid=fakeGuid;
    int creates=fakeInput.creates;CHECK(OpenReadDevice(0)==0);CHECK(fakeInput.creates==creates && g_read[0].isWheel);
    readWheel();ticket={};ticket.cbSize=sizeof(ticket);ticket.version=1;
}
static void arm(){CHECK(ExternalFfbArm(session,&ticket)==EXT_WAITING);}
static ExternalFfbClientFrameV1 client(uint64_t seq=1) {
    ExternalFfbClientFrameV1 f={};f.cbSize=sizeof(f);f.flags=EXT_ALL_CLIENT_FLAGS;
    f.generation=ticket.generation;f.sequence=seq;f.capturedQpc=fakeNow;f.qpcFrequency=1000;return f;
}
static ExternalFfbForceFrameV1 source(uint64_t seq=1,int magnitude=2000) {
    ExternalFfbForceFrameV1 f={};f.cbSize=sizeof(f);f.attestations=EXT_ALL_ATTESTATIONS;
    f.generation=ticket.generation;f.nonceLow=ticket.nonceLow;f.nonceHigh=ticket.nonceHigh;
    f.sequence=seq;f.capturedQpc=fakeNow;f.qpcFrequency=1000;f.signedMagnitude=magnitude;return f;
}
static int renew(uint64_t seq=1){auto f=client(seq);return ExternalFfbRenewClient(session,&f);}
static int submit(uint64_t seq=1,int magnitude=2000){auto f=source(seq,magnitude);return ExternalFfbSubmitConstant(session,&f);}
static void ready(){reset();arm();CHECK(renew()==EXT_OK);CHECK(submit()==EXT_APPLIED);}
static void checkNoResume(){CHECK(renew(999)==EXT_STALE);CHECK(submit(999)==EXT_STALE);}

int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG","0");
    // Real Win32 process metadata only: verifies production DuplicateHandle /
    // creation identity / path logic without creating processes or touching UI.
    HANDLE self=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,GetCurrentProcessId());CHECK(self!=nullptr);
    FILETIME created={},exited={},kernel={},user={};CHECK(GetProcessTimes(self,&created,&exited,&kernel,&user)!=FALSE);
    ExternalProcessV1 identity={static_cast<uint64_t>(reinterpret_cast<uintptr_t>(self)),
        (static_cast<uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime,GetCurrentProcessId(),0};
    HANDLE pinned=nullptr;CHECK(ExternalPinProcess(identity,&pinned));CHECK(pinned!=self && ExternalAlive(pinned));
    wchar_t selfPath[32768];DWORD chars=32768;CHECK(QueryFullProcessImageNameW(pinned,0,selfPath,&chars)!=FALSE);
    CHECK(ExternalPathMatches(pinned,selfPath,chars) && !ExternalPathMatches(pinned,L"C:\\wrong.exe",12));CloseHandle(pinned);
    identity.creationFileTime++;CHECK(!ExternalPinProcess(identity,&pinned));identity.creationFileTime--;identity.pid++;
    CHECK(!ExternalPinProcess(identity,&pinned));CloseHandle(self);identity.handle=UINTPTR_MAX;
    CHECK(!ExternalPinProcess(identity,&pinned));
    g_externalPlatform.now=fakeQpc;g_externalPlatform.frequency=fakeFrequency;g_externalPlatform.installOwner=fakeInstall;
    g_externalPlatform.ownerValid=fakeOwnerValid;g_externalPlatform.pinProcess=fakePin;g_externalPlatform.pathMatches=fakePathMatches;
    g_externalPlatform.alive=fakeProcessAlive;g_externalPlatform.foregroundPid=fakeForeground;g_externalPlatform.nonce=fakeRandom;
    g_externalPlatform.startWatchdog=fakeThread;g_externalPlatform.closeHandle=fakeClose;
    CHECK(ExternalFfbEnter((HWND)101)==EXT_INVALID && !g_externalMode.load());
    g_di=&fakeInput;CHECK(ExternalFfbEnter((HWND)100)==EXT_OK);CHECK(ExternalFfbEnter((HWND)101)==EXT_WRONG_MODE);
    auto c=config();uint64_t out=888;
    c.version=2;CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&out)==EXT_INVALID && out==0);
    c=config();c.target.reserved=1;CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&out)==EXT_INVALID);
    c=config();c.target.creationFileTime=100;CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&out)==EXT_INVALID);
    c=config();fakePath=false;CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&out)==EXT_INVALID);fakePath=true;
    CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",0,&out)==EXT_INVALID);
    CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",12,&out)==EXT_INVALID); // Includes NUL, not a path extent.
#ifndef _WIN64
    c.target.handle=0x100000001ULL;CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&out)==EXT_INVALID);
#endif
    for(unsigned field=0;field<5;++field){c=config();if(field==0)c.maxMagnitude=10001;if(field==1)c.sourceMaxAgeMs=151;
        if(field==2)c.clientMaxAgeMs=251;if(field==3)c.pulseMaxMs=51;if(field==4)memset(c.wheelGuid,0,16);
        CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&out)==EXT_INVALID);}

    // Either handshake ordering; a client refresh never replays a buffered force.
    reset();arm();int starts=fakeEffect.starts;
    CHECK(submit()==EXT_WAITING && fakeEffect.starts==starts);CHECK(renew()==EXT_OK && fakeEffect.starts==starts);
    fakeNow=1001;CHECK(submit(2,-5000)==EXT_APPLIED && fakeEffect.magnitude==-5000 && fakeEffect.direction==5000);
    CHECK(submit(2)==EXT_STALE && fakeEffect.starts==starts+1);
    for(uint64_t seq=3;seq<20;++seq){fakeNow+=20;readWheel();CHECK(renew(seq)==EXT_OK);CHECK(submit(seq)==EXT_APPLIED);}
    CHECK(fakeNow>ticket.expiresQpc && status().state==EXT_ARMED); // No periodic re-arm timer.
    reset();arm();CHECK(renew()==EXT_OK);CHECK(submit()==EXT_APPLIED);

    // Source-first accepted echo can outlive the initial ticket while waiting
    // for its separate client-startup deadline. Continued source cannot extend it.
    reset();arm();CHECK(submit()==EXT_WAITING);
    for(uint64_t seq=2;seq<=9;++seq){fakeNow+=20;readWheel();CHECK(submit(seq)==EXT_WAITING);}
    CHECK(fakeNow>ticket.expiresQpc);CHECK(renew()==EXT_OK);fakeNow++;CHECK(submit(10)==EXT_APPLIED);
    reset();arm();CHECK(submit()==EXT_WAITING);
    for(uint64_t seq=2;seq<=13;++seq){fakeNow+=20;readWheel();CHECK(submit(seq)==EXT_WAITING);}
    fakeNow=1250;readWheel();CHECK(status().reason==EXT_REASON_CLIENT_STALE);checkNoResume();

    // Half-open initial ticket, +/-one-QPC-tick ambiguity, old producer backlog.
    reset();arm();auto f=source();f.capturedQpc=ticket.issuedQpc-2;
    CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_STALE);f.capturedQpc=ticket.issuedQpc-1;
    CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_WAITING);
    reset();arm();f=source();f.capturedQpc=fakeNow+2;CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_STALE);
    f.capturedQpc=fakeNow+1;CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_WAITING);
    reset();arm();for(int i=0;i<7;++i){fakeNow+=20;readWheel();}
    fakeNow=ticket.expiresQpc;f=source();CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_STALE);CHECK(status().reason==EXT_REASON_SOURCE_STALE);
    ready();f=source(2);f.nonceLow++;CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_STALE);
    f=source(2);f.qpcFrequency=999;CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_STALE);
    f=source(2);f.reserved=1;CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_INVALID);
    f=source(2,5001);CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_INVALID);
    f=source(2);f.attestations&=~EXT_DRIVING;fakeNow++;f.capturedQpc=fakeNow;
    CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_INELIGIBLE && status().reason==EXT_REASON_SOURCE_INVALID);checkNoResume();

    // No watchdog tick: old leases are observed before NEW traffic can replace
    // them. Keep the independent wheel sample current to isolate each lease.
    ready();{ExternalPolicyGuard p;g_external.status.wheelReadQpc=1150;}fakeNow=1150;
    CHECK(submit(2)==EXT_STALE && status().reason==EXT_REASON_SOURCE_STALE);checkNoResume();
    ready();{ExternalPolicyGuard p;g_external.status.wheelReadQpc=1250;g_external.status.sourceDeadlineQpc=1300;}fakeNow=1250;
    CHECK(renew(2)==EXT_STALE && status().reason==EXT_REASON_CLIENT_STALE);checkNoResume();
    ready();fakeNow=1050;readWheel();CHECK(status().reason==EXT_REASON_INPUT);checkNoResume();

    // Identity/focus/window/required input failures revoke; PID reuse cannot
    // substitute a new process for the pinned dead handle.
    for(int why=0;why<5;++why){ready();if(why<3)fakeAlive[why]=false;if(why==3)fakeFocus=99;if(why==4)fakeOwner=false;
        CHECK(submit(2)==EXT_STALE);auto s=status();CHECK(s.state==EXT_REVOKED);checkNoResume();}
    ready();fakeWheel.readResult=E_FAIL;int axes[8],pov[4];unsigned char buttons[128];
    CHECK(!ReadDeviceStateWithPov(0,axes,8,buttons,128,pov,4));CHECK(status().reason==EXT_REASON_INPUT);checkNoResume();
    ready();int acquireCount=fakeWheel.acquires;fakeWheel.pollResult=DIERR_NOTACQUIRED;readWheel();CHECK(fakeWheel.acquires==acquireCount);
    ready();CloseReadDevices();CHECK(status().reason==EXT_REASON_INPUT);checkNoResume();
    ready();driverHook=[](Stage s){if(s==ReadDevice)fakeNow+=50;return S_OK;};readWheel();driverHook={};CHECK(status().reason==EXT_REASON_INPUT);

    // Delays, focus loss and every driver failure between admission and Start.
    for(Stage stage:{StopFx,SetFx,GetFx,ActOn,StartFx}) {
        ready();starts=fakeEffect.starts;fakeNow++;bool once=false;
        driverHook=[&](Stage s){if(s==stage && !once){once=true;fakeNow+=151;}return S_OK;};
        CHECK(submit(2)!=EXT_APPLIED);driverHook={};CHECK(fakeEffect.starts==starts+(stage==StartFx?1:0));CHECK(!g_external.active());checkNoResume();
        ready();starts=fakeEffect.starts;fakeNow++;once=false;
        driverHook=[&](Stage s){if(s==stage && !once){once=true;return E_FAIL;}return S_OK;};
        CHECK(submit(2)!=EXT_APPLIED);driverHook={};CHECK(fakeEffect.starts==starts+(stage==StartFx?1:0));CHECK(status().state==EXT_FAULTED);
    }
    ready();fakeNow++;fakeEffect.mismatch=true;starts=fakeEffect.starts;
    CHECK(submit(2)!=EXT_APPLIED && fakeEffect.starts==starts);fakeEffect.mismatch=false;
    for(int field=0;field<2;++field){ready();fakeNow++;starts=fakeEffect.starts;fakeEffect.badTrigger=field==0;fakeEffect.badDelay=field==1;
        CHECK(submit(2)!=EXT_APPLIED && fakeEffect.starts==starts);fakeEffect.badTrigger=fakeEffect.badDelay=false;}
    // Caller memory reuse cannot alter an already validated local frame.
    ready();fakeNow++;f=source(2,-2000);driverHook=[&](Stage stage){if(stage==StopFx)f.signedMagnitude=10000;return S_OK;};
    CHECK(ExternalFfbSubmitConstant(session,&f)==EXT_APPLIED && fakeEffect.magnitude==-2000);driverHook={};
    ready();fakeNow++;starts=fakeEffect.starts;driverHook=[](Stage s){if(s==SetFx)fakeFocus=0;return S_OK;};
    CHECK(submit(2)!=EXT_APPLIED && fakeEffect.starts==starts);driverHook={};
    ready();fakeNow++;starts=fakeEffect.starts;CHECK(submit(2,0)==EXT_CANCELED && fakeEffect.starts==starts);
    ready();fakeNow=1040;starts=fakeEffect.starts;CHECK(submit(2)==EXT_APPLIED && fakeEffect.duration==10000);
    ready();fakeNow=1031;starts=fakeEffect.starts;bool delayed=false;
    driverHook=[&](Stage stage){if(stage==SetFx && !delayed){delayed=true;fakeNow+=1;}return S_OK;};
    CHECK(submit(2)!=EXT_APPLIED && fakeEffect.starts==starts);driverHook={}; // 19 ms configured, only 18 ms input lease left.

    // A blocked Start permits policy revocation/close/status but no concurrent
    // COM cancellation/destruction. Completion cannot start a second pulse.
    ready();fakeNow++;starts=fakeEffect.starts;
    std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;int result=0;
    driverHook=[&](Stage s){if(s==StartFx){std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_one();cv.wait(lock,[&]{return release;});}return S_OK;};
    std::thread caller([&]{result=submit(2);});
    {std::unique_lock<std::mutex> lock(mutex);CHECK(cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}));}
    fakeNow=1200;CHECK(ExternalFfbClose(session)==EXT_PENDING);auto s=status();
    CHECK(s.state==EXT_REVOKED && s.cancellation==EXT_CANCEL_REQUESTED && (s.flags&EXT_DRIVER_BUSY) && (s.flags&EXT_CLOSE_PENDING));
    {std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_one();caller.join();driverHook={};
    CHECK(result!=EXT_APPLIED && fakeEffect.starts==starts+1 && status().state==EXT_SESSION_CLOSED);

    // Read/close serialized through same production gate.
    ready();entered=release=false;driverHook=[&](Stage stage){if(stage==ReadDevice){std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_one();cv.wait(lock,[&]{return release;});}return S_OK;};
    int releases=fakeWheel.releases;std::thread reader([&]{readWheel();});
    {std::unique_lock<std::mutex> lock(mutex);CHECK(cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}));}
    CHECK(ExternalFfbClose(session)==EXT_PENDING && fakeWheel.releases==releases);
    {std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_one();reader.join();driverHook={};
    CHECK(ExternalTryService() && fakeWheel.releases==releases+1 && status().state==EXT_SESSION_CLOSED);

    // Close requested DURING an older Stop transaction cannot be overwritten.
    ready();entered=release=false;driverHook=[&](Stage stage){if(stage==StopFx){std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_one();cv.wait(lock,[&]{return release;});}return S_OK;};
    std::thread stopper([&]{ExternalFfbDisarm(session,EXT_REASON_STOP);});
    {std::unique_lock<std::mutex> lock(mutex);CHECK(cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}));}
    CHECK(ExternalFfbClose(session)==EXT_PENDING);
    {std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_one();stopper.join();driverHook={};
    CHECK(status().cancellation==EXT_CANCEL_REQUESTED);CHECK(ExternalTryService());CHECK(status().state==EXT_SESSION_CLOSED);

    // SAME-THREAD driver message pumping: recursive CS alone is not exclusion.
    // Reentry may revoke immediately but cannot invoke COM/free on this stack.
    for(Stage stage:{StartFx,ReadDevice,StopFx}) {
        ready();fakeNow++;bool once=false;int beforeReleases=fakeWheel.releases;
        driverHook=[&](Stage enteredStage){if(enteredStage==stage && !once){once=true;size_t count=operations.size();
            CHECK(ExternalFfbClose(session)==EXT_PENDING);CloseReadDevices();CloseAuxDevice();
            CHECK(operations.size()==count && fakeWheel.releases==beforeReleases);
            CHECK(status().cancellation==EXT_CANCEL_REQUESTED);
        }return S_OK;};
        if(stage==ReadDevice)readWheel();else CHECK(submit(2)!=EXT_APPLIED);
        driverHook={};CHECK(once);CHECK(ExternalTryService());CHECK(status().state==EXT_SESSION_CLOSED && fakeWheel.releases==beforeReleases+1);
    }
    ready();fakeNow++;bool once=false;int forwarded=forwardedMessages;driverHook=[&](Stage enteredStage){if(enteredStage==StartFx && !once){once=true;
        int before=fakeWheel.releases;CHECK(ExternalWndProc((HWND)100,WM_CLOSE,0,0)==0);CHECK(fakeWheel.releases==before && forwardedMessages==forwarded);
    }return S_OK;};CHECK(submit(2)!=EXT_APPLIED);driverHook={};CHECK(status().state==EXT_SESSION_CLOSED);
    CHECK(ExternalWndProc((HWND)100,WM_CLOSE,0,0)==123 && forwardedMessages==forwarded+1);

    // Autocentre baseline is preserved exactly and restored AFTER Unacquire.
    for(DWORD prior:{0UL,1UL}){reset(prior);operations.clear();CHECK(ExternalFfbClose(session)==EXT_CLOSED && fakeWheel.autocentre==prior);
        auto un=std::find(operations.begin(),operations.end(),UnacquireDevice),rest=std::find(operations.begin(),operations.end(),SetAuto),rel=std::find(operations.begin(),operations.end(),ReleaseDevice);
        CHECK(un<rest && rest<rel);}
    CloseReadDevices();c=config();fakeWheel.autocentre=1;operations.clear();
    driverHook=[](Stage stage){return stage==GetAuto?E_NOTIMPL:S_OK;};CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&session)==EXT_FAULT);driverHook={};
    CHECK(std::find(operations.begin(),operations.end(),SetAuto)==operations.end() && fakeWheel.autocentre==1);
    operations.clear();driverHook=[](Stage stage){return stage==Range?E_FAIL:S_OK;};
    CHECK(ExternalFfbOpen(&c,L"C:\\Game.exe",11,&session)==EXT_FAULT);driverHook={};CHECK(fakeWheel.autocentre==1);
    reset();driverHook=[](Stage stage){return stage==SetAuto?E_FAIL:S_OK;};
    CHECK(ExternalFfbClose(session)==EXT_FAULT);driverHook={};CHECK(ExternalFfbClose(session)==EXT_FAULT);
    CHECK(status().state==EXT_SESSION_CLOSED && status().cancellation==EXT_CANCEL_API_FAILED);
    CHECK(ExternalTryService() && ExternalFfbClose(session)==EXT_FAULT); // Watchdog cannot erase recorded restoration failure.
    forwarded=forwardedMessages;CHECK(!g_device && !g_externalEffect);
    CHECK(ExternalWndProc((HWND)100,WM_CLOSE,0,0)==123 && forwardedMessages==forwarded+1);
    CHECK(ExternalFfbClose(session)==EXT_FAULT && !(status().flags&EXT_CLOSE_PENDING));
    // A failed Unacquire forbids an acquired autocentre write; report lost
    // preservation rather than claiming orderly success.
    session=0;CloseReadDevices();reset();operations.clear();driverHook=[](Stage stage){return stage==UnacquireDevice?E_FAIL:S_OK;};
    CHECK(ExternalFfbClose(session)==EXT_FAULT);driverHook={};CHECK(std::find(operations.begin(),operations.end(),SetAuto)==operations.end());
    session=0;CloseReadDevices();

    // Sticky guards cover every legacy force family, selection, HWND replacement,
    // reacquire and autocentre route, also after close/panic/free/failed open.
    for(int closeRoute=0;closeRoute<3;++closeRoute){ready();if(closeRoute==0)PanicStop();if(closeRoute==1)FreeDirectInput();if(closeRoute==2)CHECK(ExternalFfbClose(session)==EXT_CLOSED);
        operations.clear();CHECK(!InitDirectInput(999));Aquire();CHECK(!StartEffect());CHECK(!SetDeviceForcesXY(5000,0));CHECK(!SetAutoCenter(TRUE));
        CHECK(CreatePeriodicEffect(30)==-1 && CreatePeriodicBurst(30,20)==-1 && CreateConstantBurst(20)==-1 && CreateConditionEffect(0)==-1);
        CHECK(!UpdatePeriodicEffect(0,1000,30000) && !PlayPeriodicBurst(0,1000,30000) && !PlayShapedPeriodicBurst(0,1000,30000,0,10));
        CHECK(!PlayConstantBurst(0,1000) && !UpdateConditionEffect(0,1000,10000,0,0));SetHoldTimeoutMs(10);
        SetPreferredDevice("different");SetPreferredDeviceGuid(&fakeGuid);SetPreferredDeviceIndex(9);SetStrictDeviceSelection(0);
        CHECK(operations.empty() && g_hwnd==(HWND)100 && !g_holdThread && g_externalMode.load());
    }
    // Merged lock order: legacy cancel routes test the external latch before
    // g_apiMutex, so revocation never waits behind another legacy caller.
    ready();
    {
        HANDLE held=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        HANDLE events[2]={held,release};
        HANDLE holder=CreateThread(nullptr,0,[](LPVOID p)->DWORD{
            HANDLE* e=static_cast<HANDLE*>(p);std::lock_guard<std::recursive_mutex> api(g_apiMutex);
            SetEvent(e[0]);WaitForSingleObject(e[1],INFINITE);return 0;},events,0,nullptr);
        CHECK(holder!=nullptr && WaitForSingleObject(held,5000)==WAIT_OBJECT_0);
        CHECK(SetDeviceForcesXY(0,0)==0);
        CHECK(status().reason==EXT_REASON_LEGACY_CANCEL && status().cancellation==EXT_CANCEL_API_DONE);
        ZeroForces();PanicStop();FreeDirectInput(); // Each returns while the api mutex is held elsewhere.
        CHECK(status().state==EXT_SESSION_CLOSED);
        SetEvent(release);CHECK(WaitForSingleObject(holder,5000)==WAIT_OBJECT_0);
        CloseHandle(holder);CloseHandle(held);CloseHandle(release);
    }
    CHECK(ExternalFfbClose(session)==EXT_CLOSED);CloseReadDevices();session=0;
    ready();ExternalFakeEffect legacy[4];g_effect=&legacy[0];g_periodic[0].fx=&legacy[1];g_constantBurst[0].fx=&legacy[2];g_condition[0].fx=&legacy[3];
    ZeroForces();for(auto& effect:legacy)CHECK(effect.stops==1);g_effect=nullptr;g_periodic[0].fx=nullptr;g_constantBurst[0].fx=nullptr;g_condition[0].fx=nullptr;
    CHECK(status().reason==EXT_REASON_LEGACY_CANCEL && status().cancellation==EXT_CANCEL_API_DONE);
    CHECK(ExternalFfbClose(session)==EXT_CLOSED);CloseReadDevices();
    // FFB Off does not disable input-only readers. Consumer reopens by saved GUID.
    CHECK(OpenReadDevice(0)==0 && !g_read[0].isWheel);readWheel();CloseReadDevices();
    CHECK(ExternalFfbEnter((HWND)100)==EXT_OK && !SetAutoCenter(TRUE));
    printf("PASS %d external-session production checks: fake identity/window/QPC, lease/rearm, finite output, blocked driver, read/close serialization, sticky legacy guards, autocentre baseline; NO hardware/window opened\n",checks);
}
