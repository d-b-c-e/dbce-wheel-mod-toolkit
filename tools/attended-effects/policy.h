#pragma once
namespace attended {
constexpr int durationMs=120, cooldownMs=1000;
constexpr int levels[]={5,10,20,30,40,50,60,70,80,90,100};
constexpr int levelCount=sizeof(levels)/sizeof(levels[0]);
constexpr int defaultIndex=5;
constexpr int defaultPercent=levels[defaultIndex], maxPercent=levels[levelCount-1];
constexpr int magnitudeCap=maxPercent*100;
inline bool allowedLevel(int value) {for(int level:levels)if(value==level)return true;return false;}
enum class Mode { None, Constant, Shaped, ShapedWithZero };
struct Policy {
    int percent=defaultPercent;
    bool connected=false, active=false;
    unsigned long long until=0;
    Mode mode=Mode::None;
    bool select(int value) { if(active || !allowedLevel(value)) return false; percent=value; return true; }
    bool begin(Mode requested, bool explicitClick, bool focused, bool gameRunning, unsigned long long now) {
        if(!explicitClick || !focused || gameRunning || !connected || active || now<until || requested==Mode::None || !allowedLevel(percent)) return false;
        mode=requested; active=true; until=now+durationMs+cooldownMs; return true;
    }
    void finish(unsigned long long now) { active=false; mode=Mode::None; until=now+cooldownMs; }
    void focusLost(unsigned long long now) { finish(now); connected=false; }
    bool zeroUpdate() const { return active && mode==Mode::ShapedWithZero; }
    bool mayUpdate(unsigned long long now,unsigned long long start,bool focused) const {
        return zeroUpdate() && focused && now>=start && now-start<durationMs;
    }
    int magnitude() const { return percent*100; }
};
template<class Actions>
HRESULT dispatch(Mode mode,int magnitude,Actions& actions) {
    if((mode!=Mode::Constant && mode!=Mode::Shaped && mode!=Mode::ShapedWithZero) || magnitude<1 || magnitude>magnitudeCap) return E_INVALIDARG;
    HRESULT hr=actions.stopBaseline();
    if(FAILED(hr)) return hr;
    return mode==Mode::Constant ? actions.constant(magnitude) : actions.shaped(magnitude);
}
template<class Actions>
bool updateZero(const Policy& policy,Actions& actions) {
    return !policy.zeroUpdate() || actions.zero();
}
// Windows/DirectInput declarations must precede this header. No production export.
template<class Device,class Effect,class Permission>
HRESULT finiteConstant(Device* device,Effect*& effect,DWORD axisCount,int magnitude,Permission mayStart) {
    if(!device || effect || magnitude<1 || magnitude>magnitudeCap || axisCount<1 || axisCount>2) return E_INVALIDARG;
    DWORD axes[2]={DIJOFS_X,DIJOFS_Y}; LONG direction[2]={magnitude,0};
    DICONSTANTFORCE force={magnitude}; DIEFFECT e={}; e.dwSize=sizeof e;
    e.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS; e.dwDuration=durationMs*1000;
    e.dwGain=DI_FFNOMINALMAX; e.dwTriggerButton=DIEB_NOTRIGGER;
    e.cAxes=axisCount; e.rgdwAxes=axes; e.rglDirection=direction;
    e.cbTypeSpecificParams=sizeof force; e.lpvTypeSpecificParams=&force;
    HRESULT hr=device->CreateEffect(GUID_ConstantForce,&e,&effect,nullptr);
    if(hr==S_OK && effect) {
        DIEFFECT check={};check.dwSize=sizeof check;
        hr=effect->GetParameters(&check,DIEP_DURATION);
        if(hr==S_OK && check.dwDuration!=durationMs*1000) hr=DIERR_UNSUPPORTED;
        if(hr==S_OK && !mayStart()) hr=DIERR_NOTACQUIRED;
        if(hr==S_OK) hr=effect->Start(1,0);
    }
    else if(hr==S_OK) hr=E_FAIL;
    if(hr!=S_OK && effect) { effect->Stop(); effect->Release(); effect=nullptr; }
    return hr;
}
}
