// Exercises the actual exported read/open functions against fake COM devices.
// No system DirectInput object, window, game or hardware is opened.
#include "../WheelFfb.cpp"
#include <cassert>
#include <initializer_list>

struct FakeDevice : IDirectInputDevice8A {
    DIJOYSTATE2 state = {};
    int polls=0, reads=0, acquires=0, releases=0, unacquires=0;
    HRESULT pollResult=S_OK, readResult=S_OK;
    bool omitPov=false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {++releases;return 1;}
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID,LPDIPROPHEADER) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID,LPCDIPROPHEADER) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Acquire() override {++acquires;return S_OK;}
    HRESULT STDMETHODCALLTYPE Unacquire() override {++unacquires;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size,LPVOID data) override {
        assert(size==sizeof(state));++reads;
        // Deliberately write even on failure: production must not publish it.
        DWORD saved[4]; memcpy(saved,((DIJOYSTATE2*)data)->rgdwPOV,sizeof(saved));
        memcpy(data,&state,sizeof(state));
        if(omitPov)memcpy(((DIJOYSTATE2*)data)->rgdwPOV,saved,sizeof(saved));
        return readResult;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA,DWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT*,LPUNKNOWN) override {assert(false);return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA,REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD) override {assert(false);return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Poll() override {++polls;return pollResult;}
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR,DWORD,LPDIFILEEFFECT,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA) override {return E_NOTIMPL;}
};

struct FakeInput : IDirectInput8A {
    int creates=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override {return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 1;}
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID,LPDIRECTINPUTDEVICE8A*,LPUNKNOWN) override {++creates;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD) override {assert(false);return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID,LPCSTR,LPGUID) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR,LPDIACTIONFORMATA,LPDIENUMDEVICESBYSEMANTICSCBA,LPVOID,DWORD) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK,LPDICONFIGUREDEVICESPARAMSA,DWORD,LPVOID) override {return E_NOTIMPL;}
};

struct Snapshot {
    int before=123, axes[8], between=456;
    unsigned char buttons[128], buttonEnd=78;
    int pov[4], after=789;
    void seed(){for(auto& a:axes)a=99;for(auto& b:buttons)b=1;for(auto& p:pov)p=9000;}
    void cleared(){for(int a:axes)assert(a==0);for(auto b:buttons)assert(b==0);for(int p:pov)assert(p==-1);guards();}
    void guards(){assert(before==123 && between==456 && buttonEnd==78 && after==789);}
    int read(int slot=0){return ReadDeviceStateWithPov(slot,axes,8,buttons,128,pov,4);}
};

