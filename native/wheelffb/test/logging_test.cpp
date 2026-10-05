// Runs production force updates against a fake COM effect; never acquires hardware.
#include "../WheelFfb.cpp"
#include <cassert>
#include <string>
#include <fstream>
#include <iterator>
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
        magnitude=(DWORD)((DICONSTANTFORCE*)e->lpvTypeSpecificParams)->lMagnitude;
        assert(e->cAxes==2 && e->rglDirection[0]==labs((long)(LONG)magnitude));
        assert(f==(DIEP_DIRECTION|DIEP_TYPESPECIFICPARAMS|DIEP_START));
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

int main(int argc, char** argv) {
    assert(argc==2);
    const bool trace=strcmp(argv[1],"on")==0, masterOff=strcmp(argv[1],"master-off")==0;
    assert(trace || masterOff || strcmp(argv[1],"off")==0 || strcmp(argv[1],"invalid")==0);
    SetEnvironmentVariableA("DBCE_FFB_LOG", masterOff ? "0" : nullptr);
    SetEnvironmentVariableA("DBCE_FFB_TRACE_FORCE", trace || masterOff ? "1" : strcmp(argv[1],"invalid")==0 ? "true" : nullptr);
    char dir[MAX_PATH], path[MAX_PATH]; assert(GetTempPathA(MAX_PATH,dir)); assert(GetTempFileNameA(dir,"ffb",0,path));
    strncpy_s(g_logPath,path,_TRUNCATE);
    assert(EnsureEffectLock());
    FakeEffect effect; g_effect=&effect;
    Log("lifecycle test marker");
    LARGE_INTEGER frequency,start,end; QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    const int samples=6000;
    for(int i=0;i<samples;i++) {
        int x=(i%2) ? -1234 : 4321;
        assert(SetDeviceForcesXY(x,0)==1);
        assert((LONG)effect.magnitude==x);
    }
    QueryPerformanceCounter(&end);
    // Repeated identical force still reaches the device but creates no extra trace.
    assert(SetDeviceForcesXY(-1234,0)==1 && effect.sets==samples+1);
    Log("bounded error test marker");
    g_effect=nullptr; 
    std::ifstream file(path); std::string data((std::istreambuf_iterator<char>(file)),{}); file.close();
    size_t count=0,pos=0; while((pos=data.find("SetDeviceForcesXY(",pos))!=std::string::npos) {count++;pos++;}
    assert(count==(trace ? samples : 0));
    assert(masterOff ? data.empty() : data.find("lifecycle test marker")!=std::string::npos && data.find("bounded error test marker")!=std::string::npos);
    assert(DeleteFileA(path));
    printf("%s: %d production updates, %zu force log lines, %.3f ms; fake output unchanged, no device opened\n",argv[1],samples,count,1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart);
}
