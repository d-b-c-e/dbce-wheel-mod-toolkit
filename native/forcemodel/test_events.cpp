// Semantic invariants, independent of regenerated profile golden outputs.
#include "force_model.h"
#include <cstdio>
#include <limits>
using namespace dbce::force;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
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
