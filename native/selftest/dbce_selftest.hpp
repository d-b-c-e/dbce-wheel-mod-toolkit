// The in-mod self-test (dbce.selftest@1, docs/SELFTEST.md; 04-cross-cutting.md D.2-D.4): a scripted sequence the mod
// runs in its own process at game speed, with one result file read afterwards. Header-only, C++17, Win32 and the
// standard library only, so a native proxy, a UE4SS mod or a standalone app can include it.
//
// The mod supplies a Host (the callbacks below) and ticks a Runner once per presented frame on the thread that may
// touch the game, usually by setting the panel's Config::selfTestTick (dbce_panel.h), which PollDev calls every frame:
//
//     static MyHost host;                                   // : dbce::selftest::Host
//     static dbce::selftest::Runner runner(host, opts);     // opts.folder = %LOCALAPPDATA%\dbce\<game>\selftest
//     panelConfig.selfTestTick = [] { runner.Tick(); };
//
// Arming (never in a player default): the developer opt-in (DBCE_DEV=1 in the environment, or [Dev] SelfTest=true in
// Options::iniPath) and a <folder>\script.json last written after this process started. A script runs once per write.
// Output goes to <folder>\<name>\: frame-<x>.png (Host::Capture), state-<x>.json (Host::Dump), log.txt (Host::LogTail)
// and result.json, written last through a rename, so its existence means the run is over. Force feedback is muted for
// the run unless the script says "muteFfb": false, and stays muted afterwards; when the mute fails, no step runs (a
// last "quit" aside).
// "set" is session-only: the mod must never save a value a self-test set.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace dbce { namespace selftest {

// What a host callback did. Busy: not finished yet; the same call is made again on the next tick with the same
// arguments (a capture waiting for the next frame, a menu still loading), until the script's timeout.
struct Status {
    enum Kind { Ok, Busy, Failed, Unsupported } kind = Ok;
    std::string reason;
    static Status ok() { return {}; }
    static Status busy() { Status s; s.kind = Busy; return s; }
    static Status fail(std::string why) { Status s; s.kind = Failed; s.reason = std::move(why); return s; }
    static Status unsupported() { Status s; s.kind = Unsupported; return s; }
};

// A "set" value or a state probe: a JSON boolean, number or string.
struct Value {
    enum Type { Bool, Number, String } type = String;
    bool b = false;
    double n = 0;
    std::string s;
    static Value boolean(bool v) { Value x; x.type = Bool; x.b = v; return x; }
    static Value number(double v) { Value x; x.type = Number; x.n = v; return x; }
    static Value text(std::string v) { Value x; x.type = String; x.s = std::move(v); return x; }
};
using Probes = std::vector<std::pair<std::string, Value>>;   // probe id ("camera.name") -> value, in this order

// The mod's side. Every callback runs on the ticking thread. A callback the mod does not override answers
// Unsupported, and a step that reaches it fails with "this mod does not support <step>".
class Host {
public:
    virtual ~Host() = default;
    // Where the game is, for {"wait": "menu"} and {"wait": "race"}: "menu", "race", or anything else while loading.
    virtual const char *Where() { return ""; }
    virtual Status MuteFfb() { return Status::unsupported(); }                                  // session-only mute
    virtual Status OpenPage(const char *page, const char *view) { (void)page; (void)view; return Status::unsupported(); }  // "" = as is
    virtual Status ClosePanel() { return Status::unsupported(); }
    virtual Status StartRace(const char *what) { (void)what; return Status::unsupported(); }   // host-specific
    virtual Status CycleCamera(bool next) { (void)next; return Status::unsupported(); }
    virtual Status PressModKey(const char *key) { (void)key; return Status::unsupported(); }  // through the mod's key path
    virtual Status SetSession(const char *key, const Value &value) { (void)key; (void)value; return Status::unsupported(); }
    virtual Status Capture(const wchar_t *pngPath) { (void)pngPath; return Status::unsupported(); }  // the game's own frame
    virtual Status Dump(Probes &out) { (void)out; return Status::unsupported(); }              // written as state-<x>.json
    virtual Status Quit() { return Status::unsupported(); }                                     // the game's normal quit
    virtual std::string LogTail() { return {}; }                                                // written as log.txt
};

