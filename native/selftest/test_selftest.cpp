// Offline check of dbce_selftest.hpp with a fake host: every schema/fixtures/selftest/<x>.script.json runs on a fake
// clock (16 ms a frame) and must write exactly <x>.result.json and make exactly the host calls in <x>.calls.txt; every
// file result.json names must exist. Then the arming rules: the developer opt-in, a script older than the process,
// one run per write, and a mod that cannot mute force feedback.
// Usage: test_selftest.exe <fixtures folder> <work folder> [update]   ("update" rewrites the goldens)
#include "dbce_selftest.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace dbce::selftest;

static int g_failures;
static void check(bool ok, const std::string &what)
{
    if (!ok) { ++g_failures; printf("FAIL: %s\n", what.c_str()); }
}
static std::string readText(const fs::path &p)
{
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str(), out;
    for (char c : s) if (c != '\r') out += c;   // goldens may be checked out with CRLF
    return out;
}
static void writeText(const fs::path &p, const std::string &s)
{
    std::ofstream f(p, std::ios::binary);
    f << s;
}

class FakeHost : public Host {
public:
    std::string calls;
    bool canMute = true;
    const char *Where() override
    {
        if (!started_) return "menu";
        return loading_ && loading_-- ? "loading" : "race";
    }
    Status MuteFfb() override
    {
        call("mute-ffb");
        if (!canMute) return Status::unsupported();
        muted_ = true;
        return Status::ok();
    }
    Status OpenPage(const char *page, const char *view) override
    {
        call(std::string("open-page ") + page + (*view ? std::string(" ") + view : std::string()));
        static const char *const pages[] = {"Controls", "FFB", "Display", "Cameras", "Telemetry", "Help"};
        if (!*page) return Status::ok();
        for (const char *p : pages) if (!_stricmp(p, page)) { page_ = p; return Status::ok(); }
        return Status::fail(std::string("no page '") + page + "' in this mod");
    }
    Status ClosePanel() override { call("close-panel"); page_ = "closed"; return Status::ok(); }
    Status StartRace(const char *what) override
    {
        call(std::string("start ") + what);
        if (strcmp(what, "race")) return Status::unsupported();
        started_ = true;
        loading_ = 3;
        return Status::ok();
    }
    Status CycleCamera(bool next) override
    {
        static const char *const names[] = {"Bonnet", "Cockpit", "Chase"};
        camera_ = (camera_ + (next ? 1 : 2)) % 3;
        call(std::string("camera ") + (next ? "next" : "previous") + " -> " + names[camera_]);
        return Status::ok();
    }
    Status PressModKey(const char *key) override
    {
        call(std::string("key ") + key);
        return strcmp(key, "F7") ? Status::fail(std::string("no mod key '") + key + "'") : Status::ok();
    }
    Status SetSession(const char *key, const Value &v) override
    {
        call(std::string("set ") + key + (v.type == Value::Bool ? (v.b ? "=true" : "=false") : v.type == Value::String ? "=" + v.s : "=<number>"));
        if (strcmp(key, "Triple.Enabled") || v.type != Value::Bool) return Status::fail(std::string("no session setting '") + key + "'");
        triple_ = v.b;
        return Status::ok();
    }
    Status Capture(const wchar_t *path) override
    {
        const std::string name = fs::path(path).filename().string();
        if (pending_ != name) { pending_ = name; call("capture requested " + name); return Status::busy(); }   // drawn next frame
        pending_.clear();
        writeText(path, std::string("\x89PNG\r\n\x1a\n", 8));
        call("capture written " + name);
        return Status::ok();
    }
    Status Dump(Probes &out) override
    {
        static const char *const names[] = {"Bonnet", "Cockpit", "Chase"};
        out.push_back({"mod.version", Value::text("0.0.0-test")});
        out.push_back({"camera.name", Value::text(names[camera_])});
        out.push_back({"camera.index", Value::number(camera_)});
        out.push_back({"screens.triple", Value::boolean(triple_)});
        out.push_back({"panel.page", Value::text(page_)});
        out.push_back({"ffb.state", Value::text(muted_ ? "muted" : "on")});
        call("dump");
        return Status::ok();
    }
    Status Quit() override { call("quit"); return Status::ok(); }
    std::string LogTail() override { return "fake host: log tail\n"; }
private:
    bool started_ = false, muted_ = false, triple_ = true;
    int loading_ = 0, camera_ = 0;
    std::string page_ = "closed", pending_;
    void call(const std::string &line) { calls += line + "\n"; }
};

static std::vector<std::string> g_log;
static void logLine(const char *line) { g_log.push_back(line); }

static FILETIME minutesFromNow(int minutes)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    u.QuadPart += (ULONGLONG)((LONGLONG)minutes * 60LL * 10000000LL);
    ft.dwLowDateTime = u.LowPart;
    ft.dwHighDateTime = u.HighPart;
    return ft;
}

// Ticks until <run>\result.json appears (or the frame budget ends); returns its text, "" when none.
static std::string run(Runner &runner, uint64_t &clock, const fs::path &result, int frames = 20000)
{
    for (int i = 0; i < frames; ++i) {
        runner.Tick(clock);
        clock += 16;
        if (fs::exists(result) && !runner.Running()) return readText(result);
    }
    return {};
}

