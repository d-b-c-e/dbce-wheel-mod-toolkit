// Production adapter under fake COM effects. No real DirectInput object is opened.
#include <windows.h>
static DWORD testForegroundPid=GetCurrentProcessId();
static HWND TestForegroundWindow(){return (HWND)1;}
static DWORD TestWindowProcess(HWND,DWORD* pid){*pid=testForegroundPid;return 1;}
#define GetForegroundWindow TestForegroundWindow
#define GetWindowThreadProcessId TestWindowProcess
#include "../WheelFfb.cpp"
#undef GetForegroundWindow
#undef GetWindowThreadProcessId
#include <cassert>
#include <initializer_list>
#include "../../forcemodel/cadence_impact_mixer.h"
struct FakeEffect:IDirectInputEffect {
 int starts=0,stops=0,releases=0,sets=0;DWORD duration=0,axes=0,flags=0;LONG magnitude=0,direction=0;
 ULONGLONG now=0,end=0;bool active=false,mismatch=false;DWORD overrideDuration=0;
 HRESULT stopHr=S_OK,setHr=S_OK,readHr=S_OK,startHr=S_OK;
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override{return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return 1;}ULONG STDMETHODCALLTYPE Release()override{releases++;active=false;return 0;}
 HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID)override{return S_OK;}
 HRESULT STDMETHODCALLTYPE GetEffectGuid(LPGUID)override{return S_OK;}
 HRESULT STDMETHODCALLTYPE SetParameters(LPCDIEFFECT e,DWORD f)override{
  sets++;flags=f;axes=e->cAxes;direction=e->rglDirection[0];magnitude=((DICONSTANTFORCE*)e->lpvTypeSpecificParams)->lMagnitude;
  if(f&DIEP_DURATION)duration=e->dwDuration;
  assert(direction==labs(magnitude) && (axes==1 || axes==2));return setHr;
 }
 HRESULT STDMETHODCALLTYPE GetParameters(LPDIEFFECT e,DWORD f)override{assert(f==DIEP_DURATION);e->dwDuration=mismatch?INFINITE:(overrideDuration?overrideDuration:duration);return readHr;}
 HRESULT STDMETHODCALLTYPE Start(DWORD iterations,DWORD f)override{assert(iterations==1 && f==0);starts++;active=startHr==S_OK;end=now+duration;return startHr;}
 HRESULT STDMETHODCALLTYPE Stop()override{stops++;active=false;return stopHr;}
 HRESULT STDMETHODCALLTYPE GetEffectStatus(LPDWORD bits)override{*bits=active&&now<end?DIEGES_PLAYING:0;return S_OK;}
 HRESULT STDMETHODCALLTYPE Download()override{return S_OK;}HRESULT STDMETHODCALLTYPE Unload()override{return S_OK;}
 HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE)override{return S_OK;}
 bool playing()const{return active&&now<end;}
};
struct FakeDevice {
 FakeEffect effect;int creates=0;HRESULT result=S_OK;
 HRESULT CreateEffect(REFGUID guid,LPCDIEFFECT e,LPDIRECTINPUTEFFECT* out,LPUNKNOWN) {
  creates++;assert(IsEqualGUID(guid,GUID_ConstantForce));assert(e->dwGain==10000 && e->dwDuration>=1000 && e->dwDuration<=500000);
  assert(e->dwTriggerButton==DIEB_NOTRIGGER && e->dwFlags==(DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS));
  assert(e->rgdwAxes[0]==DIJOFS_X && e->rglDirection[0]==0 && ((DICONSTANTFORCE*)e->lpvTypeSpecificParams)->lMagnitude==0);
  effect.duration=e->dwDuration;*out=&effect;return result;
 }
};
int main(){
 SetEnvironmentVariableA("DBCE_FFB_LOG","0");
 for(DWORD axes:{1UL,2UL})for(int ms:{1,120,500})for(int mag:{-10000,-5000,-1,1,5000,10000}) {
  FakeDevice d;LPDIRECTINPUTEFFECT fx=nullptr;
  assert(wheel_constant_burst::create(&d,fx,axes,ms)==S_OK && d.effect.starts==0);
  assert(wheel_constant_burst::play(fx,axes,ms,mag)==S_OK && d.effect.magnitude==mag && d.effect.direction==labs(mag));
  assert(d.effect.flags==(DIEP_DIRECTION|DIEP_TYPESPECIFICPARAMS|DIEP_DURATION));
  d.effect.now=ms*1000-1;assert(d.effect.playing());d.effect.now++;assert(!d.effect.playing());
  assert(wheel_constant_burst::play(fx,axes,ms,mag)==S_OK && d.effect.starts==2); // Same event amplitude restarts.
  assert(wheel_constant_burst::play(fx,axes,ms,0)==S_OK && !d.effect.playing() && d.effect.starts==2);
 }
 for(HRESULT error:{(HRESULT)E_FAIL,(HRESULT)DIERR_INPUTLOST,(HRESULT)DIERR_NOTEXCLUSIVEACQUIRED,(HRESULT)DI_TRUNCATED})for(int stage=0;stage<5;stage++) {
  FakeDevice d;LPDIRECTINPUTEFFECT fx=nullptr;wheel_constant_burst::create(&d,fx,2,120);
  if(stage==0){if(SUCCEEDED(error))continue;d.effect.stopHr=error;}
  if(stage==1)d.effect.setHr=error;if(stage==2)d.effect.readHr=error;if(stage==3)d.effect.startHr=error;if(stage==4)d.effect.mismatch=true;
  assert(wheel_constant_burst::play(fx,2,120,5000)!=S_OK && !fx && d.effect.releases==1);
  assert(d.effect.starts==(stage==3?1:0));
 }
 FakeDevice shortened;LPDIRECTINPUTEFFECT shortFx=nullptr;
 wheel_constant_burst::create(&shortened,shortFx,2,120);shortened.effect.overrideDuration=119000;
 assert(wheel_constant_burst::play(shortFx,2,120,5000)!=S_OK && !shortFx && shortened.effect.starts==0);
 FakeDevice bad;LPDIRECTINPUTEFFECT fx=nullptr;
 for(int ms:{0,501})assert(wheel_constant_burst::create(&bad,fx,2,ms)==E_INVALIDARG);
 bad.result=DIERR_UNSUPPORTED;assert(wheel_constant_burst::create(&bad,fx,2,120)==DIERR_UNSUPPORTED && !fx);
 InitializeCriticalSection(&g_lock);g_lockInit=true;g_device=(LPDIRECTINPUTDEVICE8)1;
 FakeEffect burst,steering,periodic;
 g_effect=&steering;g_periodic[0].used=true;g_periodic[0].fx=&periodic;
 auto& s=g_constantBurst[0];s.used=true;s.durationMs=120;s.axes=2;s.fx=&burst;
 assert(PlayConstantBurst(0,-5000)==1 && burst.magnitude==-5000);
 assert(!steering.sets && !steering.stops && !steering.starts && !periodic.stops); // Independent output.
 int originalStops=burst.stops,originalSets=burst.sets,originalStarts=burst.starts;
 assert(SetDeviceForcesXY(0,0)==1 && SetDeviceForcesXY(-2000,0)==1);
 assert(burst.stops==originalStops && burst.sets==originalSets && burst.starts==originalStarts && s.started);
 ZeroForces(); // The bounded route owns one constant command, with no parallel fake bursts.
 // New opt-in mixed commands use the unchanged production output route under fakes.
 for(int hz:{20,30,50,60,120,250,1000}) {
  dbce::force::CadenceImpactMixer mixed;
  const double dt=1.0/hz;assert(mixed.trigger(1,1,1,true));float previous=0;int previousUnits=0;
  for(int i=0;i<int(std::ceil(.4/dt))+2;i++){
   double begin=1-dt+i*dt;float value=mixed.mix(.25f,begin,begin+dt,true,true);
   assert(!mixed.last_input_rejected&&std::abs(value)<=.25f);
   assert(std::abs(value-previous)<=1.5f*float(dt)+1e-6f);previous=value;
   int command=int(std::round(value*10000));assert(std::abs(command)<=2500);
   assert(std::abs(command-previousUnits)<=int(std::ceil(1.5*dt*10000+1e-6)));previousUnits=command;
   assert(SetDeviceForcesXY(command,0)==1);assert(std::abs(g_lastForce)<=2500);
  }
  assert(mixed.mix(.25f,2,2+dt,false,true)==0);assert(SetDeviceForcesXY(0,0)==1);
 }
 // Rejection requires immediate caller zero, not waiting for another mix/watchdog.
 for(int kind=0;kind<3;kind++) {
  dbce::force::CadenceImpactMixer mixed;assert(mixed.trigger(1,1,1,true));
  float value=mixed.mix(0,1,1.01,true,true);assert(value>0);
  assert(SetDeviceForcesXY(int(std::round(value*10000)),0)==1&&g_lastForce>0);
  bool accepted=kind==0?mixed.trigger(1,1,1.01):kind==1?mixed.trigger(1,0,1.01,true):mixed.trigger(1,1,0,true);
  if(!accepted) { ZeroForces(); } // existing guarded caller route
  assert(!accepted&&mixed.last_input_rejected&&mixed.last_command==0&&g_lastForce==0);
  assert(mixed.mix(0,1.01,1.011,true,true)==0);
 }
 assert(SetDeviceForcesXY(1000,0)==1);
 g_lastUpdateTick=100;g_holdMs=150;
 assert(!ApplyHoldTimeout(250)&&ApplyHoldTimeout(251)&&g_lastForce==0);
 assert(SetDeviceForcesXY(1000,0)==1);GuardWndProc(nullptr,WM_ACTIVATEAPP,FALSE,0);assert(g_lastForce==0);
 int sets=burst.sets;assert(!PlayConstantBurst(0,10001) && !PlayConstantBurst(0,-10001) && !PlayConstantBurst(4,5000) && burst.sets==sets);
 testForegroundPid=0;assert(!PlayConstantBurst(0,5000));assert(PlayConstantBurst(0,0)==1);testForegroundPid=GetCurrentProcessId();
 assert(PlayConstantBurst(0,5000)==1);ZeroForces();assert(!s.started && s.lastMag==0 && burst.stops>0 && periodic.stops>0);
 assert(PlayConstantBurst(0,5000)==1);g_lastForce=0;g_lastUpdateTick=100;g_holdMs=100;
 assert(!ApplyHoldTimeout(200) && ApplyHoldTimeout(201) && !s.started);
 assert(PlayConstantBurst(0,5000)==1);GuardWndProc(nullptr,WM_ACTIVATEAPP,FALSE,0);assert(!s.started);
 int starts=burst.starts;GuardWndProc(nullptr,WM_ACTIVATEAPP,TRUE,0);assert(burst.starts==starts);
 assert(StopConstantBurst(0)==1 && StopConstantBurst(0)==1);
 burst.mismatch=true;assert(!PlayConstantBurst(0,5000) && !s.fx && s.used && s.durationMs==120);
 // Panic's release helper and full shutdown both clear pool ownership.
 ReleaseConstantBursts();assert(!s.used);s.used=true;s.fx=&burst;s.durationMs=120;s.axes=2;
 g_device=nullptr;FreeDirectInput();assert(!s.used && !s.fx && !g_effect && !g_periodic[0].used);
 DeleteCriticalSection(&g_lock);g_lockInit=false;
 puts("PASS finite constant helper/production exports: signed axes, finite expiry, failures, independent steering/periodics, focus/watchdog/full shutdown; no hardware opened");
}
