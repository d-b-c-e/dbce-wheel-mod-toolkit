#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "policy.h"
#include "game_guard.h"
struct FakeEffect {
    int starts=0,stops=0,releases=0; DWORD duration=0; unsigned long long now=0,end=0;bool active=false;
    HRESULT startResult=S_OK,readResult=S_OK;bool mismatch=false;
    HRESULT GetParameters(LPDIEFFECT e,DWORD flags) {assert(flags==DIEP_DURATION);e->dwDuration=mismatch?INFINITE:duration;return readResult;}
    HRESULT Start(DWORD iterations,DWORD flags) { assert(iterations==1 && flags==0);starts++;active=startResult==S_OK;end=now+duration;return startResult; }
    HRESULT Stop() {stops++;active=false;return S_OK;}
    ULONG Release() {releases++;active=false;return 0;}
    bool playing() const {return active && now<end;}
};
struct FakeDevice {
    FakeEffect effect;int creates=0;HRESULT result=S_OK;LONG magnitude=0,direction=0;DWORD gain=0,axes=0;
    HRESULT CreateEffect(REFGUID guid,LPCDIEFFECT e,FakeEffect** out,LPUNKNOWN) {
        creates++;assert(IsEqualGUID(guid,GUID_ConstantForce));assert(e->dwDuration==120000);
        assert(e->dwTriggerButton==DIEB_NOTRIGGER && e->dwFlags==(DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS));
        magnitude=((DICONSTANTFORCE*)e->lpvTypeSpecificParams)->lMagnitude;direction=e->rglDirection[0];gain=e->dwGain;axes=e->cAxes;
        assert(e->rgdwAxes[0]==DIJOFS_X && (axes==1 || e->rgdwAxes[1]==DIJOFS_Y));
        effect.duration=e->dwDuration;*out=result==S_OK?&effect:nullptr;return result;
    }
};
struct FakeActions {
    int baselineStops=0,constants=0,shapes=0,zeros=0;HRESULT stopResult=S_OK;
    HRESULT stopBaseline() {baselineStops++;return stopResult;}
    HRESULT constant(int magnitude) {assert(baselineStops>0 && magnitude<=2000);constants++;return S_OK;}
    HRESULT shaped(int magnitude) {assert(baselineStops>0 && magnitude<=2000);shapes++;return S_OK;}
    bool zero() {zeros++;return true;}
};
struct FakeProcesses {
    int index=0;bool firstFailure=false,nextFailure=false;
    attended::ProcessRow rows[3]={{"BorderlessGaming.exe",5364},{"steam.exe",22},{"GravelLauncher.exe",33}};
    unsigned long error() const {return 5;}
    attended::ScanStep first(attended::ProcessRow& row) {row=rows[0];return firstFailure?attended::ScanStep::Error:attended::ScanStep::Item;}
    attended::ScanStep next(attended::ProcessRow& row) {
        if(nextFailure)return attended::ScanStep::Error;
        if(++index==3)return attended::ScanStep::End;
        row=rows[index];return attended::ScanStep::Item;
    }
};
int main() {
    for(const char* helper:{"BorderlessGaming.exe","steam.exe","steamwebhelper.exe","EpicGamesLauncher.exe","GravelLauncher.exe","artofrally-helper.exe","iRacingArcadeUpdater.exe"})assert(!attended::recognizedGame(helper));
    for(const char* game:{"artofrally.exe","ART OF RALLY.EXE","OR2006C2C.exe","Gravel.exe","Gravel-Win64-Shipping.exe","iRacingArcade.exe"})assert(attended::recognizedGame(game));
    FakeProcesses helpers;assert(!attended::checkGames(helpers).blocked);
    FakeProcesses running;running.rows[1]={"artofrally.exe",1234};auto check=attended::checkGames(running);
    assert(check.blocked && strstr(check.reason,"artofrally.exe") && strstr(check.reason,"1234"));
    FakeProcesses failedFirst;failedFirst.firstFailure=true;assert(attended::checkGames(failedFirst).blocked);
    FakeProcesses failedNext;failedNext.nextFailure=true;assert(attended::checkGames(failedNext).blocked);
    using attended::Mode;attended::Policy p;
    assert(p.percent==5 && p.magnitude()==500 && !p.active && !p.connected);
    assert(!p.begin(Mode::Constant,true,true,false,0));p.connected=true;
    assert(!p.begin(Mode::Constant,false,true,false,0)); // No implicit dispatch.
    assert(!p.begin(Mode::Constant,true,false,false,0)); // Background blocked.
    assert(!p.begin(Mode::Constant,true,true,true,0)); // Game process blocks.
    for(int invalid:{-1,0,6,21,40,100}) assert(!p.select(invalid));
    assert(p.begin(Mode::Constant,true,true,false,0) && !p.zeroUpdate());
    assert(!p.select(20) && !p.begin(Mode::Shaped,true,true,false,1));
    p.finish(120);assert(!p.begin(Mode::Shaped,true,true,false,1119));
    assert(p.begin(Mode::Shaped,true,true,false,1120) && !p.zeroUpdate());
    p.focusLost(1130);assert(!p.connected && !p.active);
    assert(!p.begin(Mode::Shaped,true,true,false,9000)); // Focus regain never auto-connects.
    p.connected=true;assert(p.select(20));assert(p.begin(Mode::ShapedWithZero,true,true,false,9000) && p.zeroUpdate());
    assert(p.mayUpdate(9119,9000,true));assert(!p.mayUpdate(9120,9000,true));
    assert(!p.mayUpdate(9119,9000,false) && !p.mayUpdate(8999,9000,true));
    p.finish(9120);assert(!p.zeroUpdate());
    for(int percent:{5,10,20}) for(DWORD axes:{1UL,2UL}) {
        FakeDevice device;FakeEffect* fx=nullptr;
        assert(attended::finiteConstant(&device,fx,axes,percent*100,[]{return true;})==S_OK);
        assert(device.magnitude==percent*100 && device.direction==device.magnitude && device.gain==10000 && device.axes==axes);
        assert(fx->playing());fx->now=119999;assert(fx->playing());fx->now=120000;assert(!fx->playing()); // No UI callback.
        assert(fx->starts==1 && fx->stops==0);
    }
    FakeActions a;
    assert(attended::dispatch(Mode::Constant,500,a)==S_OK && a.baselineStops==1 && a.constants==1 && !a.shapes);
    assert(attended::dispatch(Mode::Shaped,500,a)==S_OK && a.baselineStops==2 && a.shapes==1);
    p.mode=Mode::Shaped;p.active=true;assert(attended::updateZero(p,a) && a.zeros==0);
    assert(attended::dispatch(Mode::ShapedWithZero,500,a)==S_OK && a.baselineStops==3 && a.shapes==2);
    p.mode=Mode::ShapedWithZero;assert(attended::updateZero(p,a) && a.zeros==1);
    p.active=false;assert(attended::updateZero(p,a) && a.zeros==1);
    a.stopResult=E_FAIL;assert(attended::dispatch(Mode::Shaped,500,a)==E_FAIL && a.shapes==2);
    assert(attended::dispatch(Mode::Constant,2001,a)==E_INVALIDARG);
    FakeDevice device;FakeEffect* fx=nullptr;
    for(int invalid:{-1,0,2001,10000}) assert(attended::finiteConstant(&device,fx,2,invalid,[]{return true;})==E_INVALIDARG);
    assert(device.creates==0);
    device.effect.startResult=E_FAIL;assert(attended::finiteConstant(&device,fx,2,500,[]{return true;})==E_FAIL && !fx && device.effect.releases==1 && device.effect.starts==1);
    FakeDevice lostFocus;FakeEffect* blockedFx=nullptr;
    assert(attended::finiteConstant(&lostFocus,blockedFx,2,500,[]{return false;})==DIERR_NOTACQUIRED && !blockedFx && lostFocus.effect.starts==0);
    FakeDevice mismatch;mismatch.effect.mismatch=true;FakeEffect* mismatchedFx=nullptr;
    assert(attended::finiteConstant(&mismatch,mismatchedFx,2,500,[]{return true;})==DIERR_UNSUPPORTED && !mismatchedFx && mismatch.effect.starts==0);
    for(HRESULT adjusted:{(HRESULT)E_FAIL,(HRESULT)DI_TRUNCATED}) {
        FakeDevice rejected;rejected.effect.readResult=adjusted;FakeEffect* rejectedFx=nullptr;
        assert(attended::finiteConstant(&rejected,rejectedFx,2,500,[]{return true;})==adjusted && !rejectedFx && rejected.effect.starts==0);
    }
    puts("PASS: exact game/helper matching and fail-closed scan; explicit dispatch, default5/cap20, focus/game/cooldown gates, finite120ms constant and no SOLO; no DirectInput device created.");
}
