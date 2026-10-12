// Reuse the existing fake COM surface. The included production legacy test is
// renamed and is not this executable's entry point. No real input object opens.
#define main prior_pov_test_main
#include "pov_read_test.cpp"
#undef main
#include <functional>
#include <cstddef>

struct SnapshotDevice:FakeDevice {
    GUID instance{},product{};
    int infos=0,properties=0,objects=0,addrefs=0,sets=0;
    bool identityFail=false,productChanges=false,rangeFail=false;
    bool pathFail=false,pathUnterminated=false;
    std::wstring path=L"\\\\?\\hid#fixture#slot0";
    std::function<void()> onRead,onPath;
    ULONG STDMETHODCALLTYPE AddRef() override {++addrefs;return 1;}
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA out) override {
        ++infos;if(identityFail)return E_FAIL;
        out->guidInstance=instance;out->guidProduct=product;
        if(productChanges && infos%2==0)++out->guidProduct.Data1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA out,DWORD offset,DWORD how) override {
        ++objects;assert(how==DIPH_BYOFFSET);out->dwOfs=offset;
        // Deliberately sparse: never infer axes/buttons from an aggregate count.
        if(offset==DIJOFS_X || offset==DIJOFS_Z){out->dwType=DIDFT_ABSAXIS;return S_OK;}
        if(offset==DIJOFS_BUTTON(2) || offset==DIJOFS_BUTTON(127)){out->dwType=DIDFT_PSHBUTTON;return S_OK;}
        if(offset==DIJOFS_POV(1)){out->dwType=DIDFT_POV;return S_OK;}
        return DIERR_OBJECTNOTFOUND;
    }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID property,LPDIPROPHEADER header) override {
        if(&property==&DIPROP_GUIDANDPATH){
            assert(header->dwHow==DIPH_DEVICE && header->dwSize==sizeof(DIPROPGUIDANDPATH));
            if(pathFail)return E_FAIL;
            auto* value=reinterpret_cast<DIPROPGUIDANDPATH*>(header);
            if(!pathUnterminated)wcscpy_s(value->wszPath,path.c_str());
            if(onPath)onPath();return S_OK;
        }
        ++properties;assert(&property==&DIPROP_RANGE);assert(header->dwHow==DIPH_BYOFFSET);
        if(rangeFail && header->dwObj==DIJOFS_Z)return E_FAIL;
        auto* range=reinterpret_cast<DIPROPRANGE*>(header);range->lMin=10;range->lMax=65000;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID,LPCDIPROPHEADER) override {++sets;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size,LPVOID data) override {
        const auto result=FakeDevice::GetDeviceState(size,data);if(onRead)onRead();return result;
    }
};
static WheelReadDeviceInfoV1 info() {WheelReadDeviceInfoV1 v{};v.struct_size=sizeof(v);v.abi=1;return v;}
static WheelReadSnapshotV1 sample() {WheelReadSnapshotV1 v{};v.struct_size=sizeof(v);v.abi=1;return v;}
int main() {
    SetEnvironmentVariableA("DBCE_FFB_LOG","0");
    static_assert(offsetof(WheelReadDeviceInfoV1,generation)==8,"generation offset");
    static_assert(offsetof(WheelReadDeviceInfoV1,minimum)==80,"range offset");
    static_assert(offsetof(WheelReadSnapshotV1,axes)==52,"axes offset");
    static_assert(offsetof(WheelReadSnapshotV1,pov)==212,"POV offset");
    FakeInput input;SnapshotDevice wheel,other;
    wheel.instance.Data1=1;wheel.product.Data1=2;other.instance.Data1=3;other.product.Data1=4;
    g_di=&input;g_device=&wheel;g_activeGuid=wheel.instance;g_auxCount=1;g_aux[0].guid=wheel.instance;
    assert(OpenReadDevice(0)==0 && input.creates==0 && wheel.acquires==0);
    auto data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_UNQUALIFIED && wheel.polls==0);
    auto metadata=info();wheel.rangeFail=true;
    assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK);
    assert(metadata.generation && metadata.flags==1 && metadata.axis_mask==5 && metadata.range_mask==1);
    assert(metadata.button_mask[0]==4 && metadata.button_mask[15]==128 && metadata.pov_mask==2);
    assert(metadata.minimum[0]==10 && metadata.maximum[0]==65000 && metadata.maximum[2]==0);
    assert(!memcmp(metadata.instance,&wheel.instance,16) && !memcmp(metadata.product,&wheel.product,16));
    assert(wheel.infos==2 && wheel.objects==140 && wheel.properties==2 && wheel.sets==0);
    const auto generation=metadata.generation;metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK);
    assert(metadata.generation==generation && wheel.infos==2); // Cached within the same qualified generation.
    uint16_t path[MAX_PATH]{};
    assert(GetReadDevicePathV1(0,generation,path,MAX_PATH)==WHEEL_READ_OK);
    assert(std::wstring(reinterpret_cast<wchar_t*>(path))==wheel.path);
    assert(GetReadDevicePathV1(0,generation+1,path,MAX_PATH)==WHEEL_READ_UNQUALIFIED && path[0]==0);
    assert(GetReadDevicePathV1(0,generation,path,2)==WHEEL_READ_BAD_ARGUMENT && path[0]==0);
    assert(GetReadDevicePathV1(0,0,path,MAX_PATH)==WHEEL_READ_BAD_ARGUMENT);
    assert(GetReadDevicePathV1(0,generation,nullptr,MAX_PATH)==WHEEL_READ_BAD_ARGUMENT);
    assert(wheel.polls==0 && wheel.reads==0 && input.creates==0 && wheel.acquires==0);
    wheel.state.lX=40000;wheel.state.lZ=22000;wheel.state.rgbButtons[2]=0x80;wheel.state.rgdwPOV[1]=4500;
    data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_OK && data.valid);
    assert(data.generation==generation && data.sequence==1 && data.read_start_us>0 && data.read_end_us>=data.read_start_us);
    assert(data.axes[0]==40000 && data.axes[2]==22000 && data.buttons[2]==0x80 && data.pov[1]==4500);
    assert(data.poll_call==0 && data.state_call==0 && wheel.polls==1 && wheel.reads==1 && wheel.acquires==0);
    auto first=data;data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_OK && data.sequence==2);
    assert(data.read_start_us>=first.read_end_us && first.sequence==1 && first.axes[0]==40000);
    wheel.pollResult=DIERR_INPUTLOST;const auto reads=wheel.reads;data=sample();
    assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_OK && !data.valid && data.poll_call==WHEEL_READ_CALL_LOST && data.state_call==WHEEL_READ_CALL_NOT_CALLED);
    assert(wheel.reads==reads && wheel.acquires==0 && data.axes[0]==0 && data.buttons[2]==0 && data.pov[1]==0xFFFFFFFFu);
    wheel.pollResult=S_OK;data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_UNQUALIFIED);
    metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK && metadata.generation>generation);
    wheel.readResult=E_FAIL;data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_OK && !data.valid && data.state_call==WHEEL_READ_CALL_GONE);
    assert(data.axes[0]==0 && data.buttons[2]==0);wheel.readResult=S_OK;
    metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK);const auto recovered=metadata.generation;
    // Same pointer and GUID after recreation must not masquerade as the old handle.
    ++g_ffbReadGeneration;data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_UNQUALIFIED);
    metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK && metadata.generation>recovered);
    g_device=&other;g_activeGuid=other.instance;data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_UNQUALIFIED && other.reads==0);
    g_device=&wheel;g_activeGuid=wheel.instance;
    InvalidateReadSnapshot(0);wheel.identityFail=true;metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_UNQUALIFIED);
    wheel.identityFail=false;wheel.productChanges=true;wheel.infos=0;metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_UNQUALIFIED);
    wheel.productChanges=false;metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK);
    assert(GetReadDevicePathV1(0,metadata.generation,path,MAX_PATH)==WHEEL_READ_OK);
    wheel.path=L"\\\\?\\hid#different-epoch";
    assert(GetReadDevicePathV1(0,metadata.generation,path,MAX_PATH)==WHEEL_READ_UNQUALIFIED && path[0]==0);
    data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_UNQUALIFIED);
    metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK);
    wheel.pathFail=true;assert(GetReadDevicePathV1(0,metadata.generation,path,MAX_PATH)==WHEEL_READ_UNQUALIFIED && path[0]==0);wheel.pathFail=false;
    wheel.pathUnterminated=true;assert(GetReadDevicePathV1(0,metadata.generation,path,MAX_PATH)==WHEEL_READ_UNQUALIFIED && path[0]==0);wheel.pathUnterminated=false;
    metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK);
    wheel.onPath=[] {CloseReadDevices();};
    assert(GetReadDevicePathV1(0,metadata.generation,path,MAX_PATH)==WHEEL_READ_UNQUALIFIED && path[0]==0);
    wheel.onPath={};assert(OpenReadDevice(0)==0);metadata=info();assert(GetReadDeviceInfoV1(0,&metadata)==WHEEL_READ_OK);
    // Closing the borrowed reader during the driver's callback cannot publish
    // its stale return; an AddRef holds the actual COM object until return.
    wheel.onRead=[] {CloseReadDevices();};data=sample();assert(ReadDeviceSnapshotV1(0,&data)==WHEEL_READ_OK && !data.valid);
    wheel.onRead={};assert(wheel.unacquires==0 && wheel.acquires==0 && wheel.sets==0 && wheel.forceCreates==0 && input.creates==0);
    assert(wheel.addrefs==wheel.releases);
    auto invalid=info();invalid.struct_size=143;assert(GetReadDeviceInfoV1(0,&invalid)==WHEEL_READ_BAD_ARGUMENT && invalid.struct_size==143);
    invalid=info();invalid.abi=2;assert(GetReadDeviceInfoV1(0,&invalid)==WHEEL_READ_BAD_ARGUMENT);
    assert(GetReadDeviceInfoV1(0,nullptr)==WHEEL_READ_BAD_ARGUMENT && ReadDeviceSnapshotV1(0,nullptr)==WHEEL_READ_BAD_ARGUMENT);
    data=sample();assert(ReadDeviceSnapshotV1(-1,&data)==WHEEL_READ_UNQUALIFIED && !data.valid);
    g_di=nullptr;g_device=nullptr;g_auxCount=0;
    std::puts("PASS: snapshot ABI, sparse objects/ranges, actual identity, one read, generations, failures, reentry and zero device/force acquisition");
}