static fs::path freshFolder(const fs::path &work, const std::string &name)
{
    const fs::path dir = work / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

static void fixtures(const fs::path &fixtureDir, const fs::path &work, bool update)
{
    int count = 0;
    for (const auto &e : fs::directory_iterator(fixtureDir)) {
        const std::string file = e.path().filename().string();
        const std::string suffix = ".script.json";
        if (file.size() <= suffix.size() || file.compare(file.size() - suffix.size(), suffix.size(), suffix)) continue;
        const std::string name = file.substr(0, file.size() - suffix.size());
        ++count;
        const fs::path dir = freshFolder(work, name);
        writeText(dir / "script.json", readText(e.path()));   // a fresh write: a copy keeps the old write time
        FakeHost host;
        Options o;
        o.folder = dir.wstring();
        o.log = logLine;
        o.startedAt = minutesFromNow(-1);
        Runner runner(host, o);
        uint64_t clock = 1000;
        const fs::path result = dir / name / "result.json";
        const std::string got = run(runner, clock, result);
        check(!got.empty(), name + ": result.json written");
        check(!fs::exists(dir / name / "result.json.tmp"), name + ": no temporary file left");
        const fs::path goldenResult = fixtureDir / (name + ".result.json"), goldenCalls = fixtureDir / (name + ".calls.txt");
        if (update) {
            writeText(goldenResult, got);
            writeText(goldenCalls, host.calls);
        }
        check(got == readText(goldenResult), name + ": result.json matches " + goldenResult.filename().string() + "\n" + got);
        check(host.calls == readText(goldenCalls), name + ": host calls match " + goldenCalls.filename().string() + "\n" + host.calls);
        for (size_t at = 0; (at = got.find("\"file\": \"", at)) != std::string::npos;) {
            at += 9;
            const std::string f = got.substr(at, got.find('"', at) - at);
            check(fs::exists(dir / name / f), name + ": " + f + " exists");
        }
        if (got.find("\"log\": \"log.txt\"") != std::string::npos) check(readText(dir / name / "log.txt") == "fake host: log tail\n", name + ": log.txt");
        printf("%s: %s\n", name.c_str(), got.find("\"pass\": true") != std::string::npos ? "pass" : "fail (expected by the fixture)");
    }
    check(count >= 5, "five or more fixture scripts");
}

static const char kScript[] = "{\"schema\":\"dbce.selftest@1\",\"name\":\"arming\",\"steps\":[{\"wait\":\"frames:2\"},{\"quit\":true}]}";

static void arming(const fs::path &work)
{
    // No opt-in: the script is ignored.
    SetEnvironmentVariableA("DBCE_DEV", nullptr);
    {
        const fs::path dir = freshFolder(work, "arming-off");
        writeText(dir / "script.json", kScript);
        FakeHost host;
        Options o;
        o.folder = dir.wstring();
        o.startedAt = minutesFromNow(-1);
        Runner runner(host, o);
        uint64_t clock = 1000;
        check(run(runner, clock, dir / "arming" / "result.json", 400).empty() && host.calls.empty(), "no opt-in: nothing runs");
    }
    // [Dev] SelfTest=true in the mod's config arms it.
    {
        const fs::path dir = freshFolder(work, "arming-ini");
        writeText(dir / "mod.ini", "[Dev]\r\nSelfTest=true\r\n");
        writeText(dir / "script.json", kScript);
        FakeHost host;
        Options o;
        o.folder = dir.wstring();
        o.iniPath = (dir / "mod.ini").wstring();
        o.startedAt = minutesFromNow(-1);
        Runner runner(host, o);
        uint64_t clock = 1000;
        const std::string got = run(runner, clock, dir / "arming" / "result.json");
        check(got.find("\"pass\": true") != std::string::npos && host.calls == "mute-ffb\nquit\n", "[Dev] SelfTest=true runs the script");
    }
    SetEnvironmentVariableA("DBCE_DEV", "1");
    // A script written before the process started (here: before startedAt) never runs.
    {
        const fs::path dir = freshFolder(work, "arming-stale");
        writeText(dir / "script.json", kScript);
        FakeHost host;
        Options o;
        o.folder = dir.wstring();
        o.startedAt = minutesFromNow(1);
        Runner runner(host, o);
        uint64_t clock = 1000;
        check(run(runner, clock, dir / "arming" / "result.json", 400).empty() && host.calls.empty(), "a stale script does not run");
    }
    // One run per write: the same script does not run again; writing it again does.
    {
        const fs::path dir = freshFolder(work, "arming-once");
        writeText(dir / "script.json", kScript);
        FakeHost host;
        Options o;
        o.folder = dir.wstring();
        o.startedAt = minutesFromNow(-1);
        Runner runner(host, o);
        uint64_t clock = 1000;
        const fs::path result = dir / "arming" / "result.json";
        check(!run(runner, clock, result).empty(), "DBCE_DEV=1 runs the script");
        fs::remove(result);
        check(run(runner, clock, result, 400).empty(), "the same script does not run twice");
        FILETIME later = minutesFromNow(0);
        HANDLE f = CreateFileW((dir / "script.json").c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        ULARGE_INTEGER u;
        u.LowPart = later.dwLowDateTime;
        u.HighPart = later.dwHighDateTime;
        u.QuadPart += 10000000ULL;   // one second on, so the write time surely differs
        later.dwLowDateTime = u.LowPart;
        later.dwHighDateTime = u.HighPart;
        SetFileTime(f, nullptr, nullptr, &later);
        CloseHandle(f);
        check(!run(runner, clock, result).empty(), "writing the script again runs it again");
        check(host.calls == "mute-ffb\nquit\nmute-ffb\nquit\n", "two runs, two mutes, two quits");
    }
    // A mod that cannot mute force feedback runs no step, but a last quit still closes it.
    {
        const fs::path dir = freshFolder(work, "arming-nomute");
        writeText(dir / "script.json", "{\"schema\":\"dbce.selftest@1\",\"name\":\"arming\",\"steps\":[{\"key\":\"F7\"},{\"quit\":true}]}");
        FakeHost host;
        host.canMute = false;
        Options o;
        o.folder = dir.wstring();
        o.startedAt = minutesFromNow(-1);
        Runner runner(host, o);
        uint64_t clock = 1000;
        const std::string got = run(runner, clock, dir / "arming" / "result.json");
        check(got.find("\"failure\": {\"step\": 0, \"reason\": \"could not mute force feedback: this mod does not support it\"}") != std::string::npos, "no mute: the run fails before step 1\n" + got);
        check(got.find("\"do\": \"key F7\", \"status\": \"skipped\"") != std::string::npos, "no mute: the key step is skipped");
        check(host.calls == "mute-ffb\nquit\n", "no mute: no key pressed, the game still quits\n" + host.calls);
    }
    // Not JSON at all: the result goes to <folder>\invalid.
    {
        const fs::path dir = freshFolder(work, "arming-invalid");
        writeText(dir / "script.json", "{ not json");
        FakeHost host;
        Options o;
        o.folder = dir.wstring();
        o.startedAt = minutesFromNow(-1);
        Runner runner(host, o);
        uint64_t clock = 1000;
        const std::string got = run(runner, clock, dir / "invalid" / "result.json");
        check(got.find("\"reason\": \"the script: not valid JSON (expected a key)\"") != std::string::npos && host.calls.empty(), "broken JSON is reported in invalid\\result.json\n" + got);
    }
}

// The same broken scripts tools\tests\Test-SelfTestSchema.ps1 feeds the schema: the reader refuses them too.
static void reader()
{
    const std::string head = "{\"schema\":\"dbce.selftest@1\",\"name\":\"x\",\"steps\":[";
    const char *const broken[] = {
        "{\"cam\":\"next\"}]}", "{\"wait\":\"menu\",\"key\":\"F7\"}]}", "{\"wait\":\"loading\"}]}",
        "{\"set\":\"Triple.Enabled\",\"value\":false,\"session\":false}]}", "{\"set\":\"Triple.Enabled\"}]}",
        "{\"f6\":\"close\",\"page\":\"Cameras\"}]}", "{\"camera\":\"next\",\"repeat\":2,\"each\":[{\"wait\":\"frames:2\",\"repeat\":2}]}]}",
        "{\"quit\":true,\"repeat\":2}]}", "]}", "{\"quit\":true},{\"wait\":\"menu\"}]}"};
    for (const char *b : broken) {
        const std::string text = head + b;
        detail::Script s;
        std::string error;
        check(!detail::ParseScript(text.data(), text.size(), s, error) && !error.empty(), "the reader refuses " + text);
    }
    const std::string other = "{\"schema\":\"dbce.selftest@2\",\"name\":\"x\",\"steps\":[{\"quit\":true}]}";
    detail::Script s;
    std::string error;
    check(!detail::ParseScript(other.data(), other.size(), s, error) && s.name == "x", "the reader refuses another schema but keeps the name");
    const std::string ok = head + "{\"camera\":\"next\",\"repeat\":2,\"each\":[{\"capture\":\"cam-{i}\"}]}]}";
    detail::Script r;
    check(detail::ParseScript(ok.data(), ok.size(), r, error) && r.steps.size() == 4 && r.steps[3].arg == "cam-2" && r.steps[3].text == "capture cam-2 (2/2)", "repeat with each expands in order");
}

int main(int argc, char **argv)
{
    if (argc < 3) { printf("usage: test_selftest <fixtures> <work> [update]\n"); return 2; }
    const fs::path fixtureDir = argv[1], work = argv[2];
    SetEnvironmentVariableA("DBCE_DEV", "1");
    fixtures(fixtureDir, work, argc > 3 && !strcmp(argv[3], "update"));
    arming(work);
    reader();
    if (g_failures) { printf("FAIL: %d check(s)\n", g_failures); return 1; }
    printf("PASS: self-test interpreter\n");
    return 0;
}
