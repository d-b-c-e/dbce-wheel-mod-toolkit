// F6 settings descriptor reader (UI-1): the GRID and art of rally samples parse into the standard pages, and every
// malformed or non-standard descriptor is refused with a reason. CPU only; built for x64 and x86 (build_test.ps1).
#include "dbce_panel_descriptor.h"
#include <stdio.h>
#include <string.h>
#include <string>

using namespace dbce::panel;

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static const Row *find(const Descriptor &d, const char *id)
{
    for (const Row &r : d.rows) if (r.id == id) return &r;
    return nullptr;
}

static int count(const Descriptor &d, StdPage page, RowLevel level)
{
    int n = 0;
    for (const Row &r : d.rows) n += r.page == page && r.level == level;
    return n;
}

// Parses `body` as the pages object of an otherwise valid descriptor; expects failure mentioning `reason`.
static void refuse(const char *pages, const char *reason, int line)
{
    std::string json = std::string("{\"settings\":{\"product\":\"Test Wheel Mod\",\"pages\":") + pages + "}}";
    Descriptor d;
    std::string error;
    const bool ok = ParseDescriptor(json.data(), json.size(), d, error);
    if (ok || error.find(reason) == std::string::npos) {
        fprintf(stderr, "FAIL line %d: expected refusal \"%s\", got %s \"%s\"\n", line, reason, ok ? "success" : "error", error.c_str());
        ++failures;
    }
    if (!ok) CHECK(d.rows.empty() && d.product.empty());
}
#define REFUSE(pages, reason) refuse(pages, reason, __LINE__)

static void refuseText(const char *json, const char *reason, int line)
{
    Descriptor d;
    std::string error;
    const bool ok = ParseDescriptor(json, strlen(json), d, error);
    if (ok || error.find(reason) == std::string::npos) {
        fprintf(stderr, "FAIL line %d: expected refusal \"%s\", got %s \"%s\"\n", line, reason, ok ? "success" : "error", error.c_str());
        ++failures;
    }
}
#define REFUSE_TEXT(json, reason) refuseText(json, reason, __LINE__)

