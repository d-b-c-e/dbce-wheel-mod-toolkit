// The toolkit triple renderer's config.ini (ue4ss\Mods\<renderer>\config.ini: DbceTriple in ADRIFT, SonicTriple in Sonic):
// "Key = value" lines before any [section], read once at game start (Triple, SideAngle, the rig measurements); the F6
// panel keeps its own [Panel] section after them. Pure text functions, tested by test_renderer_config.cpp.
#pragma once
#include <string.h>
#include <string>
#include <vector>

namespace dbce { namespace renderer {

inline std::string Trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Lines without their line ends; the line end used by the text ("\r\n" when it has none).
inline std::vector<std::string> Lines(const std::string &text, std::string *eol)
{
    if (eol) *eol = text.find("\r\n") != std::string::npos || text.find('\n') == std::string::npos ? "\r\n" : "\n";
    std::vector<std::string> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t nl = text.find('\n', at);
        std::string line = text.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
        if (nl == std::string::npos) break;
        at = nl + 1;
    }
    return out;
}

inline bool IsSection(const std::string &line) { std::string t = Trim(line); return t.size() >= 2 && t[0] == '[' && t.back() == ']'; }

// The line's key when it is "key = value" (not a comment), else "".
inline std::string KeyOf(const std::string &line)
{
    std::string t = Trim(line);
    if (t.empty() || t[0] == '#' || t[0] == ';') return std::string();
    size_t eq = t.find('=');
    return eq == std::string::npos ? std::string() : Trim(t.substr(0, eq));
}

// The value of a key before the first [section] ("" when absent; the last one counts, as the renderer reads it).
inline std::string TopValue(const std::string &text, const char *key)
{
    std::string value;
    for (const std::string &line : Lines(text, nullptr)) {
        if (IsSection(line)) break;
        if (_stricmp(KeyOf(line).c_str(), key) == 0) value = Trim(line.substr(line.find('=') + 1));
    }
    return value;
}

// The text with one key set before the first [section]: every such line replaced in place, or one line added after
// the last key line there (before the first section when there is none). Every other line is kept, and so is the text's line end;
// the result ends with a line end.
inline std::string SetTopValue(const std::string &text, const char *key, const std::string &value)
{
    std::string eol;
    std::vector<std::string> lines = Lines(text, &eol);
    const std::string entry = std::string(key) + " = " + value;
    size_t top = lines.size(), lastKey = std::string::npos;
    bool found = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (IsSection(lines[i])) { top = i; break; }
        if (KeyOf(lines[i]).empty()) continue;
        lastKey = i;
        if (_stricmp(KeyOf(lines[i]).c_str(), key) == 0) { lines[i] = entry; found = true; }
    }
    if (!found) lines.insert(lines.begin() + (lastKey == std::string::npos ? top : lastKey + 1), entry);
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        out += eol;
    }
    return out;
}

// The STD-022 choice kept in the config: 0 Off, 1 Surround, 2 Separate monitors. Surround and Separate monitors both
// mean the renderer's Triple = Auto (it finds the connected layout at start); Screens records which one the player
// chose. Without Screens, the connected layout decides.
inline int ScreensOf(const std::string &text, bool separateLayout)
{
    std::string triple = TopValue(text, "Triple"), screens = TopValue(text, "Screens");
    if (_stricmp(triple.c_str(), "off") == 0) return 0;
    if (_stricmp(screens.c_str(), "surround") == 0) return 1;
    if (_stricmp(screens.c_str(), "separate") == 0 || _stricmp(screens.c_str(), "separate monitors") == 0) return 2;
    return separateLayout ? 2 : 1;
}
inline std::string WithScreens(const std::string &text, int choice)
{
    if (choice == 0) return SetTopValue(text, "Triple", "Off");
    std::string out = text;
    if (_stricmp(TopValue(text, "Triple").c_str(), "on") != 0) out = SetTopValue(out, "Triple", "Auto");   // On (forced) stays
    return SetTopValue(out, "Screens", choice == 2 ? "Separate monitors" : "Surround");
}

}} // namespace dbce::renderer