struct Options {
    std::wstring folder;                    // holds script.json; runs go to <folder>\<name>
    std::wstring iniPath;                   // the mod's config, for [Dev] SelfTest=true; empty: DBCE_DEV only
    void (*log)(const char *line) = nullptr;
    FILETIME startedAt{};                   // scripts written before this are ignored; zero: this process's start
};

// %LOCALAPPDATA%\dbce\<game>\selftest
inline std::wstring DefaultFolder(const wchar_t *game)
{
    wchar_t base[MAX_PATH] = L"";
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (!n || n >= MAX_PATH) return {};
    return std::wstring(base) + L"\\dbce\\" + game + L"\\selftest";
}

inline bool DevOptIn(const std::wstring &iniPath)
{
    char env[8] = "";
    const DWORD n = GetEnvironmentVariableA("DBCE_DEV", env, sizeof env);
    if (n && n < sizeof env && !strcmp(env, "1")) return true;
    if (iniPath.empty()) return false;
    wchar_t v[16] = L"";
    GetPrivateProfileStringW(L"Dev", L"SelfTest", L"", v, 16, iniPath.c_str());
    return !_wcsicmp(v, L"true") || !wcscmp(v, L"1");
}

namespace detail {

// ---- a strict JSON reader (objects keep their key order) ----
struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> fields;
    const Json *get(const char *key) const
    {
        for (const auto &f : fields) if (f.first == key) return &f.second;
        return nullptr;
    }
};

