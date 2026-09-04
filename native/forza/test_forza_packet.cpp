// Offset and size conformance test for forza_packet.h. Mirrors the C# tests in
// Dbce.Wheel.Telemetry.Tests so both encoders are pinned to the same bytes.
#include "forza_packet.h"
#include <cstdio>
#include <cstdlib>

using namespace dbce::forza;

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++fails; } } while (0)

static float f32(const uint8_t* b, int off) { float f; std::memcpy(&f, b + off, 4); return f; }

int main()
{
    Sled s; std::memset(&s, 0, sizeof s);
    s.isRaceOn = 1; s.timestampMs = 1234; s.engineMaxRpm = 7500.f; s.engineIdleRpm = 900.f; s.currentEngineRpm = 4321.f;
    s.onRumble[0] = 1;
    Dash d; std::memset(&d, 0, sizeof d);
    d.speed = 33.5f; d.gear = 3; d.accel = 200; d.brake = 10; d.steer = -50; d.posX = 1; d.posY = 2; d.posZ = 3; d.lapNumber = 7; d.racePosition = 2;

    uint8_t buf[400];

    // sizes
    CHECK(build(FORZA_SLED_232, s, d, buf, sizeof buf) == 232);
    CHECK(build(FORZA_FM7_DASH_311, s, d, buf, sizeof buf) == 311);
    CHECK(build(FORZA_HORIZON_324, s, d, buf, sizeof buf, 0x52) == 324);
    CHECK(build(FORZA_HORIZON_324, s, d, buf, 323) == 0);          // too small is refused

    // Horizon anchors validated live: Speed@256, Gear@319, Steer@320, Accel@315, Brake@316, sentinel@323
    build(FORZA_HORIZON_324, s, d, buf, sizeof buf, 0x52);
    CHECK(f32(buf, 256) == 33.5f);
    CHECK(buf[319] == 3);
    CHECK((int8_t)buf[320] == -50);
    CHECK(buf[315] == 200 && buf[316] == 10);
    CHECK(buf[323] == 0x52);
    CHECK(f32(buf, 244) == 1.f && f32(buf, 248) == 2.f && f32(buf, 252) == 3.f);
    for (int i = 232; i < 244; ++i) CHECK(buf[i] == 0);            // the insert stays zero
    CHECK(*(int32_t*)(buf + 116) == 1);                            // rumble strip is s32

    // FM7: everything in the dash sits 12 bytes earlier
    uint8_t fm7[400]; build(FORZA_FM7_DASH_311, s, d, fm7, sizeof fm7);
    CHECK(f32(fm7, 244) == 33.5f);
    CHECK(fm7[307] == 3);
    CHECK((int8_t)fm7[308] == -50);
    CHECK(fm7[303] == 200 && fm7[304] == 10);
    for (int i = 0; i < 232; ++i) CHECK(fm7[i] == buf[i]);        // identical sled

    // sled-only layout carries the header
    uint8_t sled[400]; build(FORZA_SLED_232, s, d, sled, sizeof sled);
    CHECK(*(int32_t*)(sled + 0) == 1 && *(uint32_t*)(sled + 4) == 1234u && f32(sled, 8) == 7500.f);

    // idle keeper: live, zero, gear 1, engine constants, sentinel K
    uint8_t idle[400]; CHECK(build_idle(FORZA_HORIZON_324, 7500.f, 700.f, 100, idle, sizeof idle) == 324);
    CHECK(*(int32_t*)(idle + 0) == 1 && f32(idle, 8) == 7500.f && f32(idle, 12) == 700.f && f32(idle, 256) == 0.f && idle[319] == 1 && idle[323] == 0x4B);

    CHECK(pedal01(0.5f) == 128 && pedal01(2.f) == 255 && pedal01(-1.f) == 0);
    CHECK(steer11(-1.f) == -127 && steer11(1.f) == 127 && steer11(0.f) == 0);

    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("forza_packet.h: all checks passed (232 / 311 / 324; Speed@256, Gear@319)\n");
    return 0;
}
