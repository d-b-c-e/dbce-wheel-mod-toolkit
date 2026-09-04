// conformance.cpp - pins the C++ force model to the C# one.
//
// Both implementations run every shipped profile through the same deterministic
// input sequence and must reproduce conformance.expected.txt. If they both
// match the golden file they match each other, and any change in how a game
// feels shows up as a diff in the repo rather than as a surprise at the wheel.
//
//   conformance.exe            check against the golden file (exit 1 on drift)
//   conformance.exe --update   rewrite it, deliberately
//
// Regenerating is a decision: the diff is the record of what changed.
#include "force_profile.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cmath>

using namespace dbce::force;

// The input sequence. Deterministic, and it exercises every term: a steering
// sweep through centre, a speed ramp, lateral load, slip past the ideal angle,
// a deepening drift, braking then accelerating, surface texture, one impact and
// one gear shift.
static Inputs tick_inputs(int n)
{
    float t = (float)n / 200.f;                     // 0..1 over the run
    Inputs i;
    i.steer = std::sin(t * 6.2831853f);             // one full sweep
    i.has_steer = true;
    i.steer_rate = std::cos(t * 6.2831853f) * 6.2831853f;
    i.has_steer_rate = true;
    i.speed_mps = 2.f + 38.f * t;                   // 7 to 144 km/h
    i.lateral_g = 1.4f * std::sin(t * 3.14159265f);
    i.has_lateral_g = true;
    i.front_lateral_force = 9000.f * std::sin(t * 3.14159265f);
    i.has_front_lateral_force = (n % 2) == 0;       // exercise both lateral sources
    i.longitudinal_g = -0.8f + 1.6f * t;
    i.has_longitudinal_g = true;
    i.front_slip_angle_deg = 14.f * t;
    i.ideal_slip_angle_deg = 8.f;
    i.has_slip = true;
    i.drift_amount = t;
    i.has_drift = true;
    i.texture = (n % 20 < 10) ? 0.7f : 0.f;
    i.impact = (n == 50) ? 0.9f : 0.f;
    i.impact_direction = (n == 50) ? -1.f : 0.f;
    i.gear_shift = (n == 100);
    return i;
}

static const int kTicks = 200;
static const float kDt = 1.f / 60.f;

// The settings, in the same order as ForceProfile.KeyNames.
static void dump_settings(std::ostringstream& out, const Profile& p)
{
    const ModelSettings& m = p.model;
    const ShaperSettings& s = p.shaper;
    const struct { const char* k; float v; } rows[] = {
        { "model.spring.strength", m.spring_strength },
        { "model.spring.fullSpeedMps", m.spring_full_speed_mps },
        { "model.damper.strength", m.damper_strength },
        { "model.damper.staticFraction", m.damper_static_fraction },
        { "model.lateral.weight", m.lateral_weight },
        { "model.lateral.forceReference", m.lateral_force_reference },
        { "model.lateral.gReference", m.lateral_g_reference },
        { "model.lateral.trailFloor", m.trail_floor },
        { "model.lateral.trailSlipSpan", m.trail_slip_span },
        { "model.lateral.gripLoss", m.grip_loss },
        { "model.load.weightTransfer", m.weight_transfer },
        { "model.load.transferMin", m.weight_transfer_min },
        { "model.load.transferMax", m.weight_transfer_max },
        { "model.texture.strength", m.texture_strength },
        { "model.texture.hz", m.texture_hz },
        { "model.impact.strength", m.impact_strength },
        { "model.impact.seconds", m.impact_seconds },
        { "model.shift.strength", m.shift_strength },
        { "model.shift.seconds", m.shift_seconds },
        { "shaper.strength", (float)s.strength },
        { "shaper.invert", s.invert ? 1.f : 0.f },
        { "shaper.deadzone", s.deadzone },
        { "shaper.attackSmoothing", s.attack_smoothing },
        { "shaper.decaySmoothing", s.decay_smoothing },
        { "shaper.softSaturation", s.soft_saturation },
        { "shaper.slewPerSecond", s.slew_per_second },
        { "shaper.outputDeadband", s.output_deadband },
        { "shaper.fadeStartKmh", s.fade_start_kmh },
        { "shaper.fadeFullKmh", s.fade_full_kmh },
        { "shaper.rampSeconds", s.ramp_seconds },
        { "shaper.peakLimit", s.peak_limit },
    };
    char line[160];
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        std::snprintf(line, sizeof line, "S %s %.6f\n", rows[i].k, rows[i].v);
        out << line;
    }
}

