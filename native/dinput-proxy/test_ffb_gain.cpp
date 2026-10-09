// Fixtures for dbce_ffb_gain.hpp with the values from the 2026-10-08 22:22 muted log: the game's rumble Square
// (magnitude 10000, gain 0, period 25000 then 0), its Damper (+k/-k 100) and its Constant force.
#include "dbce_ffb_gain.hpp"
#include <cstdio>
#include <cstdlib>

static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

using namespace dbce::ffb;

int main() {
    // Square at gain 0 (how the game silences it): silent at any period.
    TypeParams sq{}; sq.periodic = {10000, 0, 0, 25000};
    TypeParams o = applyGain(K_PERIODIC, sq, paramBytes(K_PERIODIC, sizeof(DIPERIODIC)), 0);
    CHECK(o.periodic.dwMagnitude == 0 && o.periodic.lOffset == 0 && o.periodic.dwPeriod == 25000);
    // The same Square with period 0 is silent even at full gain (undefined wave).
    sq.periodic.dwPeriod = 0; sq.periodic.lOffset = 3000;
    o = applyGain(K_PERIODIC, sq, sizeof(DIPERIODIC), 10000);
    CHECK(o.periodic.dwMagnitude == 0 && o.periodic.lOffset == 0);
    // Half gain halves magnitude and offset; full gain changes nothing; over-range gain clamps.
    sq.periodic = {8000, -2000, 0, 25000};
    o = applyGain(K_PERIODIC, sq, sizeof(DIPERIODIC), 5000); CHECK(o.periodic.dwMagnitude == 4000 && o.periodic.lOffset == -1000);
    o = applyGain(K_PERIODIC, sq, sizeof(DIPERIODIC), 10000); CHECK(o.periodic.dwMagnitude == 8000 && o.periodic.lOffset == -2000);
    o = applyGain(K_PERIODIC, sq, sizeof(DIPERIODIC), 65535); CHECK(o.periodic.dwMagnitude == 10000);   // 2x cap, then the wheel's maximum

    // Constant: signed magnitude scales; gain 0 gives 0 (the force slider at 0).
    TypeParams c{}; c.constant.lMagnitude = -10000;
    CHECK(applyGain(K_CONSTANT, c, sizeof(DICONSTANTFORCE), 2500).constant.lMagnitude == -2500);
    CHECK(applyGain(K_CONSTANT, c, sizeof(DICONSTANTFORCE), 0).constant.lMagnitude == 0);
    // Ramp: both ends scale.
    TypeParams r{}; r.ramp = {-6000, 6000};
    o = applyGain(K_RAMP, r, sizeof(DIRAMPFORCE), 5000); CHECK(o.ramp.lStart == -3000 && o.ramp.lEnd == 3000);

    // Damper from the log (+k/-k 100, saturation 10000): coefficients scale, offset/saturation/dead band unchanged.
    TypeParams d{}; d.condition[0] = {0, 100, 100, 10000, 10000, 0}; d.condition[1] = {50, 4000, -4000, 9000, 9000, 10};
    o = applyGain(K_CONDITION, d, paramBytes(K_CONDITION, 2 * sizeof(DICONDITION)), 5000);
    CHECK(o.condition[0].lPositiveCoefficient == 50 && o.condition[0].lNegativeCoefficient == 50 && o.condition[0].dwPositiveSaturation == 10000);
    CHECK(o.condition[1].lPositiveCoefficient == 2000 && o.condition[1].lNegativeCoefficient == -2000 && o.condition[1].lOffset == 50 && o.condition[1].lDeadBand == 10);
    // A single-axis condition scales only the one entry it has.
    o = applyGain(K_CONDITION, d, paramBytes(K_CONDITION, sizeof(DICONDITION)), 0);
    CHECK(o.condition[0].lPositiveCoefficient == 0 && o.condition[1].lPositiveCoefficient == 4000);

    // Sizes: too-short buffers are not handled (passed through unchanged by the proxy).
    CHECK(paramBytes(K_CONSTANT, 2) == 0 && paramBytes(K_PERIODIC, sizeof(DIPERIODIC) - 1) == 0 && paramBytes(K_OTHER, 64) == 0);
    CHECK(paramBytes(K_CONDITION, sizeof(DICONDITION)) == sizeof(DICONDITION) && paramBytes(K_CONDITION, 3 * sizeof(DICONDITION)) == 0 && paramBytes(K_CONDITION, 2 * sizeof(DICONDITION)) == 2 * sizeof(DICONDITION));

    // Strength: 50% is the game's own level; 100% doubles, capped at the wheel's maximum; 0% silences.
    CHECK(combinedGain(10000, 50) == 10000 && combinedGain(10000, 100) == 20000 && combinedGain(10000, 0) == 0 && combinedGain(4000, 75) == 6000);
    CHECK(combinedGain(65535, 50) == 10000 && combinedGain(10000, 300) == 20000 && combinedGain(10000, -5) == 0 && combinedGain(0, 100) == 0);
    c.constant.lMagnitude = 7000;
    CHECK(applyGain(K_CONSTANT, c, sizeof(DICONSTANTFORCE), combinedGain(10000, 100)).constant.lMagnitude == 10000);
    c.constant.lMagnitude = -3000;
    CHECK(applyGain(K_CONSTANT, c, sizeof(DICONSTANTFORCE), combinedGain(10000, 100)).constant.lMagnitude == -6000);
    CHECK(applyGain(K_CONSTANT, c, sizeof(DICONSTANTFORCE), combinedGain(10000, 25)).constant.lMagnitude == -1500);
    // The game's rumble silenced by its gain stays silent at any strength.
    sq.periodic = {10000, 0, 0, 25000};
    CHECK(applyGain(K_PERIODIC, sq, sizeof(DIPERIODIC), combinedGain(0, 100)).periodic.dwMagnitude == 0);

    // Envelope levels scale with the gain.
    DIENVELOPE env{sizeof(DIENVELOPE), 10000, 100000, 6000, 200000};
    DIENVELOPE eo = applyGain(env, 5000);
    CHECK(eo.dwAttackLevel == 5000 && eo.dwFadeLevel == 3000 && eo.dwAttackTime == 100000 && eo.dwFadeTime == 200000);
    std::printf("PASS %d effect-gain checks\n", checks);
    return 0;
}
