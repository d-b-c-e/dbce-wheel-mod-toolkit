// Checks for camera_sets.hpp: starting choices, saved keys, toggle and zoom writes, lens target, labels.
#include "camera_sets.hpp"
#include <map>
#include <string>
using namespace dbce_camsets;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
static Settings read(const std::map<std::string, std::string> &ini)
{
    return readSettings([&](const char *k, char *b, size_t n) {
        auto it = ini.find(k);
        if (it == ini.end()) return false;
        strcpy_s(b, n, it->second.c_str());
        return true;
    });
}
int main()
{
    Settings s = read({});
    CHECK(!s.realistic[kChase] && !s.realistic[kBonnet] && s.realistic[kBumper] && s.realistic[kDash] && s.realistic[kCockpit]);
    s = read({{"MatchGameFov", "true"}});
    CHECK(!s.realistic[kBumper] && !s.realistic[kCockpit] && !s.realistic[kChase]);
    s = read({{"MatchGameFov", "false"}, {"BonnetRealistic", "off"}, {"CockpitGameOffsetDegrees", "-4 ; note"}, {"BumperRealisticOffsetDegrees", "x"}});
    CHECK(s.realistic[kCockpit] && !s.realistic[kBonnet] && s.offset[kCockpit][0] == -4 && s.offset[kBumper][1] == 0);
    Write w = toggle(s, kCockpit);
    CHECK(!strcmp(w.key, "CockpitRealistic") && !strcmp(w.value, "false") && !s.realistic[kCockpit]);
    w = zoom(s, kCockpit, kZoomIn);
    CHECK(!strcmp(w.key, "CockpitGameOffsetDegrees") && s.offset[kCockpit][0] == -6);
    w = zoom(s, kCockpit, kZoomReset);
    CHECK(!strcmp(w.value, "0") && s.offset[kCockpit][0] == 0);
    const float game[2] = {0.9f, 0.4f}, seat[2] = {0.6f, 0.2f};
    float out[2];
    target(s, kNone, game, seat, out);   CHECK(out[0] == game[0] && out[1] == game[1]);
    target(s, kBumper, game, seat, out); CHECK(out[0] == seat[0] && out[1] == seat[1]);
    s.offset[kBonnet][0] = 10;
    target(s, kBonnet, game, seat, out);
    const double deg = 2 * atan((double)out[1]) * 57.29577951308232, base = 2 * atan(0.4) * 57.29577951308232;
    CHECK(fabs(deg - base - 10) < 1e-3 && fabs(out[0] / out[1] - 0.9 / 0.4) < 1e-4);
    char b[256];
    label(b, sizeof b, s, kNone, "DiRT 4");
    CHECK(!strcmp(b, "This camera keeps DiRT 4's own FOV (not identified yet)"));
    name(b, sizeof b, s, kBonnet);
    CHECK(!strcmp(b, "Bonnet Camera - Game FOV (Custom)"));
    printf(fails ? "FAIL %d\n" : "PASS camera_sets\n", fails);
    return fails ? 1 : 0;
}
