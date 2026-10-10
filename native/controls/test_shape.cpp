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

    long axes[8] = {33265, 0, 0, 0, 0, 1743, 0, 0};
    StateView v;
    v.axes = axes;
    auto range = [](int, long &mn, long &mx) { mn = 0; mx = 65535; return true; };
    check(t.apply("{00000000-0000-0000-0000-0000000000a1}", v, range) == 1 && axes[5] == 0 && axes[0] == 33265, "applied to the brake axis only");
    axes[5] = 1743;
    check(t.apply("{00000000-0000-0000-0000-0000000000b2}", v, range) == 0 && axes[5] == 1743, "another device untouched");
    auto noRange = [](int, long &, long &) { return false; };
    check(t.apply("{00000000-0000-0000-0000-0000000000a1}", v, noRange) == 0 && axes[5] == 1743, "unreadable range: left as read");
    StateView none;
    check(t.apply("{00000000-0000-0000-0000-0000000000a1}", none, range) == 0, "no axes: nothing");
    check(!ShapeTable().any(), "an empty table");

    std::cout << (g_failures ? "FAIL " : "PASS ") << g_checks << " shape checks (" << (sizeof(void *) * 8) << "-bit)\n";
    return g_failures ? 1 : 0;
}
