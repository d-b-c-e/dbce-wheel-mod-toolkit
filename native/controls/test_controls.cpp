// Runs controls-vectors.txt against dbce_controls.hpp, plus the [Controls] section reader. No game, no device.
// test_controls.exe [vectors file]
#include "dbce_controls.hpp"
#include "dbce_inject.hpp"
#include "fixtures/pcmod_axis_calibration.hpp"
#include <cmath>
#include <fstream>
#include <iostream>

using namespace dbce::controls;

static int g_checks, g_failures;
static void check(bool ok, const std::string &what)
{
    ++g_checks;
    if (!ok) { ++g_failures; std::cerr << "FAIL " << what << "\n"; }
}
static std::vector<std::string> fields(const std::string &line)
{
    std::vector<std::string> f;
    size_t p = 0;
    for (;;) {
        size_t q = line.find(" | ", p);
        if (q == std::string::npos) { f.push_back(line.substr(p)); break; }
        f.push_back(line.substr(p, q - p));
        p = q + 3;
    }
    return f;
}
static Binding must(const std::string &text, const std::string &where, int schema = 1)
{
    Binding b;
    std::string why;
    check(parse(text, b, why, schema), where + ": vector binding parses (" + why + ")");
    return b;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "controls-vectors.txt";
    std::ifstream in(path);
    if (!in) { std::cerr << "cannot open " << path << "\n"; return 2; }
    std::string line;
    int n = 0, vectors = 0;
    while (std::getline(in, line)) {
        ++n;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        auto f = fields(line);
        const std::string where = std::string(path) + ":" + std::to_string(n);
        ++vectors;
        // "parse2", "norm2", ...: the same operation reading the binding as schema 2.
        const int schema = f[0].size() > 1 && f[0].back() == '2' ? 2 : 1;
        if (schema == 2) f[0].pop_back();
        if (f[0] == "parse" && f.size() == 3) {
            Binding b; std::string why;
            bool ok = parse(f[1], b, why, schema);
            check(ok, where + " parses (" + why + ")");
            if (ok) {
                check(format(b) == f[2], where + " canonical: " + format(b));
                Binding again; check(parse(format(b), again, why, schema) && format(again) == f[2], where + " round trip");
            }
        } else if (f[0] == "reject" && f.size() == 3) {
            Binding b; std::string why;
            bool ok = parse(f[1], b, why, schema);
            check(!ok && why == f[2], where + " rejected as " + f[2] + " (got " + (ok ? "accepted" : why) + ")");
        } else if (f[0] == "fits" && f.size() == 4) {
            Binding b = must(f[2], where, schema);
            std::string why = fits(f[1], b);
            check((why.empty() ? std::string("ok") : why) == f[3], where + " fits " + f[1] + ": " + (why.empty() ? "ok" : why));
        } else if (f[0] == "norm" && f.size() == 5) {
            Binding b = must(f[2], where, schema);
            double got = normalize(f[1], b, std::strtol(f[3].c_str(), nullptr, 10));
            check(std::fabs(got - std::strtod(f[4].c_str(), nullptr)) <= 1e-6, where + " norm " + f[1] + " " + f[3] + " = " + std::to_string(got));
        } else if (f[0] == "denorm" && f.size() == 5) {
            Binding b = must(f[2], where, schema);
            long got = denormalize(f[1], b, std::strtod(f[3].c_str(), nullptr));
            check(got == std::strtol(f[4].c_str(), nullptr, 10), where + " denorm " + f[1] + " " + f[3] + " = " + std::to_string(got));
        } else if (f[0] == "rescale" && f.size() == 7) {
            long v[6];
            for (int i = 0; i < 6; ++i) v[i] = std::strtol(f[i + 1].c_str(), nullptr, 10);
            check(rescale(v[0], v[1], v[2], v[3], v[4]) == v[5], where + " rescale");
        } else if (f[0] == "dibutton" && f.size() == 3) {
            check((diButtonDown(std::strtol(f[1].c_str(), nullptr, 10)) ? 1 : 0) == std::strtol(f[2].c_str(), nullptr, 10), where + " dibutton");
        } else check(false, where + " unknown vector: " + line);
    }
    check(vectors >= 70, "at least 70 vectors read (" + std::to_string(vectors) + ")");

    // One line only: CR and LF cannot sit in the line-based vector file, so they are checked here (Astra 3473).
    {
        const std::string r12 = "dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";
        for (const char *cut : {"\r", "\n", "\r\n"}) {
            Binding b; std::string why;
            check(!parse("button 1 " + r12 + " name=\"MOZA" + cut + "steer = axis 0\"", b, why) && why == "bad-name", "CR/LF inside a name");
            check(!parse("button 1" + std::string(cut) + r12, b, why) && why == "bad-name", "CR/LF between tokens");
        }
        Binding b; std::string why;
        check(parse("button\t1\t" + r12 + "\tname=\"MOZA\tR12\"", b, why) && b.name == "MOZA\tR12", "tab stays a separator and a name character");
    }
    // [Controls] section: header values, valid, explicitly unbound, invalid and unknown entries.
    {
        const std::string r12 = "dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";
        Section s = parseSection({
            "Schema = 1", "Profile = Rig", "Revision = abc123", "Transmission = Sequential", "; comment",
            "steer = axis 0 " + r12 + " range=0..65535 rest=33265 travel=-1",
            "throttle = axis 2 " + r12 + " range=0..65535 rest=0 travel=+1",
            "confirm = button 31 " + r12, "horn =", "back = axis 1 " + r12 + " range=0..65535 rest=0 travel=+1",
            "wheelie = button 3 " + r12, "camera = button 99 " + r12 + " dev=" + "{00000000-0000-0000-0000-0000000000a1}",
            "steer = axis 1 " + r12 + " range=0..65535 rest=0 travel=+1",
        });
        check(s.error.empty() && s.schema == 1 && s.profile == "Rig" && s.revision == "abc123" && s.transmission == "Sequential", "section header");
        check(s.bound.size() == 3 && find(s, "steer") && find(s, "steer")->index == 0 && find(s, "confirm") && !find(s, "horn"), "section bound: the first steer wins");
        check(s.unbound.size() == 1 && s.unbound[0] == "horn", "section unbound");
        bool back = false, wheelie = false, camera = false, dup = false;
        for (auto &i : s.invalid) {
            back |= i.first == "back" && i.second == "wrong-kind";
            wheelie |= i.first == "wheelie" && i.second == "unknown-action";
            camera |= i.first == "camera" && i.second == "duplicate";
            dup |= i.first == "steer" && i.second == "duplicate";
        }
        check(back && wheelie && camera && dup && s.invalid.size() == 4, "section invalid entries reported with reasons");
        check(productId(*find(s, "steer")) == 0x0006346Eu, "product id (PID << 16 | VID)");
        Section v2 = parseSection({"Schema = 2", "steer = axis 0 " + r12 + " range=0..65535 rest=0 travel=+1"});
        check(!v2.error.empty() && v2.bound.empty(), "schema 2: nothing applies");
        Section none = parseSection({"steer = axis 0 " + r12 + " range=0..65535 rest=0 travel=+1"});
        check(none.error == "no Schema" && none.bound.empty(), "no schema: nothing applies");
    }
    // Test injection commands (dbce_inject.hpp).
    {
        const std::string r12 = "dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";
        Section s = parseSection({"Schema = 1", "steer = axis 0 " + r12 + " range=0..65535 rest=33265 travel=-1",
                                  "throttle = axis 2 " + r12 + " range=0..65535 rest=0 travel=+1", "confirm = button 31 " + r12, "navLeft = hat 0 27000 " + r12});
        InjectCommand c; std::string why;
        check(parseInject("inject action steer 0.5 ms=1000", &s, c, why) && c.action == "steer" && c.binding.index == 0 && c.raw == 49151 && c.ms == 1000, "inject action steer 0.5");
        check(parseInject("inject action throttle 1 ms=50", &s, c, why) && c.raw == 65535, "inject action throttle 1");
        check(parseInject("inject action confirm 1 ms=200", &s, c, why) && c.binding.kind == Kind::Button && c.raw == 1, "inject action confirm");
        check(parseInject("inject action navLeft 1 ms=200", &s, c, why) && c.raw == 27000, "inject action navLeft");
        check(!parseInject("inject action brake 1 ms=200", &s, c, why) && why == "brake is not bound", "unbound action refused");
        check(!parseInject("inject action steer 0.5 ms=20", &s, c, why), "ms below 50 refused");
        check(!parseInject("inject action steer x ms=200", &s, c, why), "bad value refused");
        check(!parseInject("inject action steer 0.5 ms=200", nullptr, c, why), "no applied controls refused");
        check(parseInject("inject raw axis 2 dev={00000000-0000-0000-0000-0000000000A1} value=40000 ms=1500", nullptr, c, why) && c.binding.dev == "{00000000-0000-0000-0000-0000000000a1}" && c.raw == 40000 && c.binding.max == 65535, "inject raw axis");
        check(parseInject("inject raw axis 0 dev={00000000-0000-0000-0000-0000000000a1} value=-100 ms=100 range=-32768..32767", nullptr, c, why) && c.binding.min == -32768, "inject raw axis with a range");
        check(parseInject("inject raw hat 0 9000 dev={00000000-0000-0000-0000-0000000000a1} value=9000 ms=100", nullptr, c, why), "inject raw hat");
        check(!parseInject("inject raw button 3 value=128 ms=100", nullptr, c, why) && why.find("dev=") == 0, "raw needs the instance");
        check(!parseInject("inject raw button 3 dev={00000000-0000-0000-0000-0000000000a1} value=128 ms=100", nullptr, c, why), "a button takes 0 or 1");
        check(!parseInject("inject raw axis 1 dev={00000000-0000-0000-0000-0000000000a1} value=70000 ms=100", nullptr, c, why), "raw value outside the range");
        check(!parseInject("press steer", &s, c, why), "not an inject command");
    }
    // Schema 2 sections: a reader opts in; by default (and in every schema-1 section) `cal=` is not read.
    {
        const std::string r12 = "dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";
        const std::string steer = "steer = axis 0 " + r12 + " range=0..65535 rest=33265 travel=+1 cal=1000..33265..64000";
        Section old = parseSection({"Schema = 2", steer});
        check(old.error == "Schema 2 is not supported (this mod reads 1)" && old.bound.empty(), "schema 2 refused by a schema-1 reader");
        Section v2 = parseSection({"Schema = 2", steer}, 2);
        const Binding *b = find(v2, "steer");
        check(v2.error.empty() && b && b->cal.size() == 3 && std::fabs(normalize("steer", *b, 17133) + 16132.0 / 32265.0) < 1e-12,
              "schema 2 read by an opted-in reader: cal applies");
        Section v1 = parseSection({steer, "Schema = 1"}, 2);
        b = find(v1, "steer");
        check(v1.error.empty() && b && b->cal.empty() && std::fabs(normalize("steer", *b, 49151) - (49151 - 32767.5) / 32767.5) < 1e-12,
              "a schema-1 section keeps its law in a schema-2 reader: cal ignored, the midpoint");
        check(parseSection({"Schema = 3", steer}, 2).error == "Schema 3 is not supported (this mod reads 1 or 2)", "schema 3 refused");
        Section bad = parseSection({"Schema = 2", "steer = axis 0 " + r12 + " range=0..65535 rest=33265 travel=+1 cal=0..300..65535"}, 2);
        check(bad.bound.empty() && bad.invalid.size() == 1 && bad.invalid[0].second == "bad-calibration", "an invalid calibration is reported, not applied");
    }
    // The inverse inside the calibrated domain is exact for every raw value; beyond it the value saturates at the lock.
    {
        const std::string r12 = "dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";
        Binding s, p, q; std::string why;
        check(parse("axis 0 " + r12 + " range=0..65535 rest=33265 travel=+1 cal=1000..33265..64000", s, why, 2), "round-trip steer parses");
        check(parse("axis 2 " + r12 + " range=0..65535 rest=1200 travel=+1 cal=1200..60000 inverted", p, why, 2), "round-trip pedal parses");
        check(parse("axis 5 " + r12 + " range=-32768..32767 rest=30000 travel=-1 cal=30000..-31000", q, why, 2), "round-trip reversed pedal parses");
        bool exact = true;
        for (long v = 1000; v <= 64000; ++v) exact = exact && denormalize("steer", s, normalize("steer", s, v)) == v;
        for (long v = 1200; v <= 60000; ++v) exact = exact && denormalize("throttle", p, normalize("throttle", p, v)) == v;
        for (long v = -31000; v <= 30000; ++v) exact = exact && denormalize("brake", q, normalize("brake", q, v)) == v;
        check(exact, "denormalize(normalize(v)) == v for every v inside the measured endpoints");
        bool saturated = true;
        for (long v : {0L, 999L}) saturated = saturated && normalize("steer", s, v) == -1.0 && denormalize("steer", s, -1.0) == 1000;
        for (long v : {64001L, 65535L}) saturated = saturated && normalize("steer", s, v) == 1.0 && denormalize("steer", s, 1.0) == 64000;
        for (long v : {60001L, 65535L}) saturated = saturated && normalize("throttle", p, v) == 0.0;
        check(saturated, "beyond the measured endpoints: saturation, mapping back to the endpoint (not the raw value)");
    }
    // Parity with the PC mod's calibration (fixtures/pcmod_axis_calibration.hpp, dead zone 0), on 0..65535.
    {
        const std::string r12 = "dev={00000000-0000-0000-0000-0000000000a1} prod={0006346e-0000-0000-0000-504944564944}";
        struct Case { long lo, centre, hi; };
        bool same = true, validSame = true;
        for (const Case &k : {Case{1000, 33265, 64000}, Case{0, 32767, 65535}, Case{5000, 30000, 40000}, Case{0, 512, 65535}, Case{0, 511, 65535}}) {
            Binding b; std::string why;
            parse("axis 0 " + r12 + " range=0..65535 rest=" + std::to_string(k.centre) + " travel=+1 cal=" + std::to_string(k.lo) + ".." +
                  std::to_string(k.centre) + ".." + std::to_string(k.hi), b, why, 2);
            const WheelInput::Calibration c{true, float(k.lo), float(k.centre), float(k.hi)};
            validSame = validSame && (fits("steer", b).empty() == WheelInput::Valid(c, true));
            if (!fits("steer", b).empty()) continue;
            for (long v = 0; v <= 65535; v += 97)
                for (bool inv : {false, true}) {
                    Binding bi = b; bi.inverted = inv;
                    same = same && std::fabs(normalize("steer", bi, v) - WheelInput::Normalize(float(v), c, true, inv, 0.0f)) <= 1e-5;
                }
        }
        for (const Case &k : {Case{1200, 0, 60000}, Case{0, 0, 8192}, Case{0, 0, 8191}, Case{60000, 0, 1200}}) {
            Binding b; std::string why;
            parse("axis 2 " + r12 + " range=0..65535 rest=" + std::to_string(k.lo) + " travel=" + (k.hi > k.lo ? "+1" : "-1") + " cal=" +
                  std::to_string(k.lo) + ".." + std::to_string(k.hi), b, why, 2);
            // The PC mod's Valid requires min <= center <= max even for pedals; its capture stores (low + high) / 2 there.
            const float pmin = float(std::min(k.lo, k.hi)), pmax = float(std::max(k.lo, k.hi));
            const WheelInput::Calibration c{true, pmin, (pmin + pmax) / 2.0f, pmax};
            if (k.hi > k.lo) validSame = validSame && (fits("throttle", b).empty() == WheelInput::Valid(c, false));
            if (!fits("throttle", b).empty() || k.hi < k.lo) continue;   // the PC mod has no reversed pedal: its min is released
            for (long v = 0; v <= 65535; v += 97)
                for (bool inv : {false, true}) {
                    Binding bi = b; bi.inverted = inv;
                    same = same && std::fabs(normalize("throttle", bi, v) - WheelInput::Normalize(float(v), c, false, inv, 0.0f)) <= 1e-5;
                }
        }
        check(validSame, "validity agrees with the PC mod on 0..65535");
        check(same, "normalization agrees with the PC mod (inverted once) on 0..65535");
    }
    if (g_failures) { std::cerr << g_failures << " of " << g_checks << " controls checks failed\n"; return 1; }
    std::cout << "PASS " << g_checks << " controls checks (" << vectors << " shared vectors)\n";
    return 0;
}
