// Actual production table, fake admission/range callbacks. No Windows/device APIs.
#include "dbce_inject_table.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace dbce::controls;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (0)
static const std::string dev = "{00000000-0000-0000-0000-000000000001}";
struct Permit {
    bool allowed = true, throws = false;
    int calls = 0, failAt = 0;
    static bool check(void *p) {
        auto &s = *static_cast<Permit *>(p); ++s.calls;
        if (s.throws) throw std::runtime_error("fake observation failure");
        return s.allowed && s.calls != s.failAt;
    }
};
static InjectCommand command(Kind kind, int index = 0) {
    InjectCommand c; c.binding.dev = dev; c.binding.kind = kind; c.binding.index = index;
    c.binding.min = 0; c.binding.max = 65535; c.raw = kind == Kind::Axis ? 60000 : kind == Kind::Button ? 1 : 9000; c.ms = 500;
    return c;
}
struct Read {
    long axes[8] = {17}; unsigned char buttons[128]{}; unsigned long povs[4] = {0xFFFFFFFF};
    StateView view{axes, buttons, 128, povs, 4}; std::vector<std::string> log;
    bool natural() const { return axes[0] == 17 && buttons[0] == 0 && povs[0] == 0xFFFFFFFF; }
};
static bool range(int, long &lo, long &hi) { lo = 0; hi = 65535; return true; }
static void queue(InjectionTable &t) {
    CHECK(t.add(command(Kind::Axis), 10).empty()); CHECK(t.add(command(Kind::Button), 10).empty()); CHECK(t.add(command(Kind::Hat), 10).empty());
}
int main() {
    { InjectionTable t; queue(t); Read r; CHECK(t.apply(dev, r.view, 20, range, r.log) == 3); CHECK(!r.natural()); }
    { Permit p; InjectionTable t(Permit::check, &p); CHECK(t.pollAdmission()); CHECK(p.calls == 0); queue(t);
      Read r; CHECK(t.apply(dev, r.view, 20, range, r.log) == 3); CHECK(r.axes[0] == 60000 && r.buttons[0] == 128 && r.povs[0] == 9000); CHECK(r.log.size() == 3); }
    { Permit p; InjectionTable t(Permit::check, &p); queue(t); p.allowed = false;
      Read r; CHECK(t.apply(dev, r.view, 20, range, r.log) == 0); CHECK(r.natural() && r.log.empty()); CHECK(!t.any() && !t.pendingFor(dev));
      p.allowed = true; CHECK(!t.add(command(Kind::Button), 30).empty()); CHECK(!t.pollAdmission()); CHECK(t.apply(dev, r.view, 40, range, r.log) == 0); }
    { Permit p; InjectionTable t(Permit::check, &p); queue(t);
      Read r; CHECK(t.apply(dev, r.view, 20, [&](int a, long &lo, long &hi) { p.allowed = false; return range(a, lo, hi); }, r.log) == 0);
      CHECK(r.natural() && r.log.empty() && !t.any()); }
    { Permit p; InjectionTable t(Permit::check, &p); queue(t); p.failAt = p.calls + 3; // after staging, before publication
      Read r; CHECK(t.apply(dev, r.view, 20, range, r.log) == 0); CHECK(r.natural() && r.log.empty() && !t.any()); }
    { Permit p; InjectionTable t(Permit::check, &p); queue(t); p.allowed = false; CHECK(!t.pollAdmission()); CHECK(!t.any());
      p.allowed = true; CHECK(!t.pollAdmission()); CHECK(!t.add(command(Kind::Hat), 30).empty()); }
    { Permit p; InjectionTable t(Permit::check, &p); queue(t); t.revoke(); p.allowed = true;
      CHECK(!t.any() && !t.pollAdmission()); CHECK(!t.add(command(Kind::Axis), 20).empty()); }
    { Permit p; p.throws = true; InjectionTable t(Permit::check, &p); CHECK(!t.add(command(Kind::Button), 20).empty());
      p.throws = false; CHECK(!t.add(command(Kind::Button), 30).empty()); }
    { Permit p; InjectionTable t(Permit::check, &p); queue(t); t.expire(600); CHECK(!t.any());
      p.allowed = false; CHECK(!t.pollAdmission()); p.allowed = true; CHECK(!t.add(command(Kind::Button), 700).empty()); }
    { Permit p; InjectionTable t(Permit::check, &p); queue(t); Read r;
      CHECK(t.apply(dev, r.view, 20, [&](int a, long &lo, long &hi) { t.revoke(); return range(a, lo, hi); }, r.log) == 0);
      CHECK(r.natural() && !t.any()); }
    { Permit p; InjectionTable t(Permit::check, &p); CHECK(t.add(command(Kind::Axis), 10).empty()); Read r;
      CHECK(t.apply(dev, r.view, 20, [&](int a, long &lo, long &hi) { CHECK(t.add(command(Kind::Button), 20).empty()); return range(a, lo, hi); }, r.log) == 2);
      CHECK(r.axes[0] == 60000 && r.buttons[0] == 128); }
    std::cout << "PASS " << checks << " injection admission checks\n";
}