static std::string build_dump(const std::string& profileText)
{
    std::ostringstream out;
    out << "# dbce force-model conformance. Regenerate deliberately: conformance --update\n";
    std::vector<std::string> ids = list_profile_ids(profileText);
    for (size_t k = 0; k < ids.size(); ++k) {
        Profile p; std::string why;
        if (!parse_profile(profileText, ids[k], p, &why)) {
            out << "# FAILED " << ids[k] << ": " << why << "\n";
            continue;
        }
        out << "P " << p.id() << "\n";
        dump_settings(out, p);

        Model model(p.model);
        Shaper shaper(p.shaper);
        char line[128];
        for (int n = 0; n < kTicks; ++n) {
            Inputs in = tick_inputs(n);
            float f = model.compute(in, kDt);
            float g = shaper.shape(f, in.speed_mps * 3.6f, kDt, model.last_was_event);
            std::snprintf(line, sizeof line, "T %d %.6f %.6f\n", n, f, g);
            out << line;
        }
    }
    return out.str();
}

// Numeric compare with a tolerance: tanh and sin differ in the last bits
// between runtimes, and that is not drift worth failing on.
static bool compare(const std::string& got, const std::string& want, std::string& firstDiff)
{
    std::istringstream a(got), b(want);
    std::string la, lb;
    int lineNo = 0;
    while (true) {
        bool ga = (bool)std::getline(a, la), gb = (bool)std::getline(b, lb);
        if (!ga && !gb) return true;
        ++lineNo;
        if (!ga || !gb) {
            std::ostringstream m; m << "line " << lineNo << ": one side ended early";
            firstDiff = m.str(); return false;
        }
        if (!la.empty() && la[la.size() - 1] == '\r') la.erase(la.size() - 1);
        if (!lb.empty() && lb[lb.size() - 1] == '\r') lb.erase(lb.size() - 1);
        if (la == lb) continue;
        if (!la.empty() && !lb.empty() && la[0] == 'T' && lb[0] == 'T') {
            int na = 0, nb = 0; float fa = 0, ga2 = 0, fb = 0, gb2 = 0;
            if (std::sscanf(la.c_str(), "T %d %f %f", &na, &fa, &ga2) == 3 &&
                std::sscanf(lb.c_str(), "T %d %f %f", &nb, &fb, &gb2) == 3 &&
                na == nb && std::fabs(fa - fb) < 1e-4f && std::fabs(ga2 - gb2) < 1e-4f)
                continue;
        }
        std::ostringstream m;
        m << "line " << lineNo << ":\n    got  " << la << "\n    want " << lb;
        firstDiff = m.str();
        return false;
    }
}

int main(int argc, char** argv)
{
    bool update = (argc > 1 && std::strcmp(argv[1], "--update") == 0);
    const std::string here = "..";                       // build/ -> native/forcemodel
    std::string profiles;
    const char* candidates[] = { "../../profiles/force-profiles.ini",
                                 "../../../profiles/force-profiles.ini",
                                 "profiles/force-profiles.ini" };
    std::string found;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
        if (detail::read_file(candidates[i], profiles)) { found = candidates[i]; break; }
    if (found.empty()) { std::printf("cannot find profiles/force-profiles.ini\n"); return 1; }

    std::string dump = build_dump(profiles);

    const char* goldenPaths[] = { "../conformance.expected.txt",
                                  "../../conformance.expected.txt",
                                  "native/forcemodel/conformance.expected.txt" };
    std::string goldenPath;
    std::string golden;
    for (size_t i = 0; i < sizeof(goldenPaths) / sizeof(goldenPaths[0]); ++i) {
        if (detail::read_file(goldenPaths[i], golden)) { goldenPath = goldenPaths[i]; break; }
    }
    if (goldenPath.empty()) goldenPath = "../conformance.expected.txt";

    if (update) {
        std::ofstream f(goldenPath.c_str(), std::ios::binary);
        if (!f) { std::printf("cannot write %s\n", goldenPath.c_str()); return 1; }
        f << dump;
        std::printf("wrote %s (%d profiles, %d ticks each)\n",
                    goldenPath.c_str(), (int)list_profile_ids(profiles).size(), kTicks);
        return 0;
    }

    if (golden.empty()) {
        std::printf("no golden file at %s - run with --update first\n", goldenPath.c_str());
        return 1;
    }
    std::string diff;
    if (!compare(dump, golden, diff)) {
        std::printf("force model output differs from %s\n  %s\n", goldenPath.c_str(), diff.c_str());
        std::printf("\nIf the change was intended, rerun with --update and commit the diff.\n");
        return 1;
    }
    std::printf("force_model.h: matches conformance.expected.txt (%d profiles x %d ticks)\n",
                (int)list_profile_ids(profiles).size(), kTicks);
    return 0;
}
