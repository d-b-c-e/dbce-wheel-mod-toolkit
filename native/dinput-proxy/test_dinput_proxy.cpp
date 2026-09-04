// Checks for dinput_proxy.h. Nothing here touches a device: the point is that
// the header compiles in both architectures and that the two pieces of pure
// logic - the re-type and the blank-twin filter - do what they claim.
#include "dinput_proxy.h"
#include <cstdio>

using namespace dbce::dproxy;

// The DirectInput 8 vtable layout. Wrong numbers corrupt an unrelated slot,
// which crashes far from the cause, so pin them where a mistake cannot build.
static_assert(DI8_CREATE_DEVICE == 3 && DI8_ENUM_DEVICES == 4, "IDirectInput8 slots");
static_assert(DID8_GET_CAPABILITIES == 3 && DID8_GET_PROPERTY == 5 && DID8_SET_PROPERTY == 6,
              "IDirectInputDevice8 property slots");
static_assert(DID8_GET_DEVICE_STATE == 9 && DID8_SET_DATA_FORMAT == 11 &&
              DID8_GET_DEVICE_INFO == 15 && DID8_CREATE_EFFECT == 18,
              "IDirectInputDevice8 slots");
static_assert(DIE_SET_PARAMETERS == 6, "IDirectInputEffect slots");

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++fails; } } while (0)

int main()
{
    // ---- the re-type ------------------------------------------------------
    // A MOZA R12: 8 axes, so DirectInput calls it 1STPERSON/SIXDOF.
    const DWORD MOZA = 0x0006346e;
    DWORD devType = DI8DEVTYPE_1STPERSON | (DI8DEVTYPE1STPERSON_SIXDOF << 8);

    CHECK(retype_as_wheel(MOZA, MOZA, devType));
    CHECK((devType & 0xFF) == DI8DEVTYPE_DRIVING);
    CHECK(((devType >> 8) & 0xFF) == DI8DEVTYPEDRIVING_THREEPEDALS);

    // Idempotent: something already a wheel is left alone, so hooking both
    // read sites cannot double-apply.
    DWORD again = devType;
    CHECK(!retype_as_wheel(MOZA, MOZA, again));
    CHECK(again == devType);

    // A different device is not touched.
    DWORD other = DI8DEVTYPE_GAMEPAD;
    CHECK(!retype_as_wheel(MOZA, 0x00011234, other));
    CHECK(other == (DWORD)DI8DEVTYPE_GAMEPAD);

    // Product key 0 means "any device".
    DWORD anyDev = DI8DEVTYPE_1STPERSON;
    CHECK(retype_as_wheel(0, 0x12345678, anyDev));
    CHECK((anyDev & 0xFF) == DI8DEVTYPE_DRIVING);

    // The high half - the HID usage bits DirectInput packs above the type -
    // must survive, or the game sees a device that contradicts itself.
    DWORD withHigh = 0xABCD0000u | DI8DEVTYPE_1STPERSON;
    CHECK(retype_as_wheel(MOZA, MOZA, withHigh));
    CHECK((withHigh & 0xFFFF0000u) == 0xABCD0000u);

    // Two pedals rather than three, when that is what the rig has.
    DWORD limited = DI8DEVTYPE_1STPERSON;
    CHECK(retype_as_wheel(MOZA, MOZA, limited, false));
    CHECK(((limited >> 8) & 0xFF) == DI8DEVTYPEDRIVING_LIMITED);

    // ---- the blank-twin filter -------------------------------------------
    int deviceA = 0, deviceB = 0;   // stand-ins for two device objects
    BlankTwinFilter f;

    // Before anything has proved itself, a blank read is accepted: a wheel
    // genuinely at rest must not be filtered out.
    CHECK(f.should_accept(&deviceA, true));

    // A real read makes that object the live one.
    CHECK(f.should_accept(&deviceA, false));

    // Now the twin's blanks are ignored - this is the whole point, since they
    // arrive at poll rate and would erase the real reads.
    CHECK(!f.should_accept(&deviceB, true));

    // The live object's own blanks are still accepted: the wheel really can
    // return to centre with nothing pressed.
    CHECK(f.should_accept(&deviceA, true));

    // And if the twin ever produces something real, it takes over.
    CHECK(f.should_accept(&deviceB, false));
    CHECK(!f.should_accept(&deviceA, true));

    f.reset();
    CHECK(f.should_accept(&deviceB, true));

    // The vtable slot numbers are checked at COMPILE time, at the top of this file: they
    // are ABI, not preferences, and a careless edit should fail the build
    // rather than a test run.

    // ---- the real DLL -----------------------------------------------------
    // system32 must be reachable and export DirectInput8Create. This is the
    // one thing a proxy cannot do without.
    CHECK(load_real_dinput8());
    CHECK(real_create() != NULL);

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("dinput_proxy.h: all checks passed (%d-bit)\n", (int)(sizeof(void*) * 8));
    return 0;
}