int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG","0");
    FakeInput input;FakeDevice wheel,replacement,reader;Snapshot out;
    g_di=&input;g_device=&wheel;g_activeGuid.Data1=1;
    g_auxCount=1;g_aux[0].guid=g_activeGuid;
    assert(OpenReadDevice(0)==0 && g_read[0].isWheel && !g_read[0].dev);
    assert(input.creates==0 && wheel.acquires==0); // Reuse FFB handle, no second device.
    wheel.state.lX=0;wheel.state.lY=65535;wheel.state.lZ=300;
    wheel.state.lRx=400;wheel.state.lRy=500;wheel.state.lRz=600;
    wheel.state.rglSlider[0]=700;wheel.state.rglSlider[1]=800;
    wheel.state.rgbButtons[0]=0x80;wheel.state.rgbButtons[1]=0x7F;wheel.state.rgbButtons[127]=0xFF;
    for(int offset:{0,4500}) {
        for(int i=0;i<4;++i)wheel.state.rgdwPOV[i]=i*9000+offset;
        out.seed();int reads=wheel.reads,polls=wheel.polls;
        assert(out.read()==1 && wheel.reads==reads+1 && wheel.polls==polls+1);
        const int expected[]={0,65535,300,400,500,600,700,800};
        for(int i=0;i<8;++i)assert(out.axes[i]==expected[i]);
        for(int i=0;i<128;++i)assert(out.buttons[i]==(i==0||i==127?1:0));
        for(int i=0;i<4;++i)assert(out.pov[i]==i*9000+offset);
        out.guards();
    }
    const DWORD neutral[]={0xFFFFFFFF,0x0000FFFF,0x1234FFFF,36000};
    memcpy(wheel.state.rgdwPOV,neutral,sizeof(neutral));
    assert(out.read());for(int p:out.pov)assert(p==-1);
    const DWORD boundaries[]={35999,0x80000000,0xFFFF0000,1};
    memcpy(wheel.state.rgdwPOV,boundaries,sizeof(boundaries));
    assert(out.read() && out.pov[0]==35999 && out.pov[1]==-1 && out.pov[2]==-1 && out.pov[3]==1);
    wheel.omitPov=true;assert(out.read());for(int p:out.pov)assert(p==-1);wheel.omitPov=false;
    // Accepted optional-button extents write no bytes outside the declared count.
    assert(ReadDeviceStateWithPov(0,out.axes,8,nullptr,0,out.pov,4));
    out.seed();assert(ReadDeviceStateWithPov(0,out.axes,8,out.buttons,1,out.pov,4));
    assert(out.buttons[0]==1 && out.buttons[1]==1 && out.buttons[127]==1);out.guards();

    for(int slot:{-1,1,8,INT_MAX}){out.seed();assert(!out.read(slot));out.cleared();}
    const int readsBefore=wheel.reads;
    for(int count:{-1,0,7,9,INT_MAX}) {
        out.seed();assert(!ReadDeviceStateWithPov(0,out.axes,count,out.buttons,128,out.pov,4));
        for(int a:out.axes)assert(a==99);for(auto b:out.buttons)assert(b==0);for(int p:out.pov)assert(p==-1);out.guards();
    }
    for(int count:{-1,0,3,5,INT_MAX}) {
        out.seed();assert(!ReadDeviceStateWithPov(0,out.axes,8,out.buttons,128,out.pov,count));
        for(int p:out.pov)assert(p==9000);for(int a:out.axes)assert(a==0);for(auto b:out.buttons)assert(b==0);out.guards();
    }
    for(int count:{-1,129,INT_MAX}) {
        out.seed();assert(!ReadDeviceStateWithPov(0,out.axes,8,out.buttons,count,out.pov,4));
        for(auto b:out.buttons)assert(b==1);for(int a:out.axes)assert(a==0);for(int p:out.pov)assert(p==-1);out.guards();
    }
    assert(!ReadDeviceStateWithPov(0,nullptr,8,out.buttons,128,out.pov,4));
    assert(!ReadDeviceStateWithPov(0,out.axes,8,nullptr,1,out.pov,4));
    assert(!ReadDeviceStateWithPov(0,out.axes,8,out.buttons,128,nullptr,4));
    assert(wheel.reads==readsBefore);

    // Failed Poll alone may still yield a valid state. Neither it nor a failed
    // GetDeviceState ever reacquires the shared FFB wheel or retries its read.
    wheel.pollResult=DIERR_INPUTLOST;assert(out.read() && wheel.acquires==0);
    for(HRESULT error:{(HRESULT)E_FAIL,(HRESULT)DIERR_INPUTLOST,(HRESULT)DIERR_NOTACQUIRED}) {
        wheel.readResult=error;int reads=wheel.reads;out.seed();
        assert(!out.read() && wheel.reads==reads+1 && wheel.acquires==0);out.cleared();
        // Legacy export retains its historical untouched-on-failure behavior.
        out.seed();assert(!ReadDeviceState(0,out.axes,out.buttons,128));
        assert(out.axes[0]==99 && out.buttons[0]==1 && out.pov[0]==9000);
    }
    wheel.readResult=S_OK;wheel.pollResult=S_OK;
    // Switching FFB device cannot retarget an existing slot; same GUID resumes.
    int oldReads=wheel.reads;g_device=&replacement;g_activeGuid.Data1=2;
    out.seed();assert(!out.read());out.cleared();assert(wheel.reads==oldReads && replacement.reads==0);
    g_device=nullptr;out.seed();assert(!out.read());out.cleared();
    g_device=&wheel;g_activeGuid.Data1=1;assert(out.read());
    CloseReadDevices();assert(!wheel.releases && !wheel.unacquires);
    out.seed();assert(!out.read());out.cleared();

    // Independent reader still uses the same owner DirectInput instance and
    // retains the existing non-FFB reacquisition policy, one state call only.
    g_readCount=1;g_read[0].isWheel=false;g_read[0].dev=&reader;
    reader.state=wheel.state;reader.pollResult=DIERR_INPUTLOST;
    assert(out.read() && reader.polls==2 && reader.reads==1 && reader.acquires==1);
    reader.readResult=DIERR_NOTACQUIRED;out.seed();
    assert(!out.read() && reader.reads==2 && reader.acquires==3);out.cleared();
    CloseReadDevices();assert(reader.releases==1 && reader.unacquires==1 && input.creates==0);
    g_di=nullptr;g_device=nullptr;g_auxCount=0;
    puts("PASS production POV snapshot: eight directions/four hats, neutral/invalid angles, bounds/canaries, one read, failure invalidation, shared handle/no reacquire, GUID swap, legacy unchanged; no hardware opened");
}
