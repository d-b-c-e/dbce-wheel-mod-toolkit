// Internal driver-finite constant effects; no change to the steering handle.
#pragma once
namespace wheel_constant_burst {
template<class Effect> HRESULT discard(Effect*& fx,HRESULT hr) {
    if(fx) {fx->Stop();fx->Release();fx=nullptr;}return hr;
}
template<class Device,class Effect> HRESULT create(Device* device,Effect*& fx,DWORD axesCount,int ms) {
    if(!device || fx || axesCount<1 || axesCount>2 || ms<1 || ms>500)return E_INVALIDARG;
    DWORD axes[2]={DIJOFS_X,DIJOFS_Y};LONG direction[2]={0,0};DICONSTANTFORCE constant={};
    DIEFFECT e={};e.dwSize=sizeof e;e.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS;
    e.dwDuration=ms*1000;e.dwGain=DI_FFNOMINALMAX;e.dwTriggerButton=DIEB_NOTRIGGER;
    e.cAxes=axesCount;e.rgdwAxes=axes;e.rglDirection=direction;
    e.cbTypeSpecificParams=sizeof constant;e.lpvTypeSpecificParams=&constant;
    HRESULT hr=device->CreateEffect(GUID_ConstantForce,&e,&fx,nullptr);
    if(hr!=S_OK || !fx)return discard(fx,hr==S_OK?E_FAIL:hr);
    return S_OK; // Idle, zero magnitude, never Start during creation.
}
template<class Effect> HRESULT play(Effect*& fx,DWORD axesCount,int ms,int magnitude) {
    if(!fx || axesCount<1 || axesCount>2 || ms<1 || ms>500 || magnitude < -10000 || magnitude>10000)return E_INVALIDARG;
    HRESULT hr=fx->Stop();
    if(FAILED(hr))return discard(fx,hr);
    if(magnitude==0)return hr;
    LONG direction[2]={magnitude<0?-(LONG)magnitude:(LONG)magnitude,0};DICONSTANTFORCE constant={magnitude};
    DIEFFECT e={};e.dwSize=sizeof e;e.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS;
    e.cAxes=axesCount;e.rglDirection=direction;e.dwDuration=ms*1000;
    e.cbTypeSpecificParams=sizeof constant;e.lpvTypeSpecificParams=&constant;
    hr=fx->SetParameters(&e,DIEP_DIRECTION|DIEP_TYPESPECIFICPARAMS|DIEP_DURATION);
    if(hr==S_OK) {
        DIEFFECT read={};read.dwSize=sizeof read;
        hr=fx->GetParameters(&read,DIEP_DURATION);
        if(hr==S_OK && read.dwDuration!=e.dwDuration)hr=DIERR_UNSUPPORTED;
    }
    if(hr==S_OK)hr=fx->Start(1,0); // No SOLO, no reacquire or stale-event retry.
    if(hr!=S_OK)return discard(fx,hr);
    return hr;
}
}
