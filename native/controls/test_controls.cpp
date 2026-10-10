// Runs controls-vectors.txt against dbce_controls.hpp, plus the [Controls] section reader. No game, no device.
// test_controls.exe [vectors file]
#include "dbce_controls.hpp"
#include "dbce_inject.hpp"
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
static Binding must(const std::string &text, const std::string &where)
{
    Binding b;
    std::string why;
    check(parse(text, b, why), where + ": vector binding parses (" + why + ")");
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
        if (f[0] == "parse" && f.size() == 3) {
            Binding b; std::string why;
            bool ok = parse(f[1], b, why);
            check(ok, where + " parses (" + why + ")");
            if (ok) {
                check(format(b) == f[2], where + " canonical: " + format(b));
                Binding again; check(parse(format(b), again, why) && format(again) == f[2], where + " round trip");
            }
        } else if (f[0] == "reject" && f.size() == 3) {
            Binding b; std::string why;
            bool ok = parse(f[1], b, why);
            check(!ok && why == f[2], where + " rejected as " + f[2] + " (got " + (ok ? "accepted" : why) + ")");
        } else if (f[0] == "fits" && f.size() == 4) {
            Binding b = must(f[2], where);
            std::string why = fits(f[1], b);
            check((why.empty() ? std::string("ok") : why) == f[3], where + " fits " + f[1] + ": " + (why.empty() ? "ok" : why));
        } else if (f[0] == "norm" && f.size() == 5) {
            Binding b = must(f[2], where);
            double got = normalize(f[1], b, std::strtol(f[3].c_str(), nullptr, 10));
            check(std::fabs(got - std::strtod(f[4].c_str(), nullptr)) <= 1e-6, where + " norm " + f[1] + " " + f[3] + " = " + std::to_string(got));
        } else if (f[0] == "denorm" && f.size() == 5) {
            Binding b = must(f[2], where);
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
    if (g_failures) { std::cerr << g_failures << " of " << g_checks << " controls checks failed\n"; return 1; }
    std::cout << "PASS " << g_checks << " controls checks (" << vectors << " shared vectors)\n";
    return 0;
}
