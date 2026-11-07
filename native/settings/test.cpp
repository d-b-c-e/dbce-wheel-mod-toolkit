#include "dbce_ini_single_key_win.hpp"
#include <cassert>
#include <cstdio>
using namespace dbce::settings;
std::string wide(const std::string& s) {
    std::string result("\xff\xfe", 2);
    for (char c : s) { result += c; result += '\0'; }
    return result;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string original = "; retained\r\n[triple]\r\n  VIEW = angle  \r\nunknown=42\r\n[camera]\r\nforward_m=0\r\nright_m=0\r\n";
    const std::string expected = "; retained\r\n[triple]\r\n  VIEW = original  \r\nunknown=42\r\n[camera]\r\nforward_m=0\r\nright_m=0\r\n";
    std::string out;
    for (const auto& prefix : {std::string(), std::string("\xef\xbb\xbf", 3)}) {
        assert(edit(prefix + original, "triple", {{"view", "original"}}, out));
        assert(out == prefix + expected);
    }
    assert(edit(wide(original), "triple", {{"view", "original"}}, out));
    assert(out == wide(expected));
    auto unicode = wide(original + "; ") + std::string("\xe9\x00", 2);
    assert(edit(unicode, "triple", {{"view", "original"}}, out));
    assert(out == wide(expected + "; ") + std::string("\xe9\x00", 2));
    assert(edit("[triple]\nview=angle  ; my note\n", "triple", {{"view", "original"}}, out));
    assert(out == "[triple]\nview=original  ; my note\n");
    assert(edit("[triple]\n; note\n[camera]\nx=0", "triple", {{"view", "angle"}}, out));
    assert(out == "[triple]\n; note\nview=angle\n[camera]\nx=0");
    assert(edit("[triple]", "triple", {{"view", "angle"}}, out));
    assert(out == "[triple]\nview=angle\n");
    assert(edit(original, "camera", {{"forward_m", "0.020"}, {"right_m", "0.100"}, {"up_m", "0.200"}}, out));
    assert(out == original.substr(0, original.find("forward_m")) + "forward_m=0.020\r\nright_m=0.100\r\nup_m=0.200\r\n");
    for (const auto& invalid : {"[triple]\nview=angle\nVIEW=original", "[triple]\n[TRIPLE]\n", "[triple\nview=angle", "[triple]\ngarbage", "[camera]\nx=0"})
        assert(!edit(invalid, "triple", {{"view", "original"}}, out));
    assert(!edit(original, "triple", {{"view", "original"}, {"VIEW", "angle"}}, out));
    assert(!edit(original, "triple", {{"view", "angle\nmode=off"}}, out));
    assert(!edit("[triple]\rview=angle", "triple", {{"view", "original"}}, out));
    assert(edit("", "Triple", {{"Screens", "Off"}}, out, true) && out == "[Triple]\nScreens=Off\n");
    assert(edit("\xef\xbb\xbf[FFB]\r\nEnabled=0", "Triple", {{"Screens", "Surround"}}, out, true));
    assert(out == "\xef\xbb\xbf[FFB]\r\nEnabled=0\r\n[Triple]\r\nScreens=Surround\r\n");
    assert(edit(wide("[FFB]\nEnabled=0"), "Triple", {{"Screens", "Off"}}, out, true));
    assert(out == wide("[FFB]\nEnabled=0\n[Triple]\nScreens=Off\n"));
    assert(!edit("[FFB]\ngarbage", "Triple", {{"Screens", "Off"}}, out, true));
    assert(!edit("[Triple]\nScreens=Off\n[Triple]\n", "Triple", {{"Screens", "Off"}}, out, true));
    const std::string path = std::string(argv[1]) + "/settings.ini";
    auto write = [&](const std::string& s) { FILE* f = fopen(path.c_str(), "wb"); assert(f); assert(fwrite(s.data(), 1, s.size(), f) == s.size()); fclose(f); };
    auto read = [&]() { FILE* f = fopen(path.c_str(), "rb"); assert(f); std::string s; int c; while ((c = fgetc(f)) != EOF) s += char(c); fclose(f); return s; };
    for (const auto& bytes : {original, wide(original)}) {
        write(bytes);
        assert(save(path.c_str(), "triple", {{"view", "original"}}));
        assert(read() == (bytes == original ? expected : wide(expected)));
    }
    write(original);
    HANDLE lock = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    assert(lock != INVALID_HANDLE_VALUE);
    assert(!save(path.c_str(), "triple", {{"view", "original"}}));
    CloseHandle(lock);
    assert(read() == original);
    write("[triple]\nview=angle\nview=original");
    assert(!save(path.c_str(), "triple", {{"view", "original"}}));
    assert(read() == "[triple]\nview=angle\nview=original");
    DeleteFileA(path.c_str());
    assert(!save(path.c_str(), "Triple", {{"Screens", "Off"}})); // Existing caller behavior is unchanged.
    assert(save(path.c_str(), "Triple", {{"Screens", "Off"}}, true));
    assert(read() == "[Triple]\nScreens=Off\n");
    assert(save(path.c_str(), "Telemetry", {{"Enabled", "0"}}, true));
    assert(read() == "[Triple]\nScreens=Off\n[Telemetry]\nEnabled=0\n");
    write("");
    assert(save(path.c_str(), "Triple", {{"Screens", "Off"}}, true));
    assert(read() == "[Triple]\nScreens=Off\n");
    DeleteFileA(path.c_str());

    // The F6 panel's [Panel] and the FFB layer's [FFB] saves used WritePrivateProfileStringA before saveKey. For the
    // layouts mods write, saveKey must give the same bytes; Windows' own writer is checked against each golden too.
    struct Golden { const char *before, *section, *key, *value, *after; };
    const Golden goldens[] = {
        {"[FFB]\r\nEnabled=1\r\nStrength=50\r\n", "FFB", "Strength", "60", "[FFB]\r\nEnabled=1\r\nStrength=60\r\n"},
        {"[FFB]\r\nEnabled=1\r\n", "FFB", "Strength", "60", "[FFB]\r\nEnabled=1\r\nStrength=60\r\n"},
        {"[FFB]\r\nEnabled=1", "FFB", "Strength", "60", "[FFB]\r\nEnabled=1\r\nStrength=60\r\n"},
        {"[FFB]\r\nEnabled=1\r\n\r\n", "FFB", "Strength", "60", "[FFB]\r\nEnabled=1\r\n\r\nStrength=60\r\n"},
        {"; mod settings\r\n[FFB]\r\n; force\r\nEnabled=1\r\n", "FFB", "Enabled", "0", "; mod settings\r\n[FFB]\r\n; force\r\nEnabled=0\r\n"},
        {"[ffb]\r\nenabled=1\r\n", "FFB", "Enabled", "0", "[ffb]\r\nenabled=0\r\n"},
        {"[FFB]\nEnabled=1\n", "FFB", "Enabled", "0", "[FFB]\nEnabled=0\n"},
        {"[Panel]\r\nPage=FFB\r\n[FFB]\r\nEnabled=1\r\n", "Panel", "View", "Advanced", "[Panel]\r\nPage=FFB\r\nView=Advanced\r\n[FFB]\r\nEnabled=1\r\n"},
        {"[FFB]\r\nEnabled=1\r\n", "Panel", "Page", "FFB", "[FFB]\r\nEnabled=1\r\n[Panel]\r\nPage=FFB\r\n"},
        {"[FFB]\r\nEnabled=1\r\n\r\n", "Panel", "Page", "FFB", "[FFB]\r\nEnabled=1\r\n\r\n[Panel]\r\nPage=FFB\r\n"},
        {"[FFB]\r\nEnabled=1\r\n", "FFB", "Enabled", "1", "[FFB]\r\nEnabled=1\r\n"},
    };
    for (const auto& g : goldens) {
        write(g.before);
        assert(saveKey(path.c_str(), g.section, g.key, g.value));
        assert(read() == g.after);
        write(g.before);
        assert(WritePrivateProfileStringA(g.section, g.key, g.value, path.c_str()));
        assert(read() == g.after);
    }
    // Intended STD-044 differences: spacing, BOM and malformed files are kept or refused instead of rewritten.
    write("[FFB]\r\nStrength = 50\r\n");
    assert(saveKey(path.c_str(), "FFB", "Strength", "60") && read() == "[FFB]\r\nStrength = 60\r\n");
    write("\xef\xbb\xbf; mod\r\n[FFB]\r\nEnabled=1\r\n");
    assert(saveKey(path.c_str(), "FFB", "Enabled", "0") && read() == "\xef\xbb\xbf; mod\r\n[FFB]\r\nEnabled=0\r\n");
    assert(GetPrivateProfileIntA("FFB", "Enabled", 1, path.c_str()) == 0);
    write("[FFB]\r\ngarbage\r\n");
    assert(!saveKey(path.c_str(), "FFB", "Enabled", "0") && read() == "[FFB]\r\ngarbage\r\n");
    // First F6 save creates the INI (LF, where Windows wrote CRLF); the game's reader sees the same value.
    DeleteFileA(path.c_str());
    assert(saveKey(path.c_str(), "Panel", "Page", "FFB") && read() == "[Panel]\nPage=FFB\n");
    char page[16] = "";
    GetPrivateProfileStringA("Panel", "Page", "", page, sizeof page, path.c_str());
    assert(std::string(page) == "FFB");
    assert(saveKeys(path.c_str(), "Panel", {{"View", "Advanced"}, {"Scale", "125"}}));
    assert(read() == "[Panel]\nPage=FFB\nView=Advanced\nScale=125\n");
    assert(!saveKey(path.c_str(), "Panel", nullptr, "x") && !saveKeys(path.c_str(), "Panel", {}));
    DeleteFileA(path.c_str());
    const std::string missing = std::string(argv[1]) + "/no-such-folder/settings.ini";
    assert(!saveKey(missing.c_str(), "Panel", "Page", "FFB"));
    assert(GetFileAttributesA((std::string(argv[1]) + "/no-such-folder").c_str()) == INVALID_FILE_ATTRIBUTES);
    puts("PASS: INI byte preservation, batched camera saves, malformed/locked refusal and atomic disk replacement");
    puts("PASS: saveKey matches WritePrivateProfileStringA for plain mod INIs; creates, batches and refuses as STD-044 says");
}
