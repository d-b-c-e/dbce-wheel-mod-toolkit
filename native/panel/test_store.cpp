// The shared store (docs/STORE.md): the C++ reader's dump of each fixture equals its .dump.txt golden, the file the
// PowerShell 5.1 and C# readers are checked against, and each X.future.json (fields a later version adds) dumps exactly
// like X.json. Usage: test_store.exe <schema\fixtures\store folder>
#include "dbce_store.h"
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace dbce::panel;

static std::vector<std::string> Lines(const std::string &text)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') { out.push_back(cur); cur.clear(); } else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static bool ReadAll(const std::wstring &path, std::string &out)
{
    FILE *f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") || !f) return false;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

static int failures = 0, lines = 0;
static void Check(bool ok, const char *what) { if (!ok) { printf("FAIL %s\n", what); ++failures; } }

static void Same(const std::wstring &file, const std::string &dump, const std::wstring &golden)
{
    std::string want;
    if (!ReadAll(golden, want)) { printf("FAIL %ls: no golden\n", golden.c_str()); ++failures; return; }
    const std::vector<std::string> got = Lines(dump), exp = Lines(want);
    for (size_t i = 0; i < got.size() && i < exp.size(); ++i, ++lines)
        if (got[i] != exp[i]) { printf("FAIL %ls line %zu: got '%s', want '%s'\n", file.c_str(), i + 1, got[i].c_str(), exp[i].c_str()); ++failures; return; }
    if (got.size() != exp.size()) { printf("FAIL %ls: %zu lines, want %zu\n", file.c_str(), got.size(), exp.size()); ++failures; }
}

int wmain(int argc, wchar_t **argv)
{
    if (argc < 2) { printf("FAIL usage: test_store <fixtures\\store folder>\n"); return 2; }
    const std::wstring dir = std::wstring(argv[1]) + L"\\";
    for (const wchar_t *name : {L"owner.profiles", L"widescreen.profiles"}) {
        for (const wchar_t *suffix : {L".json", L".future.json"}) {
            const std::wstring file = dir + name + suffix;
            if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
            Store st;
            std::string error;
            if (!LoadStore(file.c_str(), st, error)) { printf("FAIL %ls: %s\n", file.c_str(), error.c_str()); ++failures; continue; }
            Same(file, DumpStore(st), dir + name + L".dump.txt");
        }
    }
    for (const wchar_t *suffix : {L".json", L".future.json"}) {
        const std::wstring file = dir + L"art-of-rally.runtime" + suffix;
        RuntimeSettings r;
        std::string error;
        if (!LoadRuntimeSettings(file.c_str(), r, error)) { printf("FAIL %ls: %s\n", file.c_str(), error.c_str()); ++failures; continue; }
        Same(file, DumpRuntimeSettings(r), dir + L"art-of-rally.runtime.dump.txt");
        Check(r.get("ffb.strength") && r.get("ffb.strength")->number == 55 && !r.get("future.curve"), "runtime values; unknown value types skipped");
    }

    Store st;
    std::string error;
    Check(LoadStore((dir + L"owner.profiles.json").c_str(), st, error), "the owner's profiles load");
    StoreChoice art = StoreFor(st, "art-of-rally");
    Check(art.profile && art.profile->name == "Moza KS Pro Triple Auto" && art.screens == &st.cockpit && art.screens->count == 3, "art uses Auto with the cockpit");
    Check(StoreFor(st, "dirt-4").profile->transmission == "H-pattern", "DiRT 4 uses Manual");
    const char other[] = "{\"Schema\":3,\"Profiles\":[{\"Id\":\"a\",\"Profile\":{}}]}";
    Check(!ParseStore(other, sizeof other - 1, st, error), "refuses another profiles Schema");
    const char none[] = "{\"Schema\":2,\"Profiles\":[]}";
    Check(!ParseStore(none, sizeof none - 1, st, error), "refuses no profiles");
    const char twice[] = "{\"Schema\":2,\"Profiles\":[{\"Id\":\"a\",\"Profile\":{}},{\"Id\":\"a\",\"Profile\":{}}]}";
    Check(!ParseStore(twice, sizeof twice - 1, st, error), "refuses duplicate profile ids");
    const std::wstring rt = StoreRuntimePath(L"art-of-rally");
    const std::wstring tail = L"\\Wheelkit\\store\\runtime\\art-of-rally.json";
    Check(rt.size() > tail.size() && rt.compare(rt.size() - tail.size(), tail.size(), tail) == 0, "runtime path");
    // The shim's [Controls] text from the store: byte for byte what shared/mod/DbceStoreControls.cs gives.
    const std::wstring controls = std::wstring(argv[1]) + L"\\..\\controls\\";
    Store drive;
    if (!LoadStore((controls + L"drive-rally.profiles.json").c_str(), drive, error)) { printf("FAIL drive store: %s\n", error.c_str()); ++failures; }
    else {
        const std::vector<std::string> delivers = {"steer", "throttle", "brake", "handbrake", "shiftUp", "shiftDown", "reverse", "start",
            "camera", "reset", "gear1", "gear2", "gear3", "gear4", "gear5", "gear6", "gear7", "gear8"};
        StoreProfile p = *StoreFor(drive, "drive-rally").profile;
        std::string why, want, text = StoreControlsText(p, delivers, why);
        Check(ReadAll(controls + L"drive-rally.store-controls.txt", want) && text == want && why.empty(), "DRIVE controls equal drive-rally.store-controls.txt byte for byte");
        const auto npos = std::string::npos;
        p.transmission = "H-pattern";
        const std::string hp = StoreControlsText(p, delivers, why);
        Check(hp.find("reverse = button 7 ") != npos && hp.find("gear7 = ") != npos && hp.find("shiftUp") == npos, "H-pattern writes the gates, not the shifts");
        p.transmission = "Automatic";
        const std::string au = StoreControlsText(p, delivers, why);
        Check(au.find("shift") == npos && au.find("gear") == npos && au.find("reverse") == npos && au.find("Transmission = Automatic") != npos, "Automatic writes no gears");
        p.transmission = "Sequential";
        for (StoreBinding &b : p.bindings) if (b.action == "shiftDown") b.index = 9;
        const std::string clash = StoreControlsText(p, delivers, why);
        Check(clash.find("shift") == npos && why.find("gears") != npos, "two gears on one control write neither");
        Check(StoreControlsText(*StoreFor(drive, "unassigned").profile, delivers, why).empty() && why.find("no steering") != npos, "no steering, no controls");
        Check(StoreControlsText(p, {"throttle"}, why).empty(), "a build that does not take steering gets no controls");
    }
    if (failures) return 1;
    printf("PASS store C++ reader: %d dump lines match across five fixtures; store controls match the C# twin\n", lines);
    return 0;
}
