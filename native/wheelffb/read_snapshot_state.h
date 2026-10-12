// Included beside g_read, under the existing API/device locks.
struct ReadSnapshotState {
    WheelReadDeviceInfoV1 info{};
    LPDIRECTINPUTDEVICE8 device{};
    uint64_t ffbGeneration{}, sequence{}, revision{};
    bool qualified{};
    wchar_t interfacePath[MAX_PATH]{};
};
static ReadSnapshotState g_readSnapshots[kMaxReadSlots];
static uint64_t g_readSnapshotSerial=0;
static void InvalidateReadSnapshot(int slot) {
    if(slot>=0 && slot<kMaxReadSlots){g_readSnapshots[slot].qualified=false;g_readSnapshots[slot].interfacePath[0]=0;++g_readSnapshots[slot].revision;}
}