class Reader {
public:
    Reader(const char *p, size_t n) : p_(p), end_(p + n) {}
    bool parse(Json &out, std::string &error)
    {
        if (end_ - p_ >= 3 && (unsigned char)p_[0] == 0xEF && (unsigned char)p_[1] == 0xBB && (unsigned char)p_[2] == 0xBF) p_ += 3;
        if (!value(out, 0)) { error = "not valid JSON (" + err_ + ")"; return false; }
        ws();
        if (p_ != end_) { error = "not valid JSON (text after the end)"; return false; }
        return true;
    }
private:
    const char *p_, *end_;
    std::string err_;
    bool fail(const char *what) { if (err_.empty()) err_ = what; return false; }
    void ws() { while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\r' || *p_ == '\n')) ++p_; }
    bool lit(const char *t)
    {
        const size_t k = strlen(t);
        if ((size_t)(end_ - p_) < k || memcmp(p_, t, k)) return fail("unknown word");
        p_ += k;
        return true;
    }
    static void utf8(std::string &o, unsigned c)
    {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    bool hex4(unsigned &c)
    {
        if (end_ - p_ < 4) return fail("short \\u escape");
        c = 0;
        for (int i = 0; i < 4; ++i, ++p_) {
            const char h = *p_;
            c <<= 4;
            if (h >= '0' && h <= '9') c |= (unsigned)(h - '0');
            else if (h >= 'a' && h <= 'f') c |= (unsigned)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') c |= (unsigned)(h - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool str(std::string &o)
    {
        ++p_;   // opening quote
        while (p_ < end_ && *p_ != '"') {
            const char c = *p_++;
            if ((unsigned char)c < 0x20) return fail("control character in a string");
            if (c != '\\') { o += c; continue; }
            if (p_ >= end_) break;
            const char e = *p_++;
            switch (e) {
            case '"': o += '"'; break;
            case '\\': o += '\\'; break;
            case '/': o += '/'; break;
            case 'b': o += '\b'; break;
            case 'f': o += '\f'; break;
            case 'n': o += '\n'; break;
            case 'r': o += '\r'; break;
            case 't': o += '\t'; break;
            case 'u': {
                unsigned c1;
                if (!hex4(c1)) return false;
                if (c1 >= 0xD800 && c1 < 0xDC00 && end_ - p_ >= 6 && p_[0] == '\\' && p_[1] == 'u') {
                    p_ += 2;
                    unsigned c2;
                    if (!hex4(c2)) return false;
                    c1 = 0x10000 + ((c1 - 0xD800) << 10) + (c2 - 0xDC00);
                }
                utf8(o, c1);
                break;
            }
            default: return fail("bad escape");
            }
        }
        if (p_ >= end_) return fail("unterminated string");
        ++p_;
        return true;
    }
    bool value(Json &v, int depth)
    {
        if (depth > 32) return fail("nested too deeply");
        ws();
        if (p_ >= end_) return fail("unexpected end");
        const char c = *p_;
        if (c == '{') {
            v.type = Json::Object;
            ++p_; ws();
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }
            for (;;) {
                ws();
                if (p_ >= end_ || *p_ != '"') return fail("expected a key");
                std::string k;
                if (!str(k)) return false;
                for (const auto &f : v.fields) if (f.first == k) return fail("duplicate key");
                ws();
                if (p_ >= end_ || *p_ != ':') return fail("expected ':'");
                ++p_;
                Json item;
                if (!value(item, depth + 1)) return false;
                v.fields.emplace_back(std::move(k), std::move(item));
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == '}') { ++p_; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = Json::Array;
            ++p_; ws();
            if (p_ < end_ && *p_ == ']') { ++p_; return true; }
            for (;;) {
                Json item;
                if (!value(item, depth + 1)) return false;
                v.items.push_back(std::move(item));
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == ']') { ++p_; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = Json::String; return str(v.s); }
        if (c == 't') { v.type = Json::Bool; v.b = true; return lit("true"); }
        if (c == 'f') { v.type = Json::Bool; return lit("false"); }
        if (c == 'n') { v.type = Json::Null; return lit("null"); }
        if (c == '-' || (c >= '0' && c <= '9')) {
            const char *s = p_;
            if (*p_ == '-') ++p_;
            while (p_ < end_ && ((*p_ >= '0' && *p_ <= '9') || *p_ == '.' || *p_ == 'e' || *p_ == 'E' || *p_ == '+' || *p_ == '-')) ++p_;
            std::string t(s, p_);
            char *stop = nullptr;
            v.type = Json::Number;
            v.n = strtod(t.c_str(), &stop);
            if (!stop || *stop) return fail("bad number");
            return true;
        }
        return fail("unexpected character");
    }
};

// ---- JSON writing ----
inline void Quote(std::string &o, const std::string &s)
{
    o += '"';
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    o += '"';
}
inline void Put(std::string &o, const Value &v)
{
    if (v.type == Value::Bool) { o += v.b ? "true" : "false"; return; }
    if (v.type == Value::String) { Quote(o, v.s); return; }
    char b[32];
    if (v.n == (double)(long long)v.n && v.n > -1e15 && v.n < 1e15) snprintf(b, sizeof b, "%lld", (long long)v.n);
    else snprintf(b, sizeof b, "%.17g", v.n);
    o += b;
}
// ---- the script ----
enum class Kind { Wait, F6, Capture, Dump, Start, Camera, Key, Set, Quit };
inline const char *KindName(Kind k)
{
    static const char *const names[] = {"wait", "f6", "capture", "dump", "start", "camera", "key", "set", "quit"};
    return names[(int)k];
}

struct Step {
    Kind kind = Kind::Wait;
    std::string arg;        // wait: menu|race|frames|ms; f6: open|close; capture/dump: the name; start/key: the text;
                            // camera: next|previous; set: the key
    std::string page, view; // f6 open
    unsigned count = 0;     // wait frames:N / ms:N
    Value value;            // set
    std::string text;       // as reported in result.json, e.g. "camera next (2/5)"
};

struct Script {
    std::string name;
    bool muteFfb = true;
    unsigned timeoutSeconds = 120;
    std::vector<Step> steps;
};

inline bool IsName(const std::string &s, bool allowIndex)
{
    if (s.empty() || s.size() > 64) return false;
    for (char c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                        (allowIndex && (c == '{' || c == '}'));
        if (!ok) return false;
    }
    return s[0] != '.';
}
inline bool IsSlug(const std::string &s)
{
    if (s.empty() || s.size() > 64 || !(s[0] >= 'a' && s[0] <= 'z')) return false;
    for (char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}
inline bool ReadCount(const std::string &t, unsigned &out)
{
    if (t.empty() || t.size() > 7 || t[0] == '0') return false;
    for (char c : t) if (c < '0' || c > '9') return false;
    out = (unsigned)strtoul(t.c_str(), nullptr, 10);
    return true;
}
inline std::string Substitute(std::string s, unsigned i)
{
    for (size_t at; (at = s.find("{i}")) != std::string::npos;) s.replace(at, 3, std::to_string(i));
    return s;
}

// One step object (without repeat/each), or false with a reason.
inline bool ParseStep(const Json &j, Step &st, std::string &why)
{
    static const char *const kinds[] = {"wait", "f6", "capture", "dump", "start", "camera", "key", "set", "quit"};
    if (j.type != Json::Object) { why = "is not an object"; return false; }
    const Json *verb = nullptr;
    for (const auto &f : j.fields) {
        bool isVerb = false;
        for (int k = 0; k < 9; ++k)
            if (f.first == kinds[k]) {
                if (verb) { why = "has two actions ('" + std::string(KindName(st.kind)) + "' and '" + f.first + "')"; return false; }
                verb = &f.second;
                st.kind = (Kind)k;
                isVerb = true;
            }
        if (!isVerb && f.first != "page" && f.first != "view" && f.first != "value" && f.first != "session" && f.first != "repeat" && f.first != "each") {
            why = "has an unknown field '" + f.first + "'";
            return false;
        }
    }
    if (!verb) { why = "has no action (wait, f6, capture, dump, start, camera, key, set or quit)"; return false; }
    const Json *page = j.get("page"), *view = j.get("view"), *value = j.get("value"), *session = j.get("session");
    if ((page || view) && st.kind != Kind::F6) { why = "'page' and 'view' belong to an f6 step"; return false; }
    if ((value || session) && st.kind != Kind::Set) { why = "'value' and 'session' belong to a set step"; return false; }
    const std::string a = verb->type == Json::String ? verb->s : std::string();
    if (st.kind != Kind::Quit && verb->type != Json::String) { why = std::string("'") + KindName(st.kind) + "' needs a text value"; return false; }
    st.arg = a;
    switch (st.kind) {
    case Kind::Wait:
        if (a == "menu" || a == "race") { st.text = "wait " + a; return true; }
        if (!a.compare(0, 7, "frames:") && ReadCount(a.substr(7), st.count)) { st.arg = "frames"; st.text = "wait " + a; return true; }
        if (!a.compare(0, 3, "ms:") && ReadCount(a.substr(3), st.count)) { st.arg = "ms"; st.text = "wait " + a; return true; }
        why = "waits for '" + a + "' (menu, race, frames:N or ms:N)";
        return false;
    case Kind::F6:
        if (a == "close") {
            if (page || view) { why = "closes the panel but names a page or view"; return false; }
            st.text = "f6 close";
            return true;
        }
        if (a != "open") { why = "f6 is open or close, not '" + a + "'"; return false; }
        if (page && (page->type != Json::String || !IsName(page->s, false))) { why = "has a bad page name"; return false; }
        if (view && (view->type != Json::String || (view->s != "simple" && view->s != "advanced" && view->s != "more"))) {
            why = "view is simple, advanced or more";
            return false;
        }
        st.page = page ? page->s : std::string();
        st.view = view ? view->s : std::string();
        st.text = "f6 open" + (st.page.empty() ? std::string() : " " + st.page) + (st.view.empty() ? std::string() : " " + st.view);
        return true;
    case Kind::Capture:
    case Kind::Dump:
        if (!IsName(a, true)) { why = std::string("names its ") + KindName(st.kind) + " '" + a + "' (letters, digits, - _ . and {i})"; return false; }
        st.text = std::string(KindName(st.kind)) + " " + a;
        return true;
    case Kind::Start:
        if (!IsSlug(a)) { why = "starts '" + a + "' (a lower-case word such as race)"; return false; }
        st.text = "start " + a;
        return true;
    case Kind::Camera:
        if (a != "next" && a != "previous") { why = "camera is next or previous, not '" + a + "'"; return false; }
        st.text = "camera " + a;
        return true;
    case Kind::Key:
        if (a.empty() || a.size() > 32) { why = "names no key"; return false; }
        st.text = "key " + a;
        return true;
    case Kind::Set:
        if (a.empty() || a.size() > 64) { why = "sets no key"; return false; }
        if (!value || (value->type != Json::Bool && value->type != Json::Number && value->type != Json::String)) {
            why = "needs a value (true, false, a number or a text)";
            return false;
        }
        if (session && !(session->type == Json::Bool && session->b)) { why = "set is session-only; 'session' can only be true"; return false; }
        st.value = value->type == Json::Bool ? Value::boolean(value->b) : value->type == Json::Number ? Value::number(value->n) : Value::text(value->s);
        st.text = "set " + a + " ";
        Put(st.text, st.value);
        return true;
    case Kind::Quit:
        if (!(verb->type == Json::Bool && verb->b)) { why = "quit is written \"quit\": true"; return false; }
        st.text = "quit";
        return true;
    }
    return false;
}

// The whole script. On failure `error` is the reason and `out.name` is the name when it could be read.
inline bool ParseScript(const char *text, size_t length, Script &out, std::string &error)
{
    Json root;
    if (!Reader(text, length).parse(root, error)) return false;
    if (root.type != Json::Object) { error = "the script is not a JSON object"; return false; }
    if (const Json *name = root.get("name"); name && name->type == Json::String && IsSlug(name->s)) out.name = name->s;
    for (const auto &f : root.fields)
        if (f.first != "schema" && f.first != "name" && f.first != "muteFfb" && f.first != "timeoutSeconds" && f.first != "steps") {
            error = "unknown field '" + f.first + "'";
            return false;
        }
    const Json *schema = root.get("schema");
    if (!schema || schema->type != Json::String || schema->s != "dbce.selftest@1") { error = "schema is not \"dbce.selftest@1\""; return false; }
    if (out.name.empty()) { error = "name is missing or not a lower-case slug"; return false; }
    if (const Json *m = root.get("muteFfb")) {
        if (m->type != Json::Bool) { error = "muteFfb is true or false"; return false; }
        out.muteFfb = m->b;
    }
    if (const Json *t = root.get("timeoutSeconds")) {
        if (t->type != Json::Number || t->n < 1 || t->n > 3600 || t->n != (double)(unsigned)t->n) { error = "timeoutSeconds is a whole number from 1 to 3600"; return false; }
        out.timeoutSeconds = (unsigned)t->n;
    }
    const Json *steps = root.get("steps");
    if (!steps || steps->type != Json::Array || steps->items.empty()) { error = "steps is missing or empty"; return false; }
    for (size_t i = 0; i < steps->items.size(); ++i) {
        const Json &j = steps->items[i];
        const std::string at = "step " + std::to_string(i + 1) + " ";
        Step st;
        std::string why;
        if (!ParseStep(j, st, why)) { error = at + why; return false; }
        const Json *repeat = j.get("repeat"), *each = j.get("each");
        unsigned times = 1;
        if (repeat && (repeat->type != Json::Number || repeat->n < 1 || repeat->n > 100 || repeat->n != (double)(unsigned)repeat->n)) {
            error = at + "repeat is a whole number from 1 to 100";
            return false;
        }
        if (repeat) times = (unsigned)repeat->n;
        std::vector<Step> inner;
        if (each) {
            if (each->type != Json::Array) { error = at + "each is a list of steps"; return false; }
            for (size_t e = 0; e < each->items.size(); ++e) {
                Step is;
                const std::string atEach = at + "each " + std::to_string(e + 1) + " ";
                if (!ParseStep(each->items[e], is, why)) { error = atEach + why; return false; }
                if (each->items[e].get("repeat") || each->items[e].get("each")) { error = atEach + "repeats inside a repeat"; return false; }
                if (is.kind == Kind::Quit) { error = atEach + "quits inside a repeat"; return false; }
                inner.push_back(std::move(is));
            }
        }
        if (st.kind == Kind::Quit && (repeat || each)) { error = at + "repeats quit"; return false; }
        for (unsigned r = 1; r <= times; ++r) {
            const std::string of = times > 1 ? " (" + std::to_string(r) + "/" + std::to_string(times) + ")" : std::string();
            Step s = st;
            if (s.kind == Kind::Capture || s.kind == Kind::Dump) { s.arg = Substitute(s.arg, r); s.text = std::string(KindName(s.kind)) + " " + s.arg; }
            s.text += of;
            out.steps.push_back(std::move(s));
            for (const Step &e : inner) {
                Step x = e;
                if (x.kind == Kind::Capture || x.kind == Kind::Dump) { x.arg = Substitute(x.arg, r); x.text = std::string(KindName(x.kind)) + " " + x.arg; }
                x.text += of;
                out.steps.push_back(std::move(x));
            }
        }
        if (st.kind == Kind::Quit && i + 1 != steps->items.size()) { error = at + "quits before the last step"; return false; }
    }
    if (out.steps.size() > 1000) { error = "more than 1000 steps after repeats"; return false; }
    return true;
}

inline std::wstring Wide(const std::string &s)
{
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
// Writes through <path>.tmp and a rename, so a reader never sees half a file.
inline bool WriteFileAtomic(const std::wstring &path, const std::string &bytes)
{
    const std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const BOOL ok = WriteFile(f, bytes.data(), (DWORD)bytes.size(), &put, nullptr) && put == bytes.size();
    CloseHandle(f);
    if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(tmp.c_str()); return false; }
    return true;
}

} // namespace detail

// The step machine. Tick() once per presented frame; at most one step starts per tick, so a step sees the frame the
// previous step caused. On a failure (or the timeout) the remaining steps are skipped, except a last "quit", which
// still runs so the game closes; result.json is written before Host::Quit is called.
class Runner {
public:
    Runner(Host &host, Options options) : host_(host), opt_(std::move(options))
    {
        if (!opt_.startedAt.dwLowDateTime && !opt_.startedAt.dwHighDateTime) {
            FILETIME exitT, kernelT, userT;
            if (!GetProcessTimes(GetCurrentProcess(), &opt_.startedAt, &exitT, &kernelT, &userT)) opt_.startedAt.dwLowDateTime = 1;
        }
    }
    bool Running() const { return running_; }

    void Tick() { Tick(GetTickCount64()); }
    void Tick(uint64_t nowMs)
    {
        if (!running_) { poll(nowMs); return; }
        step(nowMs);
    }

private:
    struct Done { std::string status; uint64_t ms = 0; std::string reason, file; };
    Host &host_;
    Options opt_;
    FILETIME seen_{};
    uint64_t nextPoll_ = 0;
    bool running_ = false, quitting_ = false;
    detail::Script script_;
    std::vector<Done> done_;
    size_t at_ = 0;
    bool stepStarted_ = false;
    uint64_t runStart_ = 0, stepStart_ = 0, frames_ = 0;
    unsigned failStep_ = 0;
    std::string failReason_;
    std::wstring runDir_;

    void logf(const std::string &line) const { if (opt_.log) opt_.log(("selftest: " + line).c_str()); }

    void poll(uint64_t now)
    {
        if (now < nextPoll_) return;
        nextPoll_ = now + 500;
        if (opt_.folder.empty()) return;
        const std::wstring file = opt_.folder + L"\\script.json";
        WIN32_FILE_ATTRIBUTE_DATA a;
        if (!GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &a)) return;
        if (CompareFileTime(&a.ftLastWriteTime, &seen_) == 0) return;
        seen_ = a.ftLastWriteTime;
        // A script left from an earlier run never starts: the owner's own launch must not run a test.
        if (CompareFileTime(&a.ftLastWriteTime, &opt_.startedAt) < 0) return;
        if (!DevOptIn(opt_.iniPath)) { logf("script.json ignored: the developer opt-in is off (DBCE_DEV=1 or [Dev] SelfTest=true)"); return; }
        std::string text;
        HANDLE f = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return;
        LARGE_INTEGER size;
        if (GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (1 << 20)) {
            text.resize((size_t)size.QuadPart);
            DWORD got = 0;
            if (!ReadFile(f, &text[0], (DWORD)text.size(), &got, nullptr)) got = 0;
            text.resize(got);
        }
        CloseHandle(f);
        begin(text, now);
    }

    void begin(const std::string &text, uint64_t now)
    {
        script_ = detail::Script();
        done_.clear();
        at_ = 0;
        stepStarted_ = false;
        quitting_ = false;
        failStep_ = 0;
        failReason_.clear();
        runStart_ = now;
        std::string error;
        const bool parsed = detail::ParseScript(text.data(), text.size(), script_, error);
        runDir_ = opt_.folder + L"\\" + detail::Wide(script_.name.empty() ? "invalid" : script_.name);
        CreateDirectoryW(runDir_.c_str(), nullptr);
        DeleteFileW((runDir_ + L"\\result.json").c_str());   // its existence means this run is over
        if (!parsed) {
            script_.steps.clear();
            fail(0, "the script: " + error);
            finish(now);
            return;
        }
        logf("running '" + script_.name + "' (" + std::to_string(script_.steps.size()) + " steps" + (script_.muteFfb ? ", force feedback muted)" : ")"));
        if (script_.muteFfb) {   // no step runs unmuted; a last quit still closes the game
            const Status s = host_.MuteFfb();
            if (s.kind != Status::Ok) fail(0, "could not mute force feedback: " + (s.kind == Status::Unsupported ? std::string("this mod does not support it") : s.reason));
        }
        running_ = true;
    }

    bool failed() const { return !failReason_.empty(); }
    void fail(unsigned stepNo, const std::string &reason)
    {
        if (failed()) return;
        failStep_ = stepNo;
        failReason_ = reason;
        logf((stepNo ? "step " + std::to_string(stepNo) + " failed: " : std::string("failed: ")) + reason);
    }

    // The callback for the current step; Busy keeps the step going.
    Status call(const detail::Step &s, uint64_t now, std::string &file)
    {
        using detail::Kind;
        switch (s.kind) {
        case Kind::Wait:
            if (s.arg == "frames") return frames_ >= s.count ? Status::ok() : Status::busy();
            if (s.arg == "ms") return now - stepStart_ >= s.count ? Status::ok() : Status::busy();
            { const char *w = host_.Where(); return w && s.arg == w ? Status::ok() : Status::busy(); }
        case Kind::F6: return s.arg == "open" ? host_.OpenPage(s.page.c_str(), s.view.c_str()) : host_.ClosePanel();
        case Kind::Start: return host_.StartRace(s.arg.c_str());
        case Kind::Camera: return host_.CycleCamera(s.arg == "next");
        case Kind::Key: return host_.PressModKey(s.arg.c_str());
        case Kind::Set: return host_.SetSession(s.arg.c_str(), s.value);
        case Kind::Capture:
            file = "frame-" + s.arg + ".png";
            return host_.Capture((runDir_ + L"\\" + detail::Wide(file)).c_str());
        case Kind::Dump: {
            file = "state-" + s.arg + ".json";
            Probes probes;
            Status st = host_.Dump(probes);
            if (st.kind != Status::Ok) return st;
            std::string o = "{\n";
            for (size_t i = 0; i < probes.size(); ++i) {
                o += "  ";
                detail::Quote(o, probes[i].first);
                o += ": ";
                detail::Put(o, probes[i].second);
                o += i + 1 < probes.size() ? ",\n" : "\n";
            }
            o += "}\n";
            if (!detail::WriteFileAtomic(runDir_ + L"\\" + detail::Wide(file), o)) return Status::fail("could not write " + file);
            return st;
        }
        case Kind::Quit: return Status::ok();
        }
        return Status::unsupported();
    }

    void step(uint64_t now)
    {
        const auto &steps = script_.steps;
        if (at_ >= steps.size()) { finish(now); return; }
        const detail::Step &s = steps[at_];
        const unsigned no = (unsigned)at_ + 1;
        if (failed() && s.kind != detail::Kind::Quit) { done_.push_back({"skipped", 0, {}, {}}); ++at_; return; }
        if (!stepStarted_) { stepStarted_ = true; stepStart_ = now; frames_ = 0; }
        else ++frames_;
        if (s.kind == detail::Kind::Quit) {   // result.json first: the game may be gone when Quit returns
            done_.push_back({"ok", now - stepStart_, {}, {}});
            ++at_;
            quitting_ = true;
            finish(now);
            return;
        }
        std::string file;
        const Status r = call(s, now, file);
        if (r.kind == Status::Busy) {
            if (now - runStart_ < (uint64_t)script_.timeoutSeconds * 1000) return;
            const std::string why = "timed out after " + std::to_string(script_.timeoutSeconds) + " s";
            done_.push_back({"failed", now - stepStart_, why, {}});
            fail(no, why);
        } else if (r.kind == Status::Ok) {
            done_.push_back({"ok", now - stepStart_, {}, file});
        } else {
            const std::string why = r.kind == Status::Unsupported ? std::string("this mod does not support ") + detail::KindName(s.kind) : r.reason.empty() ? std::string("failed") : r.reason;
            done_.push_back({"failed", now - stepStart_, why, {}});
            fail(no, why);
        }
        ++at_;
        stepStarted_ = false;
        if (at_ >= steps.size()) finish(now);
    }

    void finish(uint64_t now)
    {
        running_ = false;
        const bool pass = !failed();
        std::string log = host_.LogTail();
        const bool haveLog = !log.empty() && detail::WriteFileAtomic(runDir_ + L"\\log.txt", log);
        std::string o = "{\n  \"schema\": \"dbce.selftest.result@1\",\n  \"name\": ";
        detail::Quote(o, script_.name);
        o += ",\n  \"pass\": ";
        o += pass ? "true" : "false";
        o += ",\n  \"muteFfb\": ";
        o += script_.muteFfb ? "true" : "false";
        o += ",\n  \"ms\": " + std::to_string(now - runStart_);
        o += ",\n  \"failure\": ";
        if (pass) o += "null";
        else { o += "{\"step\": " + std::to_string(failStep_) + ", \"reason\": "; detail::Quote(o, failReason_); o += "}"; }
        if (haveLog) o += ",\n  \"log\": \"log.txt\"";
        o += ",\n  \"steps\": [";
        for (size_t i = 0; i < done_.size(); ++i) {
            const Done &d = done_[i];
            o += i ? ",\n    " : "\n    ";
            o += "{\"step\": " + std::to_string(i + 1) + ", \"do\": ";
            detail::Quote(o, script_.steps[i].text);
            o += ", \"status\": \"" + d.status + "\", \"ms\": " + std::to_string(d.ms);
            if (!d.file.empty()) { o += ", \"file\": "; detail::Quote(o, d.file); }
            if (!d.reason.empty()) { o += ", \"reason\": "; detail::Quote(o, d.reason); }
            o += "}";
        }
        o += done_.empty() ? "]\n}\n" : "\n  ]\n}\n";
        if (!detail::WriteFileAtomic(runDir_ + L"\\result.json", o)) logf("could not write result.json");
        logf(std::string("'") + (script_.name.empty() ? "invalid" : script_.name) + "' " + (pass ? "passed" : "failed") + " in " + std::to_string(now - runStart_) + " ms");
        if (quitting_) {
            quitting_ = false;
            const Status q = host_.Quit();
            if (q.kind != Status::Ok) logf("quit: " + (q.kind == Status::Unsupported ? std::string("this mod does not support it") : q.reason));
        }
    }
};

}} // namespace dbce::selftest
