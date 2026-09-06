#include "impact_mixer.h"
#include "../telemetry/signal_sample.h"
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv) {
    CHECK(argc == 2);
    std::ifstream input(argv[1]); CHECK(bool(input));
    std::string line; std::getline(input, line); unsigned rows = 0;
    while (std::getline(input, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        double age; float structural, strength, direction, expected;
        std::istringstream row(line); CHECK(bool(row >> age >> structural >> strength >> direction >> expected));
        dbce::force::ImpactMixer mixer;
        CHECK(mixer.trigger(1.f, direction, 0));
        CHECK(std::fabs(mixer.mix(structural, age, strength) - expected) < 1e-6f);
        ++rows;
    }
    CHECK(rows == 8);
    dbce::force::ImpactMixer mixer;
    for (int i = 0; i < 8; ++i) CHECK(mixer.trigger(1.f, 1.f, 0));
    CHECK(!mixer.trigger(1.f, 1.f, 0) && mixer.rejected_events == 1);
    CHECK(mixer.mix(1.f, .005) <= 1.f);
    CHECK(mixer.mix(-1.f, .010) >= -1.f);
    CHECK(mixer.mix(1.f, .001) == 0.f); // rewind cancels all pending impacts
    CHECK(mixer.mix(0.f, .005) == 0.f);
    double area = 0;
    for (int n = 0; n < 1000; ++n) area += dbce::force::ImpactMixer::envelope(n * .0001) * .0001;
    CHECK(std::fabs(area) < 1e-8);
    using namespace dbce::telemetry;
    SignalSample sample;
    CHECK(!sample.usable(0, 1));
    sample.source = Source::hud_ocr; sample.quality = Quality::held;
    sample.unit = Unit::metres_per_second; sample.seconds = 10.; sample.value = 0.;
    CHECK(sample.usable(10.1, .2)); // a valid stopped car is different from unknown
    CHECK(!sample.usable(10.3, .2) && !sample.usable(9., .2));
    sample.value = std::numeric_limits<double>::quiet_NaN(); CHECK(!sample.usable(10., 1));
    std::puts("8 cross-language impact rows, bounded overlap, zero-area envelope and sample validity passed");
}