int main(int argc, char **argv)
{
    const char *samples = argc > 1 ? argv[1] : "samples";
    wchar_t path[260];
    std::string error;

    // GRID (2019), its own descriptor (UI-7): the game reads the wheel; Telemetry shows the game's own UDP; no developer rows.
    Descriptor grid;
    swprintf_s(path, L"%hs\\grid-2019.settings.json", samples);
    CHECK(LoadDescriptor(path, grid, error));
    if (!error.empty()) fprintf(stderr, "grid: %s\n", error.c_str());
    CHECK(grid.product == "GRID (2019) Wheel Mod" && grid.version == "0.2.26");
    CHECK(grid.keys == "F6 settings \xC2\xB7 F8 force feedback off \xC2\xB7 F7 camera view");
    CHECK(count(grid, StdPage::Telemetry, RowLevel::Simple) == 2 && count(grid, StdPage::Telemetry, RowLevel::Advanced) == 0);
    for (int p = 0; p < (int)StdPage::Developer; ++p) CHECK(count(grid, (StdPage)p, RowLevel::Developer) == 0);
    const Row *screens = find(grid, "display.screens");
    CHECK(screens && screens->kind == RowKind::Choice && screens->choices.size() == 4 && screens->defaultValue == 1 && screens->nextStart);
    const Row *strength = find(grid, "ffb.strength");
    CHECK(strength && strength->kind == RowKind::Percent && strength->minValue == 0 && strength->maxValue == 100 && strength->defaultValue == 50 && !strength->nextStart);
    const Row *angle = find(grid, "display.side-angle");
    CHECK(angle && angle->kind == RowKind::Slider && angle->unit == "\xC2\xB0" && angle->level == RowLevel::Simple && angle->nextStart);
    const Row *curve = find(grid, "display.curve");
    CHECK(curve && curve->kind == RowKind::Slider && curve->maxValue == 20000 && curve->defaultValue == 1500 && curve->level == RowLevel::Advanced);
    const Row *gain = find(grid, "ffb.normalize-gain");
    CHECK(gain && gain->level == RowLevel::Advanced && gain->nextStart && gain->defaultValue == 0);
    const Row *status = find(grid, "controls.status");
    CHECK(status && status->kind == RowKind::Status && status->label.empty() && status->text == "Set up your wheel in the game's Controls menu.");

    // art of rally: every standard page, five bindings on Controls, the rest under More options.
    Descriptor art;
    swprintf_s(path, L"%hs\\art-of-rally.settings.json", samples);
    CHECK(LoadDescriptor(path, art, error));
    if (!error.empty()) fprintf(stderr, "art: %s\n", error.c_str());
    for (int p = 0; p < (int)StdPage::Developer; ++p) CHECK(count(art, (StdPage)p, RowLevel::Simple) > 0);
    CHECK(count(art, StdPage::Controls, RowLevel::Simple) == 6);   // status + five bindings
    const Row *steer = find(art, "controls.steering");
    CHECK(steer && steer->kind == RowKind::Binding && steer->help == "Turn the wheel fully left, then right.");
    const Row *gearbox = find(art, "controls.gearbox");
    CHECK(gearbox && gearbox->choices.size() == 3 && gearbox->defaultValue == 0);
    const Row *port = find(art, "telemetry.port");
    CHECK(port && port->minValue == 1 && port->maxValue == 65535 && port->defaultValue == 8000);
    CHECK(find(art, "controls.open-settings") && find(art, "controls.open-settings")->text == "Keyboard \xC2\xB7 F6");
    int kinds[8] = {};
    for (const Row &r : art.rows) ++kinds[(int)r.kind];
    for (int k = 0; k < 8; ++k) CHECK(kinds[k] > 0);   // the art sample uses every kind

    // Without the "settings" wrapper (a dbce-settings.json beside the mod), with a BOM, escapes and surrogate pairs.
    {
        const char json[] = "\xEF\xBB\xBF{\"product\":\"A \\u00b7 B \\ud83d\\ude97\",\"pages\":{\"help\":[{\"id\":\"h.a\",\"kind\":\"action\",\"label\":\"Go\\n\"}]}}";
        Descriptor d;
        CHECK(ParseDescriptor(json, sizeof json - 1, d, error));
        CHECK(d.product == "A \xC2\xB7 B \xF0\x9F\x9A\x97" && d.rows.size() == 1 && d.rows[0].label == "Go\n" && d.rows[0].page == StdPage::Help);
    }

    // Refusals: what is not one of the standard pages, kinds, levels and fields.
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"knob\",\"label\":\"A\"}]}", "unknown kind 'knob'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"Toggle\",\"label\":\"A\"}]}", "unknown kind 'Toggle'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"widget\",\"label\":\"A\"}]}", "unknown kind 'widget'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"label\":\"A\"}]}", "missing 'kind'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":7,\"label\":\"A\"}]}", "'kind' must be a string");
    REFUSE("{\"graphics\":[]}", "unknown page 'graphics'");
    REFUSE("{\"developer\":[]}", "unknown page 'developer'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"level\":\"expert\"}]}", "unknown level 'expert'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"apply\":\"restart\"}]}", "unknown apply 'restart'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"colour\":\"red\"}]}", "unknown field 'colour'");
    REFUSE("{\"ffb\":[{\"kind\":\"toggle\",\"label\":\"A\"}]}", "missing 'id'");
    REFUSE("{\"ffb\":[{\"id\":\"a b\",\"kind\":\"toggle\",\"label\":\"A\"}]}", "bad id");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\"}],\"help\":[{\"id\":\"a\",\"kind\":\"action\",\"label\":\"B\"}]}", "duplicate id");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\"}]}", "missing 'label'");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"\"}]}", "empty label");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"default\":1}]}", "true or false");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"help\":\"one two three four five six seven eight nine ten eleven twelve thirteen\"}]}", "over 12 words");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"choice\",\"label\":\"A\",\"choices\":[\"X\"]}]}", "2-8 choices");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"choice\",\"label\":\"A\",\"choices\":[\"X\",\"X\"]}]}", "duplicate choice");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"choice\",\"label\":\"A\",\"choices\":[\"X\",\"Y\"],\"default\":\"Z\"}]}", "not one of the choices");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"choice\",\"label\":\"A\",\"choices\":[\"X\",\"Y\"],\"default\":2}]}", "not one of the choices");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"choices\":[\"X\",\"Y\"]}]}", "only a choice has choices");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"slider\",\"label\":\"A\",\"min\":0}]}", "needs min and max");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"slider\",\"label\":\"A\",\"min\":5,\"max\":5}]}", "min must be below max");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"percent\",\"label\":\"A\",\"default\":120}]}", "outside min-max");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"slider\",\"label\":\"A\",\"min\":0,\"max\":1.5}]}", "whole numbers");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"number\",\"label\":\"A\",\"min\":0,\"max\":9,\"decimals\":5}]}", "decimals is 0-3");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"min\":0,\"max\":9}]}", "only percent, slider and number have a range");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"unit\":\"cm\"}]}", "only a slider or number has a unit");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"action\",\"label\":\"A\",\"default\":true}]}", "has no default");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"action\",\"label\":\"A\",\"apply\":\"next-start\"}]}", "cannot be next-start");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"toggle\",\"label\":\"A\",\"text\":\"x\"}]}", "only a status or binding has text");
    REFUSE("{\"ffb\":{\"id\":\"a\"}}", "must be an array of rows");
    REFUSE("{\"ffb\":[\"a\"]}", "must be an object");
    REFUSE("{\"controls\":[{\"id\":\"a\",\"kind\":\"binding\",\"label\":\"A\"},{\"id\":\"b\",\"kind\":\"binding\",\"label\":\"B\"},"
           "{\"id\":\"c\",\"kind\":\"binding\",\"label\":\"C\"},{\"id\":\"d\",\"kind\":\"binding\",\"label\":\"D\"},"
           "{\"id\":\"e\",\"kind\":\"binding\",\"label\":\"E\"},{\"id\":\"f\",\"kind\":\"binding\",\"label\":\"F\"}]}", "more than five simple controls");
    REFUSE("{\"ffb\":[{\"id\":\"a\",\"kind\":\"status\"},{\"id\":\"b\",\"kind\":\"status\"},{\"id\":\"c\",\"kind\":\"status\"}]}", "more than two simple status lines");
    REFUSE_TEXT("{\"settings\":{\"pages\":{}}}", "missing 'product'");
    REFUSE_TEXT("{\"settings\":{\"product\":\"P\",\"pages\":{},\"theme\":\"dark\"}}", "unknown field 'theme'");
    REFUSE_TEXT("{\"settings\":{\"product\":\"P\"}}", "'pages' must be an object");
    REFUSE_TEXT("[1,2]", "the settings block must be an object");
    REFUSE_TEXT("{\"product\":\"P\",\"pages\":{}", "expected ',' or '}'");
    REFUSE_TEXT("{\"product\":\"P\",\"pages\":{}} x", "text after the JSON value");
    REFUSE_TEXT("{\"product\":\"P\",\"product\":\"Q\",\"pages\":{}}", "duplicate member");
    REFUSE_TEXT("{\"product\":\"\\ud83d\",\"pages\":{}}", "unpaired surrogate");
    REFUSE_TEXT("{\"product\":\"\xC0\xAF\",\"pages\":{}}", "invalid UTF-8");
    REFUSE_TEXT("{\"product\":\"P\",\"pages\":{\"ffb\":[{\"id\":\"a\",\"kind\":\"percent\",\"label\":\"A\",\"default\":01}]}}", "expected ',' or '}'");
    REFUSE_TEXT("", "unexpected end");

    printf(failures ? "FAIL %d descriptor checks\n" : "PASS descriptor checks (GRID %d rows, art %d rows)\n",
           failures ? failures : (int)grid.rows.size(), (int)art.rows.size());
    return failures ? 1 : 0;
}
