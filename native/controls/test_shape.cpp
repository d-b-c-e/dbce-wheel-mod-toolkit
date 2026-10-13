// Checks dbce_shape.hpp (pedal rest shaping). No game, no device. test_shape.exe
#include "dbce_shape.hpp"
#include <iostream>

using namespace dbce::controls;

static int g_checks, g_failures;
static void check(bool ok, const std::string &what)
{
    ++g_checks;
    if (!ok) { ++g_failures; std::cerr << "FAIL " << what << "\n"; }
}
static Binding must(const std::string &text)
{
    Binding b;
    std::string why;
    check(parse(text, b, why), "parses: " + text + " (" + why + ")");
    return b;
}

static const std::string R12 = " dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";

int main()
{
    const Binding brake = must("axis 5" + R12 + " range=0..65535 rest=1743 travel=+1");
    const Binding throttle = must("axis 2" + R12 + " range=0..65535 rest=0 travel=+1");
    const Binding high = must("axis 1" + R12 + " range=0..65535 rest=64000 travel=-1");
    const Binding inverted = must("axis 6" + R12 + " range=0..65535 rest=1743 travel=+1 inverted");

    // The owner's brake in the game's own range 0..65535: released at its rest, full at the end, linear between.
    check(shapePedal("brake", brake, 1743, 0, 65535) == 0, "brake at rest reads released");
    check(shapePedal("brake", brake, 0, 0, 65535) == 0 && shapePedal("brake", brake, 1000, 0, 65535) == 0, "below rest stays released");
    check(shapePedal("brake", brake, 65535, 0, 65535) == 65535, "brake full reads full");
    check(shapePedal("brake", brake, 33639, 0, 65535) == 32768, "brake halfway reads half (33639 -> 32768)");
    // Another game range (the game sets DIPROP_RANGE; the binding was captured in 0..65535).
    check(shapePedal("brake", brake, 1743 - 32768, -32768, 32767) == -32768, "game range -32768..32767: rest reads its minimum");
    check(shapePedal("brake", brake, 32767, -32768, 32767) == 32767, "game range -32768..32767: full");
    check(shapePedal("brake", brake, 266, 0, 10000) == 0, "game range 0..10000: rest (1743 of 65535 is 266) reads 0");
    check(shapePedal("brake", brake, 10000, 0, 10000) == 10000, "game range 0..10000: full");

    // With the rest at the travel's start, shaping is the identity (and the table refuses it).
    bool identity = true;
    for (long r = 0; r <= 65535; r += 257) identity = identity && shapePedal("throttle", throttle, r, 0, 65535) == r;
    check(identity, "rest at the start: identity across the range");
    check(!needsShape("throttle", throttle) && needsShape("brake", brake), "needsShape");

    // Travel -1 resting near the top: the filter reads the maximum as released, so the rest maps to the maximum.
    check(shapePedal("brake", high, 64000, 0, 65535) == 65535 && shapePedal("brake", high, 0, 0, 65535) == 0, "travel -1: rest and full");
    check(shapePedal("brake", high, 65535, 0, 65535) == 65535, "travel -1: beyond the rest stays released");
    // Inverted (the player's explicit inversion): full at the rest. The filter reads the maximum as released, so the
    // rest maps to the minimum (read as full) and the full end to the maximum (read as released).
    check(shapePedal("clutch", inverted, 1743, 0, 65535) == 0 && shapePedal("clutch", inverted, 65535, 0, 65535) == 65535, "inverted ends");
    check(!releasedAtMin(inverted) && releasedAtMin(brake) && !releasedAtMin(high), "releasedAtMin");

    // Extreme values: no overflow on x86 (long is 32-bit), clamped to the game range.
    const Binding wide = must("axis 7" + R12 + " range=-2147483648..2147483647 rest=-2147483000 travel=+1");
    check(shapePedal("handbrake", wide, 2147483647L, -2147483647L - 1, 2147483647L) == 2147483647L, "full int32 range: full");
    check(shapePedal("handbrake", wide, -2147483647L - 1, -2147483647L - 1, 2147483647L) == -2147483647L - 1, "full int32 range: below rest");
    check(shapePedal("brake", brake, 5, 10, 10) == 5, "an empty game range leaves the sample");

    // The table: pedal axes needing a shape only; one shape per axis; applied to that device's axis only.
    ShapeTable t;
    std::string why;
    check(t.add("brake", brake, why) && t.any(), "brake added");
    check(!t.add("throttle", throttle, why) && why.find("no shape needed") != std::string::npos, "throttle refused: no shape needed");
    check(!t.add("steer", must("axis 0" + R12 + " range=0..65535 rest=33265 travel=-1"), why) && why == "not a pedal axis", "steer refused");
    check(!t.add("confirm", must("button 31" + R12), why) && why == "not a pedal axis", "a button refused");
    check(!t.add("clutch", must("axis 5" + R12 + " range=0..65535 rest=1743 travel=+1"), why) && why.find("already shaped") != std::string::npos, "a second shape on one axis refused");
    check(!t.add("brake", must("axis 4" + R12 + " range=0..65535 rest=65535 travel=+1"), why) && why == "rest-at-full", "rest at the full end refused");

    const std::string a1 = "{00000000-0000-0000-0000-0000000000a1}", b2 = "{00000000-0000-0000-0000-0000000000b2}";
    // A binding built in code (not by parse()) must still be well formed: normalized identities, travel exactly +-1.
    auto malformed = [&](void (*edit)(Binding &), const char *reason) {
        Binding b = brake;
        b.index = 3;   // another axis, so the duplicate rule does not answer first
        edit(b);
        ShapeTable fresh;
        std::string w;
        return !fresh.add("brake", b, w) && w == reason;
    };
    check(malformed([](Binding &b) { b.dev = ""; }, "bad-dev"), "empty dev refused");
    check(malformed([](Binding &b) { b.dev = "{000000A1-0000-0000-0000-000000000000}"; }, "bad-dev"), "upper-case dev refused (parse stores lowercase)");
    check(malformed([](Binding &b) { b.dev = "000000a1-0000-0000-0000-000000000000"; }, "bad-dev"), "dev without braces refused");
    check(malformed([](Binding &b) { b.prod = "{0006346e}"; }, "bad-prod"), "malformed prod refused");
    check(malformed([](Binding &b) { b.travel = 0; }, "bad-travel"), "travel 0 refused");
    check(malformed([](Binding &b) { b.travel = 2; }, "bad-travel"), "travel +2 refused");
    check(malformed([](Binding &b) { b.index = 8; }, "bad-index"), "axis 8 refused");
    check(malformed([](Binding &b) { b.rest = 70000; }, "rest outside the range"), "rest beyond the range refused");

    // Observation: requested is not applied until a read was shaped; misses keep their reason; the status lines say so.
    {
        ShapeTable o;
        o.add("brake", brake, why);
        check(describe(o)[0].find("not seen yet") != std::string::npos && o.state(0).shaped == 0, "requested: not seen yet");
        long ax[8] = {0, 0, 0, 0, 0, 1743, 0, 0};
        StateView sv; sv.axes = ax;
        int firsts = 0;
        auto first = [&](const PedalShape &, ShapeMiss, long, long) { ++firsts; };
        auto ok = [](int, long &mn, long &mx) { mn = 0; mx = 65535; return true; };
        auto bad = [](int, long &, long &) { return false; };
        o.apply("{00000000-0000-0000-0000-0000000000a1}", sv, bad, first);
        o.apply("{00000000-0000-0000-0000-0000000000a1}", sv, bad, first);
        check(firsts == 1 && o.state(0).unavailable == 2 && o.state(0).lastMiss == ShapeMiss::Range, "a miss is reported once per reason, counted every time");
        check(describe(o)[0].find("NOT corrected") != std::string::npos, "status: not corrected: " + describe(o)[0]);
        o.apply("{00000000-0000-0000-0000-0000000000a1}", sv, ok, first);
        o.apply("{00000000-0000-0000-0000-0000000000a1}", sv, ok, first);
        check(firsts == 2 && o.state(0).shaped == 2, "the first correction is reported once");
        check(describe(o)[0].find("corrected, but some reads were not") != std::string::npos, "status: corrected after a miss");
        o.missAll("{00000000-0000-0000-0000-0000000000a1}", ShapeMiss::Format, first);
        check(firsts == 3 && o.state(0).lastMiss == ShapeMiss::Format, "a new miss reason is reported");
        o.noteUnidentified();
        check(describe(o).size() == 2 && describe(o)[1].find("could not identify") != std::string::npos, "unidentified devices listed");
    }

    long axes[8] = {33265, 0, 0, 0, 0, 1743, 0, 0};
    StateView v;
    v.axes = axes;
    auto range = [](int, long &mn, long &mx) { mn = 0; mx = 65535; return true; };
    ShapeResult r = t.apply(a1, v, range);
    check(r.shaped == 1 && r.unavailable == 0 && axes[5] == 0 && axes[0] == 33265, "applied to the brake axis only");
    axes[5] = 1743;
    r = t.apply(b2, v, range);
    check(r.shaped == 0 && r.unavailable == 0 && axes[5] == 1743, "another device untouched");
    r = t.apply("{00000000-0000-0000-0000-0000000000A1}", v, range);
    check(r.shaped == 0 && axes[5] == 1743, "an instance spelled differently (not the stored lowercase form) is not matched");
    // A required shape whose range cannot be read, or is not increasing, is reported unavailable and left as read.
    auto noRange = [](int, long &, long &) { return false; };
    r = t.apply(a1, v, noRange);
    check(r.shaped == 0 && r.unavailable == 1 && axes[5] == 1743, "unreadable range: unavailable, left as read");
    auto flat = [](int, long &mn, long &mx) { mn = 100; mx = 100; return true; };
    auto backwards = [](int, long &mn, long &mx) { mn = 65535; mx = 0; return true; };
    check(t.apply(a1, v, flat).unavailable == 1 && t.apply(a1, v, backwards).unavailable == 1 && axes[5] == 1743, "flat or reversed range: unavailable");
    StateView none;
    check(t.apply(a1, none, range).unavailable == 1, "no axes: unavailable");
    check(!ShapeTable().any(), "an empty table");

    // The same axis on two devices: each gets its own shape, applied only to its own device.
    ShapeTable two;
    const Binding otherBrake = must("axis 5 dev={00000000-0000-0000-0000-0000000000b2} prod={05310483-0000-0000-0000-504944564944} range=0..65535 rest=3000 travel=+1");
    check(two.add("brake", brake, why) && two.add("clutch", otherBrake, why), "same axis index on two devices accepted");
    long first[8] = {0, 0, 0, 0, 0, 1743, 0, 0}, second[8] = {0, 0, 0, 0, 0, 3000, 0, 0};
    StateView v1, v2;
    v1.axes = first; v2.axes = second;
    check(two.apply(a1, v1, range).shaped == 1 && two.apply(b2, v2, range).shaped == 1 && first[5] == 0 && second[5] == 0, "each device's rest reads released");
    first[5] = 3000;
    two.apply(a1, v1, range);
    check(first[5] == shapePedal("brake", brake, 3000, 0, 65535) && first[5] > 0, "the first device keeps its own rest (3000 is pressed there)");

    // Both travel directions with and without the player's inversion, at rest, half and full (game range 0..65535).
    struct Case { const char *text; long rest, half, full; long atRest, atHalf, atFull; };
    const Case cases[] = {
        {" range=0..65535 rest=1743 travel=+1", 1743, 33639, 65535, 0, 32768, 65535},
        {" range=0..65535 rest=1743 travel=+1 inverted", 1743, 33639, 65535, 0, 32768, 65535},   // filter released at max
        {" range=0..65535 rest=63792 travel=-1", 63792, 31896, 0, 65535, 32768, 0},
        {" range=0..65535 rest=63792 travel=-1 inverted", 63792, 31896, 0, 65535, 32768, 0},    // filter released at min
    };
    for (const Case &c : cases) {
        const Binding b = must("axis 4" + R12 + c.text);
        const std::string name = std::string("travel/inversion") + c.text;
        check(shapePedal("brake", b, c.rest, 0, 65535) == c.atRest, name + ": rest");
        check(shapePedal("brake", b, c.half, 0, 65535) == c.atHalf, name + ": half");
        check(shapePedal("brake", b, c.full, 0, 65535) == c.atFull, name + ": full");
        // What the game's end-anchored filter then reads equals the contract's normalized value.
        for (long raw : {c.rest, c.half, c.full}) {
            const long shaped = shapePedal("brake", b, raw, 0, 65535);
            const double filterReads = releasedAtMin(b) ? shaped / 65535.0 : (65535 - shaped) / 65535.0;
            check(std::fabs(filterReads - normalize("brake", b, raw)) < 1e-4, name + ": filter reads the contract value at " + std::to_string(raw));
        }
    }

    // Schema 2 `cal` (mufa 62480 F1): the shape follows the measured released and full, not rest and travel.
    {
        auto must2 = [](const std::string &text) { Binding b; std::string why; check(parse(text, b, why, 2), "parses (schema 2): " + text + " (" + why + ")"); return b; };
        // Rest at the travel's start, so schema 1 would need no shape, but full measured at 60000: the shape is needed.
        const Binding shortFull = must2("axis 2" + R12 + " range=0..65535 rest=0 travel=+1 cal=0..60000");
        check(needsShape("throttle", shortFull) && releasedAtMin(shortFull), "cal stopping short of the end needs a shape");
        check(shapePedal("throttle", shortFull, 60000, 0, 65535) == 65535 && shapePedal("throttle", shortFull, 0, 0, 65535) == 0 &&
              shapePedal("throttle", shortFull, 30000, 0, 65535) == 32768, "cal 0..60000: full at the measured full, half between");
        ShapeTable t2; std::string w;
        check(t2.add("throttle", shortFull, w), "the table takes it (" + w + ")");
        // cal exactly the device ends: the identity, refused as no shape needed.
        const Binding ends = must2("axis 2" + R12 + " range=0..65535 rest=0 travel=+1 cal=0..65535");
        check(!needsShape("throttle", ends), "cal at both device ends: no shape");
        // A reversed calibration on a pedal whose legacy fields disagree (rest at the full end, travel +1): cal decides the
        // direction (released at the maximum), as fits() and normalize() do.
        const Binding reversed = must2("axis 5" + R12 + " range=0..65535 rest=65535 travel=+1 cal=64000..2000");
        check(!releasedAtMin(reversed) && needsShape("brake", reversed), "reversed cal: released at the maximum, shape needed");
        check(shapePedal("brake", reversed, 64000, 0, 65535) == 65535 && shapePedal("brake", reversed, 2000, 0, 65535) == 0,
              "reversed cal: released reads the maximum, full the minimum");
        for (long raw : {64000L, 33000L, 2000L}) {
            const long shaped = shapePedal("brake", reversed, raw, 0, 65535);
            const double filterReads = releasedAtMin(reversed) ? shaped / 65535.0 : (65535 - shaped) / 65535.0;
            check(std::fabs(filterReads - normalize("brake", reversed, raw)) < 1e-4, "reversed cal: filter reads the contract value at " + std::to_string(raw));
        }
        // Read as schema 1, the same text keeps its old rule (cal ignored).
        check(!needsShape("throttle", must("axis 2" + R12 + " range=0..65535 rest=0 travel=+1 cal=0..60000")), "schema 1 ignores cal");
    }
    // F2: a parsed-but-unfitted cal is refused explicitly, never indexed past its points nor read as schema 1: neutral
    // (normalize 0) and the binding's rest (denormalize), for every value.
    {
        Binding two, three, outside; std::string why;
        check(parse("axis 0" + R12 + " range=0..65535 rest=33265 travel=+1 cal=1200..60000", two, why, 2) && fits("steer", two) == "bad-calibration",
              "a 2-point cal on steer parses and does not fit");
        check(parse("axis 2" + R12 + " range=0..65535 rest=0 travel=+1 cal=1000..33265..64000", three, why, 2) && fits("throttle", three) == "bad-calibration",
              "a 3-point cal on a pedal parses and does not fit");
        check(parse("axis 0" + R12 + " range=0..65535 rest=33265 travel=+1 cal=0..300..65535", outside, why, 2) && fits("steer", outside) == "bad-calibration",
              "a too-narrow steer cal parses and does not fit");
        bool neutral = true;
        for (long raw : {0L, 1200L, 33265L, 49151L, 65535L}) {
            neutral = neutral && normalize("steer", two, raw) == 0.0 && normalize("throttle", three, raw) == 0.0 && normalize("steer", outside, raw) == 0.0;
        }
        for (double n : {-1.0, -0.5, 0.0, 0.5, 1.0})
            neutral = neutral && denormalize("steer", two, n) == 33265 && denormalize("throttle", three, n) == 0 && denormalize("steer", outside, n) == 33265;
        check(neutral, "unfitted cal: neutral normalize and the rest on denormalize, for every value");
    }

    std::cout << (g_failures ? "FAIL " : "PASS ") << g_checks << " shape checks (" << (sizeof(void *) * 8) << "-bit)\n";
    return g_failures ? 1 : 0;
}
