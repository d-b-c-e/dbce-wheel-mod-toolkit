// Optional bridge exports. No COM object escapes; no new device is opened.
// Ported onto native-0.9.1 (c8ec2ee) from ecc49bd + 64ccf67: this baseline has no external FFB session
// and no test-injection table, so their guards are absent; the reads and checks are otherwise unchanged.
static_assert(sizeof(WheelReadDeviceInfoV1)==144,"read metadata ABI");
static_assert(sizeof(WheelReadSnapshotV1)==232,"read snapshot ABI");
struct ReadSnapshotReference {
    LPDIRECTINPUTDEVICE8 device;
    explicit ReadSnapshotReference(LPDIRECTINPUTDEVICE8 value):device(value){device->AddRef();}
    ~ReadSnapshotReference(){device->Release();}
};
static int64_t ReadSnapshotTime() {
    LARGE_INTEGER frequency{},now{};
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0 || !QueryPerformanceCounter(&now))return -1;
    return (now.QuadPart/frequency.QuadPart)*1000000 + (now.QuadPart%frequency.QuadPart)*1000000/frequency.QuadPart;
}
static LPDIRECTINPUTDEVICE8 ReadSnapshotDevice(int slot) {
    if(slot<0 || slot>=g_readCount)return nullptr;
    const auto& s=g_read[slot];
    if(s.isWheel && (!g_device || !IsEqualGUID(s.guid,g_activeGuid)))return nullptr;
    return s.isWheel?g_device:s.dev;
}
static uint8_t ReadSnapshotCall(HRESULT hr) {
    if(SUCCEEDED(hr))return WHEEL_READ_CALL_OK;
    if(hr==E_PENDING)return WHEEL_READ_CALL_PENDING;
    if(hr==DIERR_INPUTLOST || hr==DIERR_NOTACQUIRED)return WHEEL_READ_CALL_LOST;
    return WHEEL_READ_CALL_GONE;
}
__declspec(dllexport) int GetReadDeviceInfoV1(int slot,WheelReadDeviceInfoV1* out) {
    if(!out || out->struct_size!=sizeof(*out) || out->abi!=1)return WHEEL_READ_BAD_ARGUMENT;
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    *out={};out->struct_size=sizeof(*out);out->abi=1;
    auto dev=ReadSnapshotDevice(slot);
    if(!dev){InvalidateReadSnapshot(slot);return WHEEL_READ_UNQUALIFIED;}
    ReadSnapshotReference keepAlive(dev);
    auto& cached=g_readSnapshots[slot];
    const auto ownerGeneration=g_read[slot].isWheel?g_ffbReadGeneration.load():0;
    if(cached.qualified && cached.device==dev && cached.ffbGeneration==ownerGeneration){*out=cached.info;return WHEEL_READ_OK;}
    InvalidateReadSnapshot(slot);
    const auto revision=cached.revision;
    DIDEVICEINSTANCE before{};before.dwSize=sizeof(before);
    const GUID empty{};
    if(FAILED(dev->GetDeviceInfo(&before)) || !IsEqualGUID(before.guidInstance,g_read[slot].guid) ||
        IsEqualGUID(before.guidInstance,empty) || IsEqualGUID(before.guidProduct,empty))return WHEEL_READ_UNQUALIFIED;
    WheelReadDeviceInfoV1 candidate{};candidate.struct_size=sizeof(candidate);candidate.abi=1;
    memcpy(candidate.instance,&before.guidInstance,16);memcpy(candidate.product,&before.guidProduct,16);
    candidate.flags=g_read[slot].isWheel?1:0;
    const DWORD offsets[8]={DIJOFS_X,DIJOFS_Y,DIJOFS_Z,DIJOFS_RX,DIJOFS_RY,DIJOFS_RZ,DIJOFS_SLIDER(0),DIJOFS_SLIDER(1)};
    for(int i=0;i<8;++i){
        DIDEVICEOBJECTINSTANCE object{};object.dwSize=sizeof(object);
        if(SUCCEEDED(dev->GetObjectInfo(&object,offsets[i],DIPH_BYOFFSET))){
            candidate.axis_mask|=1u<<i;
            DIPROPRANGE range{};range.diph.dwSize=sizeof(range);range.diph.dwHeaderSize=sizeof(range.diph);
            range.diph.dwObj=offsets[i];range.diph.dwHow=DIPH_BYOFFSET;
            if(SUCCEEDED(dev->GetProperty(DIPROP_RANGE,&range.diph)) && range.lMin<range.lMax){
                candidate.range_mask|=1u<<i;candidate.minimum[i]=range.lMin;candidate.maximum[i]=range.lMax;
            }
        }
    }
    for(int i=0;i<128;++i){DIDEVICEOBJECTINSTANCE object{};object.dwSize=sizeof(object);
        if(SUCCEEDED(dev->GetObjectInfo(&object,DIJOFS_BUTTON(i),DIPH_BYOFFSET)))candidate.button_mask[i/8]|=uint8_t(1u<<(i%8));}
    for(int i=0;i<4;++i){DIDEVICEOBJECTINSTANCE object{};object.dwSize=sizeof(object);
        if(SUCCEEDED(dev->GetObjectInfo(&object,DIJOFS_POV(i),DIPH_BYOFFSET)))candidate.pov_mask|=1u<<i;}
    DIDEVICEINSTANCE after{};after.dwSize=sizeof(after);
    if(FAILED(dev->GetDeviceInfo(&after)) || !IsEqualGUID(before.guidInstance,after.guidInstance) ||
        !IsEqualGUID(before.guidProduct,after.guidProduct) || ReadSnapshotDevice(slot)!=dev || cached.revision!=revision ||
        (g_read[slot].isWheel && ownerGeneration!=g_ffbReadGeneration.load()))return WHEEL_READ_UNQUALIFIED;
    candidate.generation=++g_readSnapshotSerial;
    cached.info=candidate;cached.device=dev;cached.ffbGeneration=ownerGeneration;cached.sequence=0;cached.qualified=true;
    *out=candidate;return WHEEL_READ_OK;
}
__declspec(dllexport) int GetReadDevicePathV1(int slot,uint64_t expectedGeneration,uint16_t* path,int capacity) {
    if(path && capacity>0)path[0]=0;
    if(!path || capacity<=0 || capacity>32768 || !expectedGeneration)return WHEEL_READ_BAD_ARGUMENT;
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    auto dev=ReadSnapshotDevice(slot);
    if(!dev){InvalidateReadSnapshot(slot);return WHEEL_READ_UNQUALIFIED;}
    ReadSnapshotReference keepAlive(dev);
    auto& cached=g_readSnapshots[slot];
    if(!cached.qualified || cached.device!=dev || cached.info.generation!=expectedGeneration ||
        (g_read[slot].isWheel && cached.ffbGeneration!=g_ffbReadGeneration.load()))return WHEEL_READ_UNQUALIFIED;
    const auto revision=cached.revision;
    DIPROPGUIDANDPATH value{};value.diph.dwSize=sizeof(value);value.diph.dwHeaderSize=sizeof(value.diph);value.diph.dwHow=DIPH_DEVICE;
    // Some drivers return success without writing a complete path. A sentinel
    // catches missing termination rather than trusting a zero-filled remainder.
    for(auto& ch:value.wszPath)ch=wchar_t(0xffff);
    if(FAILED(dev->GetProperty(DIPROP_GUIDANDPATH,&value.diph)))return WHEEL_READ_UNQUALIFIED;
    size_t length=0;while(length<MAX_PATH && value.wszPath[length])++length;
    DIDEVICEINSTANCE after{};after.dwSize=sizeof(after);
    if(length==0 || length==MAX_PATH || FAILED(dev->GetDeviceInfo(&after)) ||
        memcmp(cached.info.instance,&after.guidInstance,16) || memcmp(cached.info.product,&after.guidProduct,16) ||
        ReadSnapshotDevice(slot)!=dev || !cached.qualified || cached.revision!=revision ||
        cached.info.generation!=expectedGeneration ||
        (g_read[slot].isWheel && cached.ffbGeneration!=g_ffbReadGeneration.load())){
        InvalidateReadSnapshot(slot);return WHEEL_READ_UNQUALIFIED;
    }
    if(cached.interfacePath[0] && wcscmp(cached.interfacePath,value.wszPath)!=0){InvalidateReadSnapshot(slot);return WHEEL_READ_UNQUALIFIED;}
    if(length+1>size_t(capacity))return WHEEL_READ_BAD_ARGUMENT;
    memcpy(cached.interfacePath,value.wszPath,(length+1)*sizeof(wchar_t));
    static_assert(sizeof(wchar_t)==sizeof(uint16_t),"Windows UTF-16 path");
    memcpy(path,value.wszPath,(length+1)*sizeof(uint16_t));return WHEEL_READ_OK;
}
__declspec(dllexport) int ReadDeviceSnapshotV1(int slot,WheelReadSnapshotV1* out) {
    if(!out || out->struct_size!=sizeof(*out) || out->abi!=1)return WHEEL_READ_BAD_ARGUMENT;
    std::lock_guard<std::recursive_mutex> apiGuard(g_apiMutex);
    *out={};out->struct_size=sizeof(*out);out->abi=1;
    out->poll_call=out->state_call=WHEEL_READ_CALL_NOT_CALLED;
    for(auto& p:out->pov)p=0xFFFFFFFFu;
    auto dev=ReadSnapshotDevice(slot);
    if(!dev){InvalidateReadSnapshot(slot);return WHEEL_READ_UNQUALIFIED;}
    ReadSnapshotReference keepAlive(dev);
    auto& cached=g_readSnapshots[slot];
    if(!cached.qualified || cached.device!=dev || (g_read[slot].isWheel && cached.ffbGeneration!=g_ffbReadGeneration.load())){
        InvalidateReadSnapshot(slot);return WHEEL_READ_UNQUALIFIED;
    }
    const auto revision=cached.revision;
    out->generation=cached.info.generation;out->sequence=++cached.sequence;
    out->read_start_us=ReadSnapshotTime();
    if(out->read_start_us<0){InvalidateReadSnapshot(slot);return WHEEL_READ_UNQUALIFIED;}
    DIJOYSTATE2 state{};for(auto& p:state.rgdwPOV)p=0xFFFFFFFFu;
    HRESULT poll=dev->Poll();out->poll_hresult=poll;out->poll_call=ReadSnapshotCall(poll);
    if(SUCCEEDED(poll)){
        HRESULT read=dev->GetDeviceState(sizeof(state),&state);out->state_hresult=read;out->state_call=ReadSnapshotCall(read);
        out->valid=SUCCEEDED(read)?1:0;
    }
    out->read_end_us=ReadSnapshotTime();
    if(out->read_end_us<out->read_start_us || ReadSnapshotDevice(slot)!=dev || cached.revision!=revision ||
        (g_read[slot].isWheel && cached.ffbGeneration!=g_ffbReadGeneration.load()))out->valid=0;
    if(out->valid){
        const LONG* axes=&state.lX;for(int i=0;i<8;++i)out->axes[i]=axes[i];
        memcpy(out->buttons,state.rgbButtons,128);memcpy(out->pov,state.rgdwPOV,16);
    }else InvalidateReadSnapshot(slot);
    return WHEEL_READ_OK;
}
