// dbce-mod.json (XC-1): the C++ reader's dump of each fixture equals schema/fixtures/<id>.dump.txt, the file the
// PowerShell 5.1 and C# readers are checked against. Usage: test_mod_file.exe <schema\fixtures folder>
#include "dbce_mod_file.h"
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

int wmain(int argc, wchar_t **argv)
{
    if (argc < 2) { printf("FAIL usage: test_mod_file <fixtures folder>\n"); return 2; }
    const std::wstring dir = argv[1];
    const wchar_t *const names[] = {L"art-of-rally", L"dirt-4", L"super-woden-rally-edge"};
    int failures = 0, lines = 0;
    for (const wchar_t *name : names) {
        ModFile m;
        std::string error, want;
        if (!LoadModFile((dir + L"\\" + name + L".dbce-mod.json").c_str(), m, error)) { printf("FAIL %ls: %s\n", name, error.c_str()); ++failures; continue; }
        if (!ReadAll(dir + L"\\" + name + L".dump.txt", want)) { printf("FAIL %ls: no dump file\n", name); ++failures; continue; }
        const std::vector<std::string> got = Lines(DumpModFile(m)), exp = Lines(want);
        for (size_t i = 0; i < got.size() && i < exp.size(); ++i, ++lines)
            if (got[i] != exp[i]) { printf("FAIL %ls line %zu: got '%s', want '%s'\n", name, i + 1, got[i].c_str(), exp[i].c_str()); ++failures; break; }
        if (got.size() != exp.size()) { printf("FAIL %ls: %zu lines, want %zu\n", name, got.size(), exp.size()); ++failures; }
    }
    ModFile m;
    std::string error;
    const char bad[] = "{\"schema\":\"dbce.mod@1\",\"id\":\"x\",\"name\":\"X\",\"version\":\"1.0.0\",\"package\":{\"kind\":\"standalone\"},"
                       "\"settings\":{\"product\":\"X\",\"pages\":{\"ffb\":[{\"id\":\"ffb.enabled\",\"kind\":\"toggle\",\"label\":\"FFB\",\"store\":{\"key\":\"K\",\"wrong\":1}}]}}}";
    if (ParseModFile(bad, sizeof bad - 1, m, error)) { printf("FAIL an unknown store field was accepted\n"); ++failures; }
    const char other[] = "{\"schema\":\"dbce.mod@2\",\"id\":\"x\",\"name\":\"X\",\"version\":\"1.0.0\",\"package\":{\"kind\":\"standalone\"}}";
    if (ParseModFile(other, sizeof other - 1, m, error)) { printf("FAIL another schema was accepted\n"); ++failures; }
    if (failures) return 1;
    printf("PASS dbce-mod.json C++ reader: %d dump lines match across three fixtures\n", lines);
    return 0;
}
