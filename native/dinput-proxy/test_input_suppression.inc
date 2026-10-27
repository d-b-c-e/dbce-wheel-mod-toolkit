// Included by the registered production-wrapper fixture; no real device/window.
static bool inputOpen, predicateThrows;
static int predicateCalls, inputObservations;
static DIJOYSTATE2 observedInput;
static bool inputPredicate()
{
    ++predicateCalls;
    if (predicateThrows) throw 1;
    return inputOpen;
}
static void inputObserver(const void *, int layout, DWORD cb, const void *data)
{
    ++inputObservations;
    if (layout == 2 && cb == sizeof observedInput) memcpy(&observedInput, data, cb);
}
template<class I> static void neutralizerChecks()
{
    using namespace dbce::fence;
    settings() = Settings{};
    FakeDeviceImpl<I> fake;
    fake.rawState = true;
    I *dev = new FencedDevice<I>(&fake, kInstText);
    struct Guarded { DIJOYSTATE2 state; unsigned char guard[16]; } got, expected;
    memset(&expected, 0x5a, sizeof expected);
    dev->SetDataFormat(&c_dfDIJoystick2);
    memset(&got, 0x5a, sizeof got);
    check(dev->GetDeviceState(sizeof got.state, &got.state) == DI_OK && !memcmp(&got, &expected, sizeof got), "panel default-null: exact state/canary");
    settings().panelOpen = inputPredicate;
    inputOpen = false; predicateThrows = false;
    dev->GetDeviceState(sizeof got.state, &got.state);
    check(!memcmp(&got, &expected, sizeof got), "panel closed: exact state/canary");
    inputOpen = true;
    settings().observe = inputObserver;
    memset(expected.state.rgbButtons, 0, sizeof expected.state.rgbButtons);
    for (auto &p : expected.state.rgdwPOV) p = (DWORD)-1;
    check(dev->GetDeviceState(sizeof got.state, &got.state) == DI_OK && !memcmp(&got, &expected, sizeof got), "open joystick2: only buttons/POVs changed; all axes and guard exact");
    check(!memcmp(&observedInput, &got.state, sizeof got.state), "observer sees final suppressed state");
    struct { DIJOYSTATE state; unsigned char guard[16]; } narrow, narrowExpected;
    memset(&narrow, 0x5a, sizeof narrow); narrowExpected = narrow;
    memset(narrowExpected.state.rgbButtons, 0, sizeof narrowExpected.state.rgbButtons);
    for (auto &p : narrowExpected.state.rgdwPOV) p = (DWORD)-1;
    dev->SetDataFormat(&c_dfDIJoystick);
    dev->GetDeviceState(sizeof narrow.state, &narrow.state);
    check(!memcmp(&narrow, &narrowExpected, sizeof narrow), "open joystick1: axes and canary exact");
    for (const DIDATAFORMAT *f : {&c_dfDIKeyboard, &c_dfDIMouse, &c_dfDIMouse2}) {
        check(dbce::input::format(f) >= dbce::input::Keyboard, "library keyboard/mouse format recognized");
        unsigned char bytes[288]; memset(bytes, 0x7b, sizeof bytes);
        dev->SetDataFormat(f);
        dev->GetDeviceState(f->dwDataSize, bytes);
        bool zero = true, guard = true;
        for (DWORD i = 0; i < f->dwDataSize; ++i) zero &= bytes[i] == 0;
        for (DWORD i = f->dwDataSize; i < sizeof bytes; ++i) guard &= bytes[i] == 0x7b;
        check(zero && guard, "keyboard/mouse neutral with trailing canary intact");
        std::vector<DIOBJECTDATAFORMAT> objects(f->rgodf, f->rgodf + f->dwNumObjs);
        std::swap(objects[0].dwOfs, objects[1].dwOfs);
        DIDATAFORMAT custom = *f; custom.rgodf = objects.data();
        check(dbce::input::format(&custom) == 0, "same-sized custom keyboard/mouse layout refused");
        dev->SetDataFormat(&custom);
        dev->GetDeviceState(f->dwDataSize, bytes);
        check(bytes[0] == 0x5a && bytes[f->dwDataSize - 1] == 0x5a, "custom keyboard/mouse is not reinterpreted");
    }
    std::vector<DIOBJECTDATAFORMAT> objects(c_dfDIJoystick2.rgodf, c_dfDIJoystick2.rgodf + c_dfDIJoystick2.dwNumObjs);
    std::swap(objects[0].dwOfs, objects[1].dwOfs);
    DIDATAFORMAT custom = c_dfDIJoystick2; custom.rgodf = objects.data();
    dev->SetDataFormat(&custom); dev->GetDeviceState(sizeof got.state, &got.state);
    check(got.state.rgbButtons[0] == 0x5a && got.state.rgdwPOV[0] == 0x5a5a5a5a, "same-size custom joystick untouched");
    dev->SetDataFormat(&c_dfDIJoystick2);
    dev->GetDeviceState(sizeof narrow.state, &narrow.state);
    check(narrow.state.rgbButtons[0] == 0x5a, "mismatched joystick size untouched");
    fake.formatResult = DIERR_INVALIDPARAM;
    check(dev->SetDataFormat(&custom) == DIERR_INVALIDPARAM, "format failure preserved");
    dev->GetDeviceState(sizeof got.state, &got.state);
    check(got.state.rgbButtons[0] == 0, "failed format change retains standard layout");
    fake.formatResult = DI_OK; fake.actionResult = DIERR_INVALIDPARAM;
    check(dev->SetActionMap(nullptr, nullptr, 0) == DIERR_INVALIDPARAM, "action-map failure preserved");
    dev->GetDeviceState(sizeof got.state, &got.state);
    check(got.state.rgbButtons[0] == 0, "failed action map retains layout");
    fake.actionResult = DI_OK;
    dev->SetActionMap(nullptr, nullptr, 0); dev->GetDeviceState(sizeof got.state, &got.state);
    check(got.state.rgbButtons[0] == 0x5a, "successful action map clears layout");
    dev->SetDataFormat(&c_dfDIJoystick2);
    fake.stateResult = DIERR_INPUTLOST;
    const int calls = predicateCalls, observations = inputObservations;
    memset(&got, 0x7b, sizeof got);
    check(dev->GetDeviceState(sizeof got.state, &got.state) == DIERR_INPUTLOST && predicateCalls == calls && inputObservations == observations && got.guard[0] == 0x7b && got.state.rgbButtons[0] == 0x7b, "failed state: HRESULT/bytes/predicate/observer preserved");
    fake.stateResult = DI_OK;
    check(dev->GetDeviceState(sizeof got.state, nullptr) == DIERR_INVALIDPARAM && predicateCalls == calls, "null state forwarded without predicate");

    dbce::controls::InjectionTable injections;
    settings().injections = &injections;
    for (const std::string text : {std::string("inject raw button 40 dev=") + kInstText + " value=1 ms=2000", std::string("inject raw hat 0 9000 dev=") + kInstText + " value=9000 ms=2000"}) {
        dbce::controls::InjectCommand c; std::string why;
        check(dbce::controls::parseInject(text, nullptr, c, why) && injections.add(c, GetTickCount64()).empty(), "panel test injection admitted");
    }
    dev->GetDeviceState(sizeof got.state, &got.state);
    check(got.state.rgbButtons[40] == 0 && got.state.rgdwPOV[0] == (DWORD)-1 && observedInput.rgbButtons[40] == 0, "injection cannot reintroduce button/POV after suppression");
    settings().injections = nullptr;
    fake.rawState = false;
    dbce::controls::Binding brake; std::string shapeWhy;
    dbce::controls::ShapeTable shapes;
    check(dbce::controls::parse(std::string("axis 5 dev=") + kInstText + " range=0..65535 rest=1743 travel=+1", brake, shapeWhy) && shapes.add("brake", brake, shapeWhy), "panel shape fixture admitted");
    settings().shapes = &shapes;
    inputOpen = false; DIJOYSTATE2 shaped{}; dev->GetDeviceState(sizeof shaped, &shaped);
    inputOpen = true; dev->GetDeviceState(sizeof got.state, &got.state);
    check(got.state.lRz == shaped.lRz && observedInput.lRz == shaped.lRz && got.state.lX == shaped.lX, "open suppression preserves shaped axes and observer sees final values");
    settings().shapes = nullptr; fake.rawState = true;
    predicateThrows = true; dev->GetDeviceState(sizeof got.state, &got.state); predicateThrows = false;
    check(got.state.rgbButtons[0] == 0, "throwing predicate contained and suppresses input");
    settings().observe = [](const void *, int, DWORD, const void *) { throw 2; };
    check(dev->GetDeviceState(sizeof got.state, &got.state) == DI_OK && got.state.rgbButtons[0] == 0, "throwing observer preserves suppressed read");
    settings().observe = nullptr;

    DIDEVICEOBJECTDATA event{}; event.dwOfs = DIJOFS_BUTTON(0); event.dwData = 0x80;
    for (DWORD stride : {(DWORD)sizeof(DIDEVICEOBJECTDATA), (DWORD)sizeof(DIDEVICEOBJECTDATA_DX3)}) {
        for (DWORD flags : {(DWORD)0, (DWORD)DIGDD_PEEK}) for (bool countOnly : {false, true}) for (HRESULT result : {DI_OK, DI_BUFFEROVERFLOW}) {
            fake.events = {event, event}; fake.dataResult = result;
            unsigned char storage[2 * sizeof(DIDEVICEOBJECTDATA) + 16]; memset(storage, 0x7b, sizeof storage);
            auto *data = countOnly ? nullptr : reinterpret_cast<LPDIDEVICEOBJECTDATA>(storage);
            DWORD n = countOnly ? INFINITE : 2;
            const DWORD capacity = n;
            check(dev->GetDeviceData(stride, data, &n, flags) == result && n == 0 && fake.dataStride == stride && fake.dataFlags == flags && fake.dataCapacity == capacity && fake.dataPointer == data && fake.countPointer == &n, "buffered open: arguments/HRESULT exact, events hidden including PEEK/count-only/overflow");
            check(fake.events.size() == ((flags & DIGDD_PEEK) ? 2u : 0u), "PEEK never consumes; normal read/drain consumes");
            check(storage[2 * stride] == 0x7b, "buffered caller capacity canary intact");
        }
    }
    fake.events = {event}; fake.dataResult = DI_OK;
    DWORD n = 0;
    check(dev->GetDeviceData(sizeof event, nullptr, &n, 0) == DI_OK && n == 0 && fake.events.size() == 1, "zero capacity does not drain");
    check(dev->GetDeviceData(sizeof event, nullptr, nullptr, 0) == DIERR_INVALIDPARAM, "null count failure preserved");
    n = 1; const int beforeFailure = predicateCalls;
    fake.dataResult = DIERR_INPUTLOST;
    check(dev->GetDeviceData(sizeof event, &event, &n, DIGDD_PEEK) == DIERR_INPUTLOST && n == 1 && fake.events.size() == 1 && predicateCalls == beforeFailure, "buffered failure leaves count/queue/predicate unchanged");
    fake.dataResult = DI_OK;
    n = 1;
    check(dev->GetDeviceData(3, &event, &n, 0) == DIERR_INVALIDPARAM && n == 1, "invalid stride failure preserved");
    check(dev->GetDeviceData(sizeof event, &event, &n, 0x80000000) == DIERR_INVALIDPARAM && n == 1, "invalid flags failure preserved");
    predicateThrows = true;
    check(dev->GetDeviceData(sizeof event, &event, &n, DIGDD_PEEK) == DI_OK && n == 0 && fake.events.size() == 1, "buffered predicate throw contained without PEEK drain");
    predicateThrows = false;
    inputOpen = false; n = 1;
    check(dev->GetDeviceData(sizeof event, &event, &n, DIGDD_PEEK) == DI_OK && n == 1 && event.dwData == 0x80 && fake.events.size() == 1, "close resumes queued PEEK input without consuming");
    settings().panelOpen = nullptr; inputOpen = true; n = 1;
    check(dev->GetDeviceData(sizeof event, &event, &n, 0) == DI_OK && n == 1 && fake.events.empty(), "default-null buffered delivery unchanged");
    settings().panelOpen = inputPredicate;
    for (bool open : {true, false}) {
        inputOpen = open;
        LPDIRECTINPUTEFFECT effect = nullptr;
        DIEFFECT e{}; e.dwSize = sizeof e;
        dev->CreateEffect(GUID_ConstantForce, &e, &effect, nullptr);
        effect->Start(1, 0); effect->Download(); effect->SetParameters(&e, DIEP_START); effect->Stop(); effect->Unload(); effect->Release();
        dev->SendForceFeedbackCommand(DISFFC_SETACTUATORSON); dev->Escape(nullptr); dev->SendDeviceData(0, nullptr, nullptr, 0);
        DIPROPDWORD prop{}; prop.diph.dwSize = sizeof prop; prop.dwData = DIPROPAUTOCENTER_ON;
        dev->SetProperty(DIPROP_AUTOCENTER, &prop.diph); dev->SetProperty(DIPROP_FFGAIN, &prop.diph);
        for (const char *m : {"CreateEffect", "SendForceFeedbackCommand", "Escape", "SendDeviceData", "SetProperty AUTOCENTER on", "SetProperty FFGAIN"}) check(fake.n(m) == 0, std::string("panel cannot open force route: ") + m);
    }
    check(dev->Release() == 0 && fake.refs == 0, "panel wrapper final release retains normal lifetime");
    settings() = Settings{};
}
