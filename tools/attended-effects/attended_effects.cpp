// Developer UI only. Includes the frozen production adapter; no modified DLL/export.
#include "../../native/wheelffb/WheelFfb.cpp"
#include <tlhelp32.h>
#include <string>
#include <initializer_list>
#include "policy.h"
#include "game_guard.h"
#ifndef PROBE_SOURCE_REV
#define PROBE_SOURCE_REV "unversioned-build"
#endif

static HWND ui,devices,strength,statusText,buttons[3],connectButton,scanButton;
static attended::Policy policy;
static LPDIRECTINPUTEFFECT pulse=nullptr;
static int burstSlot=-1;
static GUID choices[8]; static int choiceCount=0;
static ULONGLONG began=0,nextZero=0; static int zeroUpdates=0;
static char records[256][1024],logFile[MAX_PATH]; static int recordCount=0,dropped=0;
static DWORD originalAuto=0; static bool haveOriginalAuto=false, initAttempted=false;
static GUID selectedGuid={};
static LARGE_INTEGER frequency;
static double milliseconds() { LARGE_INTEGER n; QueryPerformanceCounter(&n); return 1000.0*n.QuadPart/frequency.QuadPart; }
static void record(const char* format,...) {
    if(recordCount==256) { dropped++;return; }
    char* line=records[recordCount++]; int used=sprintf_s(line,1024,"qpc_ms=%.3f ",milliseconds());
    va_list args;va_start(args,format);vsnprintf_s(line+used,1024-used,_TRUNCATE,format,args);va_end(args);
}
static void flush() {
    if(policy.active || !recordCount) return;
    FILE* f=nullptr;
    if(fopen_s(&f,logFile,"a") || !f) { SetWindowTextA(statusText,"Could not write diagnostic log. Stop and check the log path.");return; }
    for(int i=0;i<recordCount;i++) fprintf(f,"%s\n",records[i]);
    if(dropped) fprintf(f,"buffer_dropped=%d\n",dropped);
    fclose(f);recordCount=0;dropped=0;
}
static bool focused() { return GetForegroundWindow()==ui; }
struct ProcessReader {
    HANDLE snapshot=INVALID_HANDLE_VALUE;PROCESSENTRY32 entry={};DWORD lastError=0;
    ~ProcessReader() {if(snapshot!=INVALID_HANDLE_VALUE)CloseHandle(snapshot);}
    unsigned long error() const {return lastError;}
    attended::ScanStep first(attended::ProcessRow& row) {
        snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snapshot==INVALID_HANDLE_VALUE) {lastError=GetLastError();return attended::ScanStep::Error;}
        entry.dwSize=sizeof entry;
        if(!Process32First(snapshot,&entry)) {lastError=GetLastError();return attended::ScanStep::Error;}
        row={entry.szExeFile,entry.th32ProcessID};return attended::ScanStep::Item;
    }
    attended::ScanStep next(attended::ProcessRow& row) {
        if(Process32Next(snapshot,&entry)) {row={entry.szExeFile,entry.th32ProcessID};return attended::ScanStep::Item;}
        lastError=GetLastError();return lastError==ERROR_NO_MORE_FILES ? attended::ScanStep::End : attended::ScanStep::Error;
    }
};
static attended::GameCheck latestGameCheck;
static bool gameRunning() {
    ProcessReader reader;latestGameCheck=attended::checkGames(reader);
    record("game_guard blocked=%d %s",latestGameCheck.blocked,latestGameCheck.reason);
    return latestGameCheck.blocked;
}
static void controls() {
    bool ready=policy.connected && !policy.active && GetTickCount64()>=policy.until && focused();
    for(HWND b:buttons) EnableWindow(b,ready);
    EnableWindow(strength,!policy.active && GetTickCount64()>=policy.until);
    EnableWindow(connectButton,!policy.active && !policy.connected);
    EnableWindow(scanButton,!policy.active && !policy.connected);
    EnableWindow(devices,!policy.connected);
}
static void effectStatus(const char* where,LPDIRECTINPUTEFFECT fx) {
    if(!fx) {record("%s no_effect",where);return;}
    DWORD bits=0;double t=milliseconds();HRESULT hr=fx->GetEffectStatus(&bits);
    record("%s GetEffectStatus hr=0x%08lX bits=0x%08lX latency_ms=%.3f (not measured torque)",where,hr,bits,milliseconds()-t);
}
static void echo(const char* label,LPDIRECTINPUTEFFECT fx,bool constant=false) {
    if(!fx) return;
    DWORD axes[2]={};LONG directions[2]={};DIPERIODIC params={};DIENVELOPE env={};env.dwSize=sizeof env;
    DIEFFECT e={};e.dwSize=sizeof e;e.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS;e.cAxes=2;e.rgdwAxes=axes;e.rglDirection=directions;
    e.lpEnvelope=constant?nullptr:&env;e.cbTypeSpecificParams=constant?sizeof(DICONSTANTFORCE):sizeof params;e.lpvTypeSpecificParams=&params;
    DWORD flags=DIEP_AXES|DIEP_DIRECTION|DIEP_GAIN|DIEP_DURATION|DIEP_SAMPLEPERIOD|DIEP_STARTDELAY|DIEP_TYPESPECIFICPARAMS|(constant?0:DIEP_ENVELOPE);
    double t=milliseconds();HRESULT hr=fx->GetParameters(&e,flags);
    if(constant) {
        record("%s constant echo hr=0x%08lX latency_ms=%.3f axes=%lu [%lu,%lu] direction=[%ld,%ld] gain=%lu duration_us=%lu sample_us=%lu delay_us=%lu magnitude=%ld (API echo only)",label,hr,milliseconds()-t,e.cAxes,axes[0],axes[1],directions[0],directions[1],e.dwGain,e.dwDuration,e.dwSamplePeriod,e.dwStartDelay,(LONG)params.dwMagnitude);
        return;
    }
    record("%s echo hr=0x%08lX latency_ms=%.3f axes=%lu [%lu,%lu] direction=[%ld,%ld] gain=%lu duration_us=%lu sample_us=%lu delay_us=%lu typeBytes=%lu magnitude=%lu period=%lu phase=%lu offset=%ld envelope=%d attack=%lu/%lu fade=%lu/%lu (API echo only)",label,hr,milliseconds()-t,e.cAxes,axes[0],axes[1],directions[0],directions[1],e.dwGain,e.dwDuration,e.dwSamplePeriod,e.dwStartDelay,e.cbTypeSpecificParams,params.dwMagnitude,params.dwPeriod,params.dwPhase,params.lOffset,e.lpEnvelope!=nullptr,env.dwAttackLevel,env.dwAttackTime,env.dwFadeLevel,env.dwFadeTime);
}
static BOOL CALLBACK objectInfo(const DIDEVICEOBJECTINSTANCEA* o,VOID*) {
    record("axis name=%s offset=%lu type=0x%08lX flags=0x%08lX actuator=%d",o->tszName,o->dwOfs,o->dwType,o->dwFlags,(o->dwFlags&DIDOI_FFACTUATOR)!=0);return DIENUM_CONTINUE;
}
static void snapshot() {
    wchar_t wide[40]={};StringFromGUID2(g_activeGuid,wide,40);char id[80];
    WideCharToMultiByte(CP_UTF8,0,wide,-1,id,sizeof id,nullptr,nullptr);
    record("connected name=%s instanceGUID=%s axes=%lu constant_effect_axes=%lu",g_activeName,id,g_activeAxes,g_effectAxes);
    double t=milliseconds();HRESULT hr=g_device->EnumObjects(objectInfo,nullptr,DIDFT_AXIS);record("EnumObjects hr=0x%08lX latency_ms=%.3f",hr,milliseconds()-t);
    DIPROPDWORD gain={};gain.diph.dwSize=sizeof gain;gain.diph.dwHeaderSize=sizeof gain.diph;gain.diph.dwHow=DIPH_DEVICE;
    t=milliseconds();hr=g_device->GetProperty(DIPROP_FFGAIN,&gain.diph);record("FFGAIN read hr=0x%08lX gain=%lu latency_ms=%.3f",hr,gain.dwData,milliseconds()-t);
    DIEFFECTINFOA info={};info.dwSize=sizeof info;t=milliseconds();hr=g_device->GetEffectInfo(&info,GUID_Sine);
    record("sine caps hr=0x%08lX type=0x%08lX static=0x%08lX dynamic=0x%08lX latency_ms=%.3f",hr,info.dwEffType,info.dwStaticParams,info.dwDynamicParams,milliseconds()-t);
    echo("constant idle",g_effect,true);
}
static void stopAll(const char* reason) {
    record("stop reason=%s elapsed_ms=%llu zero_updates=%d",reason,policy.active ? GetTickCount64()-began : 0,zeroUpdates);
    if(pulse) {HRESULT hr=pulse->Stop();record("pulse.Stop hr=0x%08lX",hr);pulse->Release();pulse=nullptr;}
    if(g_device) ZeroForces();
    policy.finish(GetTickCount64());controls();flush();
}
static void disconnect(const char* reason) {
    stopAll(reason);
    if(g_device) g_device->Unacquire();
    // Failed OpenCandidate may have changed autocenter then closed its handle.
    // Reopen only that GUID on the same DI instance, never acquire it, and
    // restore only if the current value actually differs from the snapshot.
    LPDIRECTINPUTDEVICE8 restoreDevice=g_device; bool temporary=false;
    if(initAttempted && haveOriginalAuto && !restoreDevice && g_di) {
        HRESULT hr=g_di->CreateDevice(selectedGuid,&restoreDevice,nullptr);
        temporary=SUCCEEDED(hr) && restoreDevice;
        record("restore property handle hr=0x%08lX",hr);
    }
    if(initAttempted && haveOriginalAuto && restoreDevice) {
        DIPROPDWORD p={};p.diph.dwSize=sizeof p;p.diph.dwHeaderSize=sizeof p.diph;p.diph.dwHow=DIPH_DEVICE;
        HRESULT read=restoreDevice->GetProperty(DIPROP_AUTOCENTER,&p.diph);
        record("autocenter cleanup read hr=0x%08lX current=%lu original=%lu",read,p.dwData,originalAuto);
        if(SUCCEEDED(read) && p.dwData!=originalAuto) {
            p.dwData=originalAuto;record("restore original autocenter hr=0x%08lX",restoreDevice->SetProperty(DIPROP_AUTOCENTER,&p.diph));
        }
    }
    if(temporary) restoreDevice->Release();
    initAttempted=false;haveOriginalAuto=false;
    FreeDirectInput();policy.connected=false;burstSlot=-1;controls();flush();
    SetWindowTextA(statusText,"Disconnected. Focus returns never reconnect or replay. Click Connect when ready.");
}
static void scan() {
    if(!focused() || policy.connected || policy.active) return;
    choiceCount=EnumerateDevices();if(choiceCount>8)choiceCount=8;
    SendMessageA(devices,CB_RESETCONTENT,0,0);
    for(int i=0;i<choiceCount;i++) {
        choices[i]=g_candidates[i].guid;wchar_t wide[40]={};StringFromGUID2(choices[i],wide,40);
        char guid[80];WideCharToMultiByte(CP_UTF8,0,wide,-1,guid,sizeof guid,nullptr,nullptr);
        char label[420];sprintf_s(label,"%s  %s",g_candidates[i].name,guid);SendMessageA(devices,CB_ADDSTRING,0,(LPARAM)label);
        record("discovered index=%d %s",i,label);
    }
    // Deliberate selection required, no implicit first-device connect.
    SetWindowTextA(statusText,"Select the wheel by name and GUID, then click Connect. Discovery applies no force.");flush();
}
static void connect() {
    if(policy.connected || policy.active || !focused()) return;
    if(gameRunning()) {SetWindowTextA(statusText,latestGameCheck.reason);flush();return;}
    int selection=(int)SendMessageA(devices,CB_GETCURSEL,0,0);
    if(selection<0 || selection>=choiceCount) {SetWindowTextA(statusText,"Select an explicit wheel first.");return;}
    haveOriginalAuto=false;
    if(EnsureDirectInput()) {
        LPDIRECTINPUTDEVICE8 temporary=nullptr;
        if(SUCCEEDED(g_di->CreateDevice(choices[selection],&temporary,nullptr))) {
            DIPROPDWORD p={};p.diph.dwSize=sizeof p;p.diph.dwHeaderSize=sizeof p.diph;p.diph.dwHow=DIPH_DEVICE;
            HRESULT hr=temporary->GetProperty(DIPROP_AUTOCENTER,&p.diph);haveOriginalAuto=SUCCEEDED(hr);originalAuto=p.dwData;
            record("original autocenter hr=0x%08lX value=%lu",hr,p.dwData);temporary->Release();
        }
    }
    // Don't modify even transient autocenter if its original value cannot be restored.
    if(!haveOriginalAuto) {SetWindowTextA(statusText,"Connect blocked: original autocenter could not be read safely. See log.");flush();return;}
    if(!focused()) {flush();return;}
    selectedGuid=choices[selection];initAttempted=true;
    SetPreferredDeviceGuid(&selectedGuid);SetStrictDeviceSelection(1);
    int ok=InitDirectInput((int)(INT_PTR)ui);record("InitDirectInput accepted=%d hr=0x%08lX",ok,g_lastHr);
    if(!ok) {disconnect("connect-failed");return;}
    policy.connected=true;burstSlot=CreatePeriodicBurst(25,120);
    record("idle CreatePeriodicBurst slot=%d hr=0x%08lX (no Start)",burstSlot,g_lastHr);snapshot();controls();flush();
    SetWindowTextA(statusText,"Connected, idle. Select 5%, 10% or 20%. Each A/B/C click requests ONE 120 ms effect.");
}
struct RuntimeActions {
    HRESULT stopBaseline() {
        ZeroForces(); // Production stops all families, including the constant handle.
        HRESULT hr=g_effect ? g_effect->Stop() : E_FAIL;
        record("baseline explicit constant.Stop hr=0x%08lX",hr);
        effectStatus("baseline-constant-stopped",g_effect);return hr;
    }
    HRESULT constant(int magnitude) { return attended::finiteConstant(g_device,pulse,g_effectAxes,magnitude,focused); }
    HRESULT shaped(int magnitude) {
        if(burstSlot<0)return DIERR_UNSUPPORTED;
        int ok=PlayShapedPeriodicBurst(burstSlot,magnitude,6250,9000,120);
        return !ok && g_lastHr==S_OK ? E_FAIL : g_lastHr;
    }
    bool zero() {
        zeroUpdates++;
        double t=milliseconds();int ok=SetDeviceForcesXY(0,0);
        record("constant-zero-update accepted=%d last_hr_not_update_result=0x%08lX latency_ms=%.3f",ok,g_lastHr,milliseconds()-t);
        return ok!=0;
    }
};
static RuntimeActions actions;
static void start(attended::Mode mode) {
    bool blocked=gameRunning();
    if(blocked) {if(policy.connected)disconnect("game-process-detected");SetWindowTextA(statusText,latestGameCheck.reason);flush();return;}
    if(!policy.begin(mode,true,focused(),blocked,GetTickCount64())) {SetWindowTextA(statusText,"Test blocked: check focus, connection, game processes and cooldown.");flush();return;}
    controls();
    zeroUpdates=0;began=GetTickCount64();
    record("BEGIN mode=%d percent=%d requested_duration_ms=120",(int)mode,policy.percent);
    double t=milliseconds();HRESULT hr=attended::dispatch(mode,policy.magnitude(),actions);
    began=GetTickCount64();nextZero=began+20;
    record("play mode=%d hr=0x%08lX latency_ms=%.3f",(int)mode,hr,milliseconds()-t);
    if(hr!=S_OK) {stopAll("play-rejected");SetWindowTextA(statusText,"Driver rejected test; see raw HRESULT log. No retry or fallback.");return;}
    auto fx=mode==attended::Mode::Constant ? pulse : g_periodic[burstSlot].fx;
    effectStatus("immediately-after-start",fx);
    DWORD state=0;double stateBegin=milliseconds();HRESULT stateHr=g_device->GetForceFeedbackState(&state);
    record("device-state hr=0x%08lX flags=0x%08lX latency_ms=%.3f (PAUSED/STOPPED may be unreliable)",stateHr,state,milliseconds()-stateBegin);
    echo("active",fx,mode==attended::Mode::Constant);
    SetWindowTextA(statusText,"120 ms test active. Finite driver duration; STOP/disconnect or loss of focus cancels.");
}
static void tick() {
    if(!policy.active) {controls();return;}
    if(!focused()) {disconnect("focus-loss");return;}
    ULONGLONG now=GetTickCount64();
    auto fx=policy.mode==attended::Mode::Constant ? pulse : g_periodic[burstSlot].fx;
    if(now-began>=120) {bool missed=policy.zeroUpdate() && zeroUpdates==0;effectStatus("before-timed-stop",fx);stopAll("duration-elapsed");SetWindowTextA(statusText,missed ? "C completed with NO zero updates before expiry: timing comparison inconclusive (see log)." : "Finished. Record what you felt. One-second cooldown; no automatic next test.");return;}
    if(now>=nextZero) {
        effectStatus("before-update",fx);
        if(policy.zeroUpdate()) {
            bool currentFocus=focused();
            if(!policy.mayUpdate(GetTickCount64(),began,currentFocus)) {
                record("zero update skipped: status query consumed test window or focus lost");
                if(!currentFocus) disconnect("focus-loss-after-status");
                else {stopAll("duration-elapsed-after-status");SetWindowTextA(statusText,"C skipped a late update after a slow status query. Review timing; do not infer mixing failure.");}
                return;
            }
            bool ok=attended::updateZero(policy,actions);
            effectStatus("after-zero-update",fx);
            if(!ok) {stopAll("zero-update-failed");return;}
        }
        nextZero=GetTickCount64()+20; // Never catch up a delayed UI timer with extra writes.
    }
}
static HWND child(const char* klass,const char* text,int x,int y,int w,int h,int id,DWORD style=0) {
    HWND window=CreateWindowExA(0,klass,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,ui,(HMENU)(INT_PTR)id,GetModuleHandleA(nullptr),nullptr);
    SendMessage(window,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return window;
}
static LRESULT CALLBACK procedure(HWND h,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_COMMAND:
        if(HIWORD(w)==BN_CLICKED) switch(LOWORD(w)) {
            case 1:scan();break;case 2:connect();break;case 3:disconnect("owner-stop");break;
            case 10:start(attended::Mode::Constant);break;case 11:start(attended::Mode::Shaped);break;case 12:start(attended::Mode::ShapedWithZero);break;
        }
        if(LOWORD(w)==4 && HIWORD(w)==CBN_SELCHANGE) {int i=(int)SendMessage(strength,CB_GETCURSEL,0,0);if(i>=0 && i<=2)policy.select(i==0?5:i==1?10:20);}
        return 0;
    case WM_TIMER:tick();return 0;
    case WM_ACTIVATEAPP:if(w==FALSE && policy.connected) disconnect("focus-loss");return 0;
    case WM_CLOSE:disconnect("window-close");DestroyWindow(h);return 0;
    case WM_QUERYENDSESSION:disconnect("session-ending");return TRUE;
    case WM_DESTROY:KillTimer(h,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcA(h,msg,w,l);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR commandLine,int show) {
    // Read-only preflight exits before UI, DirectInput, environment or log setup.
    if(commandLine && *commandLine) {
        if(strcmp(commandLine,"--check-games")!=0)return 64;
        ProcessReader reader;auto result=attended::checkGames(reader);
        char text[640];sprintf_s(text,"source=%s blocked=%d %s\r\n",PROBE_SOURCE_REV,result.blocked,result.reason);
        DWORD written=0;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,(DWORD)strlen(text),&written,nullptr);
        return result.blocked ? 2 : 0;
    }
    // These affect THIS diagnostic process only; no environment/profile persistence.
    SetEnvironmentVariableA("DBCE_FFB_LOG","0");SetEnvironmentVariableA("DBCE_FFB_TRACE_FORCE","0");
    QueryPerformanceFrequency(&frequency);
    char local[MAX_PATH]={};if(!GetEnvironmentVariableA("LOCALAPPDATA",local,MAX_PATH))return 1;
    char folder[MAX_PATH];sprintf_s(folder,"%s\\DbceWheel",local);CreateDirectoryA(folder,nullptr);
    SYSTEMTIME time;GetLocalTime(&time);sprintf_s(logFile,"%s\\attended-effects-%04u%02u%02u-%02u%02u%02u-%lu.log",folder,time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId());
    WNDCLASSA klass={};klass.lpfnWndProc=procedure;klass.hInstance=instance;klass.lpszClassName="DbceAttendedEffects";klass.hCursor=LoadCursor(nullptr,IDC_ARROW);klass.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassA(&klass);
    ui=CreateWindowA(klass.lpszClassName,"DBCE attended effect comparison - developer diagnostic",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,820,470,nullptr,nullptr,instance,nullptr);
    if(!ui)return 1;
    child("STATIC","Owner-operated only. No force until A/B/C click. Close all games. Default peak 5%; maximum 20%.",20,18,760,24,0);
    scanButton=child("BUTTON","Find wheels",20,52,110,30,1);devices=child("COMBOBOX","",145,52,630,140,5,CBS_DROPDOWNLIST|WS_VSCROLL);
    connectButton=child("BUTTON","Connect selected wheel",20,98,190,32,2);child("BUTTON","STOP / Disconnect",225,98,175,32,3);
    child("STATIC","Matched peak:",425,105,100,24,0);strength=child("COMBOBOX","",530,98,130,100,4,CBS_DROPDOWNLIST);
    for(const char* value:{"5% (default)","10%","20% (cap)"})SendMessageA(strength,CB_ADDSTRING,0,(LPARAM)value);SendMessage(strength,CB_SETCURSEL,0,0);
    buttons[0]=child("BUTTON","A: finite constant X pulse",20,155,245,40,10);
    buttons[1]=child("BUTTON","B: shaped sine alone",280,155,235,40,11);
    buttons[2]=child("BUTTON","C: sine + zero steering updates",530,155,245,40,12);
    child("STATIC","A: 120 ms positive constant pulse, finite in driver (same peak/duration, different impulse).\r\nB/C: 6.25 Hz sine, phase 90 degrees, fade to zero over 120 ms.\r\nC adds production zero-force updates approximately every 20 ms. No repeat/autoplay.\r\nAPI acceptance, echo and PLAYING status do not measure actual wheel torque.",20,214,755,90,0);
    statusText=child("STATIC","Disconnected. Click Find wheels to enumerate without acquiring or applying force.",20,318,755,48,0);
    child("EDIT",logFile,20,380,755,26,0,ES_READONLY|ES_AUTOHSCROLL|WS_BORDER);
    record("harness_source=%s",PROBE_SOURCE_REV);
    record("session adapter_source=c319b0258b11d1f07492a48dbc110d3e50c14dfd native=%d default=5 cap=20 duration=120; API observations not torque",kVersion);
    controls();ShowWindow(ui,show);SetTimer(ui,1,10,nullptr);flush();
    MSG message;while(GetMessage(&message,nullptr,0,0)>0) {TranslateMessage(&message);DispatchMessage(&message);}
    return 0;
}
