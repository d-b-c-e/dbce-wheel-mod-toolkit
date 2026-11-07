// Strict reader of the F6 settings descriptor (see dbce_panel_descriptor.h). A small JSON parser (RFC 8259 values,
// UTF-8 text, \u escapes including surrogate pairs) feeds a validator that knows the fixed page set and row kinds.
#include "dbce_panel_descriptor.h"
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

namespace dbce { namespace panel {

namespace {

struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;
    const Json *get(const char *key) const
    {
        for (const auto &kv : o) if (kv.first == key) return &kv.second;
        return nullptr;
    }
};

class Parser {
public:
    Parser(const char *text, size_t length, std::string &error) : p_(text), end_(text + length), error_(error) {}
    bool document(Json &out)
    {
        if (end_ - p_ >= 3 && (unsigned char)p_[0] == 0xEF && (unsigned char)p_[1] == 0xBB && (unsigned char)p_[2] == 0xBF) p_ += 3;
        if (!value(out, 0)) return false;
        ws();
        return p_ == end_ || fail("text after the JSON value");
    }

private:
    const char *p_, *end_;
    std::string &error_;
    const char *start_ = p_;

    bool fail(const char *what)
    {
        char b[96];
        sprintf_s(b, "JSON: %s at byte %d", what, (int)(p_ - start_));
        error_ = b;
        return false;
    }
    void ws() { while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_; }
    bool lit(const char *word)
    {
        size_t n = strlen(word);
        if ((size_t)(end_ - p_) < n || memcmp(p_, word, n)) return false;
        p_ += n;
        return true;
    }
    static void utf8(std::string &out, unsigned cp)
    {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    bool hex4(unsigned &v)
    {
        if (end_ - p_ < 4) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = *p_++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        return true;
    }
    bool string(std::string &out)
    {
        ++p_;   // opening quote
        while (p_ < end_ && *p_ != '"') {
            unsigned char c = (unsigned char)*p_;
            if (c < 0x20) return fail("control character in a string");
            if (c != '\\') {
                // Copy one UTF-8 sequence, checking its shape (no overlongs or surrogates checked beyond the lead).
                int n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 && c >= 0xC2 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 && c <= 0xF4 ? 4 : 0;
                if (!n || end_ - p_ < n) return fail("invalid UTF-8");
                for (int i = 1; i < n; ++i) if (((unsigned char)p_[i] & 0xC0) != 0x80) return fail("invalid UTF-8");
                out.append(p_, (size_t)n);
                p_ += n;
                continue;
            }
            if (++p_ == end_) break;
            char e = *p_++;
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned cp;
                if (!hex4(cp)) return fail("bad \\u escape");
                if (cp >= 0xD800 && cp < 0xDC00) {
                    unsigned lo;
                    if (end_ - p_ < 2 || p_[0] != '\\' || p_[1] != 'u') return fail("unpaired surrogate");
                    p_ += 2;
                    if (!hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) return fail("unpaired surrogate");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp < 0xE000) return fail("unpaired surrogate");
                utf8(out, cp);
                break;
            }
            default: return fail("bad escape");
            }
        }
        if (p_ == end_) return fail("unterminated string");
        ++p_;
        return true;
    }
    bool number(double &out)
    {
        const char *s = p_;
        if (p_ < end_ && *p_ == '-') ++p_;
        if (p_ == end_ || *p_ < '0' || *p_ > '9') return fail("bad number");
        if (*p_ == '0') ++p_; else while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        if (p_ < end_ && *p_ == '.') {
            ++p_;
            if (p_ == end_ || *p_ < '0' || *p_ > '9') return fail("bad number");
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
            ++p_;
            if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
            if (p_ == end_ || *p_ < '0' || *p_ > '9') return fail("bad number");
            while (p_ < end_ && *p_ >= '0' && *p_ <= '9') ++p_;
        }
        std::string t(s, (size_t)(p_ - s));
        out = strtod(t.c_str(), nullptr);
        return isfinite(out) || fail("number out of range");
    }
    bool value(Json &out, int depth)
    {
        if (depth > 16) return fail("nested too deeply");
        ws();
        if (p_ == end_) return fail("unexpected end");
        char c = *p_;
        if (c == '{') {
            out.type = Json::Object;
            ++p_; ws();
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }
            for (;;) {
                ws();
                if (p_ == end_ || *p_ != '"') return fail("expected a member name");
                std::string key;
                if (!string(key)) return false;
                for (const auto &kv : out.o) if (kv.first == key) return fail(("duplicate member '" + key + "'").c_str());
                ws();
                if (p_ == end_ || *p_ != ':') return fail("expected ':'");
                ++p_;
                out.o.emplace_back(key, Json());
                if (!value(out.o.back().second, depth + 1)) return false;
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == '}') { ++p_; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            out.type = Json::Array;
            ++p_; ws();
            if (p_ < end_ && *p_ == ']') { ++p_; return true; }
            for (;;) {
                out.a.emplace_back();
                if (!value(out.a.back(), depth + 1)) return false;
                ws();
                if (p_ < end_ && *p_ == ',') { ++p_; continue; }
                if (p_ < end_ && *p_ == ']') { ++p_; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { out.type = Json::String; return string(out.s); }
        if (lit("true")) { out.type = Json::Bool; out.b = true; return true; }
        if (lit("false")) { out.type = Json::Bool; out.b = false; return true; }
        if (lit("null")) { out.type = Json::Null; return true; }
        out.type = Json::Number;
        return number(out.n);
    }
};

// ---- validation -----------------------------------------------------------------------------------------------
struct Checker {
    std::string &error;
    bool fail(const std::string &what) { error = what; return false; }

    bool str(const Json &row, const char *key, std::string &out, const std::string &where, bool required)
    {
        const Json *v = row.get(key);
        if (!v) return !required || fail(where + ": missing '" + key + "'");
        if (v->type != Json::String) return fail(where + ": '" + key + "' must be a string");
        out = v->s;
        return true;
    }
    bool num(const Json &row, const char *key, double &out, const std::string &where)
    {
        const Json *v = row.get(key);
        if (!v) return true;
        if (v->type != Json::Number) return fail(where + ": '" + key + "' must be a number");
        out = v->n;
        return true;
    }
    static int words(const std::string &s)
    {
        int n = 0;
        bool in = false;
        for (char c : s) {
            const bool space = c == ' ' || c == '\t' || c == '\n';
            if (!space && !in) ++n;
            in = !space;
        }
        return n;
    }
    static bool idOk(const std::string &id)
    {
        if (id.empty() || id.size() > 63) return false;
        for (char c : id)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
        return true;
    }

    bool row(const Json &j, StdPage page, Row &r)
    {
        std::string where = std::string("page '") + StdPageId(page) + "' row";
        if (j.type != Json::Object) return fail(where + ": must be an object");
        static const char *const kFields[] = {"id", "kind", "label", "help", "level", "apply", "choices", "min", "max",
                                              "default", "unit", "decimals", "text"};
        if (!str(j, "id", r.id, where, true)) return false;
        if (!idOk(r.id)) return fail(where + ": bad id '" + r.id + "'");
        where = "row '" + r.id + "'";
        for (const auto &kv : j.o) {
            bool known = false;
            for (const char *f : kFields) known = known || kv.first == f;
            if (!known) return fail(where + ": unknown field '" + kv.first + "'");
        }
        r.page = page;
        std::string kind, level = "simple", apply = "live";
        if (!str(j, "kind", kind, where, true)) return false;
        static const char *const kKinds[] = {"toggle", "choice", "percent", "slider", "number", "binding", "action", "status"};
        int k = -1;
        for (int i = 0; i < 8; ++i) if (kind == kKinds[i]) k = i;
        if (k < 0) return fail(where + ": unknown kind '" + kind + "'");
        r.kind = (RowKind)k;
        if (!str(j, "label", r.label, where, r.kind != RowKind::Status)) return false;
        if (r.kind != RowKind::Status && r.label.empty()) return fail(where + ": empty label");
        if (!str(j, "help", r.help, where, false) || !str(j, "unit", r.unit, where, false) || !str(j, "text", r.text, where, false)) return false;
        if (words(r.help) > 12) return fail(where + ": help is over 12 words");
        if (!str(j, "level", level, where, false) || !str(j, "apply", apply, where, false)) return false;
        if (level == "simple") r.level = RowLevel::Simple;
        else if (level == "advanced") r.level = RowLevel::Advanced;
        else if (level == "developer") r.level = RowLevel::Developer;
        else return fail(where + ": unknown level '" + level + "'");
        if (apply == "live") r.nextStart = false;
        else if (apply == "next-start") r.nextStart = true;
        else return fail(where + ": unknown apply '" + apply + "'");
        if (r.nextStart && (r.kind == RowKind::Action || r.kind == RowKind::Status)) return fail(where + ": an action or status cannot be next-start");

        const Json *def = j.get("default");
        const bool numeric = r.kind == RowKind::Percent || r.kind == RowKind::Slider || r.kind == RowKind::Number;
        if (j.get("choices") && r.kind != RowKind::Choice) return fail(where + ": only a choice has choices");
        if ((j.get("min") || j.get("max")) && !numeric) return fail(where + ": only percent, slider and number have a range");
        if (j.get("decimals") && r.kind != RowKind::Number) return fail(where + ": only a number has decimals");
        if (j.get("unit") && r.kind != RowKind::Slider && r.kind != RowKind::Number) return fail(where + ": only a slider or number has a unit");
        if (j.get("text") && r.kind != RowKind::Status && r.kind != RowKind::Binding) return fail(where + ": only a status or binding has text");
        if (def && (r.kind == RowKind::Status || r.kind == RowKind::Action || r.kind == RowKind::Binding))
            return fail(where + ": a " + kind + " has no default");
        switch (r.kind) {
        case RowKind::Toggle:
            if (def && def->type != Json::Bool) return fail(where + ": a toggle's default is true or false");
            r.defaultValue = def && def->b ? 1 : 0;
            break;
        case RowKind::Choice: {
            const Json *c = j.get("choices");
            if (!c || c->type != Json::Array || c->a.size() < 2 || c->a.size() > 8) return fail(where + ": a choice needs 2-8 choices");
            for (const Json &e : c->a) {
                if (e.type != Json::String || e.s.empty()) return fail(where + ": choices are non-empty strings");
                for (const std::string &seen : r.choices) if (seen == e.s) return fail(where + ": duplicate choice '" + e.s + "'");
                r.choices.push_back(e.s);
            }
            if (def) {
                int index = -1;
                if (def->type == Json::String) { for (size_t i = 0; i < r.choices.size(); ++i) if (r.choices[i] == def->s) index = (int)i; }
                else if (def->type == Json::Number && def->n == floor(def->n)) index = (int)def->n;
                if (index < 0 || index >= (int)r.choices.size()) return fail(where + ": default is not one of the choices");
                r.defaultValue = index;
            }
            break;
        }
        case RowKind::Percent: case RowKind::Slider: case RowKind::Number: {
            if (r.kind == RowKind::Percent) { r.minValue = 0; r.maxValue = 100; }
            else if (!j.get("min") || !j.get("max")) return fail(where + ": a " + kind + " needs min and max");
            if (!num(j, "min", r.minValue, where) || !num(j, "max", r.maxValue, where)) return false;
            if (!(r.minValue < r.maxValue)) return fail(where + ": min must be below max");
            r.defaultValue = r.minValue;
            if (def && def->type != Json::Number) return fail(where + ": default must be a number");
            if (def) r.defaultValue = def->n;
            if (r.defaultValue < r.minValue || r.defaultValue > r.maxValue) return fail(where + ": default is outside min-max");
            if (r.kind != RowKind::Number) {
                if (r.minValue != floor(r.minValue) || r.maxValue != floor(r.maxValue) || r.defaultValue != floor(r.defaultValue))
                    return fail(where + ": a " + kind + " uses whole numbers");
            }
            double d = 0;
            if (!num(j, "decimals", d, where)) return false;
            if (d < 0 || d > 3 || d != floor(d)) return fail(where + ": decimals is 0-3");
            r.decimals = (int)d;
            break;
        }
        default: break;
        }
        return true;
    }
};

} // namespace

bool ParseDescriptor(const char *json, size_t length, Descriptor &out, std::string &error)
{
    out = Descriptor();
    error.clear();
    Json root;
    Parser parser(json ? json : "", json ? length : 0, error);
    if (!parser.document(root)) return false;
    Checker ck{error};
    const Json *s = &root;
    if (root.type == Json::Object && root.get("settings")) s = root.get("settings");
    if (s->type != Json::Object) return ck.fail("the settings block must be an object");
    for (const auto &kv : s->o) {
        static const char *const kTop[] = {"product", "version", "modPage", "keys", "config", "pages"};
        bool known = false;
        for (const char *f : kTop) known = known || kv.first == f;
        if (!known) return ck.fail("settings: unknown field '" + kv.first + "'");
    }
    Descriptor d;
    if (!ck.str(*s, "product", d.product, "settings", true) || !ck.str(*s, "version", d.version, "settings", false) ||
        !ck.str(*s, "modPage", d.modPage, "settings", false) || !ck.str(*s, "keys", d.keys, "settings", false) ||
        !ck.str(*s, "config", d.config, "settings", false)) return false;
    if (d.product.empty()) return ck.fail("settings: empty product");
    const Json *pages = s->get("pages");
    if (!pages || pages->type != Json::Object) return ck.fail("settings: 'pages' must be an object");
    for (const auto &kv : pages->o) {
        int page = -1;
        for (int p = 0; p < (int)StdPage::Developer; ++p) if (kv.first == StdPageId((StdPage)p)) page = p;
        if (page < 0) return ck.fail("settings: unknown page '" + kv.first + "'");
        if (kv.second.type != Json::Array) return ck.fail("page '" + kv.first + "' must be an array of rows");
        int controls = 0, statuses = 0;
        for (const Json &j : kv.second.a) {
            Row r;
            if (!ck.row(j, (StdPage)page, r)) return false;
            for (const Row &seen : d.rows) if (seen.id == r.id) return ck.fail("row '" + r.id + "': duplicate id");
            if (r.level == RowLevel::Simple) (RowIsControl(r.kind) ? controls : statuses)++;
            d.rows.push_back(std::move(r));
        }
        if (controls > 5) return ck.fail("page '" + kv.first + "': more than five simple controls");
        if (statuses > 2) return ck.fail("page '" + kv.first + "': more than two simple status lines");
    }
    out = std::move(d);
    return true;
}

bool LoadDescriptor(const wchar_t *path, Descriptor &out, std::string &error)
{
    out = Descriptor();
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) { error = "descriptor: cannot open the file"; return false; }
    LARGE_INTEGER size;
    std::string text;
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (1 << 20);
    if (ok) {
        text.resize((size_t)size.QuadPart);
        DWORD got = 0;
        ok = text.empty() || (ReadFile(f, &text[0], (DWORD)text.size(), &got, nullptr) && got == text.size());
    }
    CloseHandle(f);
    if (!ok) { error = "descriptor: cannot read the file (or it is over 1 MB)"; return false; }
    return ParseDescriptor(text.data(), text.size(), out, error);
}

}} // namespace dbce::panel
