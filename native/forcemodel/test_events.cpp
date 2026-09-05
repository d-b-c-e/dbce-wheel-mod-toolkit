// Semantic invariants, independent of regenerated profile golden outputs.
#include "force_profile.h"
#include <cstdio>
#include <limits>
using namespace dbce::force;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv) {
    // Exercise actual file loading as well as the text-parser golden tests.
    // The un-terminated directory catches platform-specific path joins.
    if (argc != 2) { std::puts("usage: events profiles-directory"); return 2; }
    Profile loaded; std::string why;
    CHECK(load_profile_dir(argv[1], "cruisn-vunit@2", loaded, &why));
    CHECK(loaded.name == "cruisn-vunit" && loaded.version == 2);
    CHECK(load_profile_dir(std::string(argv[1]) + "/", "cruisn-vunit@2", loaded, &why));
    CHECK(!load_profile_dir(argv[1], "missing-profile@1", loaded, &why));
    ModelSettings s; s.impact_strength = .5f;
    Model m(s); Inputs i; i.has_steer = true; i.impact = 1.f;
    CHECK(std::fabs(m.compute(i, .01f)) == .5f);
    CHECK(m.last_was_event);
    m.reset(); CHECK(!m.last_was_event && m.last_structural == 0.f);
    i.impact = 0.f; CHECK(m.compute(i, .01f) == 0.f);
    for (int hz : {30, 60, 250, 1000}) {
        RiseDetector d; int hits = 0;
        for (int n = 0; n < hz * 3; ++n)
            if (d.observe(n < hz ? 0.f : 1.f, double(n) / hz)) ++hits;
        CHECK(hits == 1);
    }
    RiseDetector d;
    for (int n = 0; n < 100; ++n) CHECK(!d.observe(1.f, n * .01));
    d.reset(); int hits = 0;
    for (int n = 0; n <= 10; ++n) if (d.observe(n / 10.f, n * .01)) ++hits;
    CHECK(hits == 1);
    d.reset(); CHECK(!d.observe(0.f, 10)); CHECK(!d.observe(1.f, 9));
    CHECK(!d.observe(std::numeric_limits<float>::quiet_NaN(), 11));
    CHECK(!d.observe(1.f, 12)); CHECK(!d.observe(0.f, 13)); CHECK(d.observe(1.f, 13.1));
    CHECK(!d.observe(1.f, 13.1));
    std::puts("event invariants passed (30/60/250/1000 Hz, center impact, invalid time)");
    return 0;
}
