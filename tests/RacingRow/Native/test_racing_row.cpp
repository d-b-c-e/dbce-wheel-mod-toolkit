// Racing-row checks for the C++ selector (shared/display/dbce_racing_row.hpp): every shared vector, exact reasons, then
// a read-only print of the live displays. No display setting, window or device is touched.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "../../../shared/display/dbce_windows_monitors.hpp"

using namespace dbce::display;

static int g_checks = 0;
static void check(bool good, const std::string& what)
{
    ++g_checks;
    if (!good)
    {
        std::printf("FAIL: %s\n", what.c_str());
        std::exit(1);
    }
}

static std::vector<std::string> split(const std::string& text, const std::string& separator)
{
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t at; (at = text.find(separator, start)) != std::string::npos; start = at + separator.size())
        parts.push_back(text.substr(start, at - start));
    parts.push_back(text.substr(start));
    return parts;
}

static int to_int(const std::string& text)
{
    size_t used = 0;
    const int value = std::stoi(text, &used);
    check(used == text.size(), "integer field: " + text);
    return value;
}

static MonitorRect monitor(const std::string& text)
{
    const auto f = split(text, ",");
    check(f.size() == 4 || (f.size() == 5 && f[4] == "primary"), "monitor field: " + text);
    return {to_int(f[0]), to_int(f[1]), to_int(f[2]), to_int(f[3]), f.size() == 5};
}

int main(int argc, char** argv)
{
    check(argc == 2, "usage: test_racing_row <racing-row-vectors.txt>");
    std::ifstream file(argv[1]);
    check(file.good(), std::string("vectors file: ") + argv[1]);
    int vectors = 0, otherAspect = 0;
    for (std::string line; std::getline(file, line);)
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto f = split(line, " | ");
        check(f.size() == 7 && f[0] == "row", "vector line: " + line);
        const std::string& name = f[1];
        const double aspect = std::stod(f[2]);
        std::vector<MonitorRect> monitors;
        if (f[3] != "-")
            for (const auto& m : split(f[3], "; ")) monitors.push_back(monitor(m));
        const auto expect = split(f[4], " ");
        check(expect.size() == 2, name + ": expected kind and rectangle");
        const auto rect = split(expect[1], ",");
        check(rect.size() == 4, name + ": expected rectangle");
        std::vector<MonitorRect> displays;
        if (f[5] != "-")
            for (const auto& index : split(f[5], " ")) displays.push_back(monitors.at(static_cast<size_t>(to_int(index))));

        const RacingRow row = select_racing_row(monitors, aspect);
        check(to_string(row.kind) == expect[0] && row.x == to_int(rect[0]) && row.y == to_int(rect[1]) && row.width == to_int(rect[2]) &&
                  row.height == to_int(rect[3]),
              name + ": expected " + f[4] + ", got " + row.to_string());
        check(row.displays == displays, name + ": selected displays");
        check(row.reason == f[6], name + ": reason\n  expected: " + f[6] + "\n  got:      " + row.reason);
        const std::string text = row.kind == RacingRowKind::None
                                     ? "no racing row: " + f[6]
                                     : expect[0] + " " + rect[2] + "x" + rect[3] + "@" + rect[0] + "," + rect[1];
        check(row.to_string() == text, name + ": to_string " + row.to_string());
        ++vectors;
        if (aspect != kSurroundAspect) ++otherAspect;
    }
    check(vectors >= 30 && otherAspect >= 4, "shared vectors read: " + std::to_string(vectors));

    // Read-only: what this machine reports now. Informational; the topology differs between machines.
    std::vector<MonitorRect> live;
    check(read_windows_monitors(live), "Windows reports the displays");
    int primaries = 0;
    std::string listed;
    for (const auto& m : live)
    {
        primaries += m.primary ? 1 : 0;
        listed += (listed.empty() ? "" : "; ") + m.to_string();
    }
    check(!live.empty() && primaries == 1, "at least one display and exactly one primary");
    for (const double aspect : {kSurroundAspect, 4.0})
    {
        const RacingRow row = select_windows_racing_row(aspect);
        std::printf("live displays: %s -> (aspect %s) %s (%s)\n", listed.c_str(), detail::aspect_text(aspect).c_str(), row.to_string().c_str(),
                    row.reason.c_str());
    }
    std::printf("PASS racing row native: %d vectors (%d with a non-default aspect), %d checks\n", vectors, otherAspect, g_checks);
    return 0;
}
