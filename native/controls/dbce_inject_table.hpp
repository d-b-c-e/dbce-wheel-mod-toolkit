// Running test injections (STD-033 section 6) and their delivery into one device read. Pure C++17 (no Windows headers):
// the DirectInput layer and the fenced device wrapper (dinput-proxy/dbce_fence.hpp) both use it, and the offline tests
// drive it directly. Devices are keyed by their instance GUID text ("{...}", lowercase), the only runtime match.
#pragma once
#include "dbce_inject.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace dbce { namespace controls {

// One device read, as the game asked for it (DIJOYSTATE or DIJOYSTATE2): the eight axes in DIJOYSTATE order, the
// buttons the format has (32 or 128), four POVs.
struct StateView {
    long *axes = nullptr;              // lX lY lZ lRx lRy lRz rglSlider[0] rglSlider[1]
    unsigned char *buttons = nullptr;
    int buttonCount = 0;
    unsigned long *povs = nullptr;
};

class InjectionTable {
public:
    // Adds or replaces the injection of the same object on the same instance. "" when accepted, else the reason.
    std::string add(const InjectCommand &c, uint64_t nowMs)
    {
        if (c.binding.dev.size() != 38) return "no device instance";
        if (c.ms < 50 || c.ms > 15000) return "ms must be 50-15000";
        if (c.binding.kind == Kind::Axis && (c.binding.index < 0 || c.binding.index > 7 || c.binding.max <= c.binding.min)) return "bad axis";
        if (c.binding.kind == Kind::Button && (c.binding.index < 0 || c.binding.index > 127)) return "bad button";
        if (c.binding.kind == Kind::Hat && (c.binding.index < 0 || c.binding.index > 3)) return "bad hat";
        if (c.binding.kind == Kind::None) return "bad kind";
        std::lock_guard<std::mutex> g(m_);
        Item *slot = nullptr;
        for (auto &i : items_) if (i.active && i.dev == c.binding.dev && i.kind == c.binding.kind && i.index == c.binding.index) slot = &i;
        if (!slot) for (auto &i : items_) if (!i.active) { slot = &i; break; }
        if (!slot) { if (items_.size() >= 16) return "16 injections already running"; items_.emplace_back(); slot = &items_.back(); }
        if (!slot->active) active_.fetch_add(1);
        *slot = Item{};
        slot->active = true;
        slot->dev = c.binding.dev; slot->kind = c.binding.kind; slot->index = c.binding.index; slot->value = c.raw;
        slot->bindMin = c.binding.min; slot->bindMax = c.binding.max; slot->until = nowMs + (uint64_t)c.ms; slot->ms = c.ms;
        return {};
    }
    bool any() const { return active_.load() > 0; }

    // Replace this read's samples for `dev`. `range(axis, min, max)` reads the range the game set on that axis; when
    // it cannot, that axis is not delivered (never guessed). Log lines (first delivery, refusal, end) go to `log`.
    template <class RangeFn>
    void apply(const std::string &dev, StateView &s, uint64_t nowMs, RangeFn range, std::vector<std::string> &log)
    {
        if (!any()) return;
        std::lock_guard<std::mutex> g(m_);
        for (auto &i : items_) {
            if (!i.active || i.dev != dev) continue;
            const char *kind = i.kind == Kind::Axis ? "axis" : i.kind == Kind::Button ? "button" : "hat";
            if (nowMs >= i.until) {
                i.active = false;
                active_.fetch_sub(1);
                log.push_back(std::string("inject: ") + kind + " " + std::to_string(i.index) + " on " + dev + " ended after " + std::to_string(i.ms) + " ms");
                continue;
            }
            if (i.kind == Kind::Axis) {
                if (!s.axes) continue;
                if (!i.ranged) { i.ranged = true; i.rangeOk = range(i.index, i.devMin, i.devMax) && i.devMax > i.devMin; }
                if (!i.rangeOk) {
                    if (!i.announced) { i.announced = true; log.push_back("inject: axis " + std::to_string(i.index) + " on " + dev + " not delivered: the game's range for it is unreadable"); }
                    continue;
                }
                long v = rescale(i.value, i.bindMin, i.bindMax, i.devMin, i.devMax);
                v = v < i.devMin ? i.devMin : v > i.devMax ? i.devMax : v;
                s.axes[i.index] = v;
                if (!i.announced) {
                    i.announced = true;
                    log.push_back("inject: axis " + std::to_string(i.index) + " on " + dev + " = " + std::to_string(v) + " (sample " + std::to_string(i.value) + " in " +
                                  std::to_string(i.bindMin) + ".." + std::to_string(i.bindMax) + ", the game's range " + std::to_string(i.devMin) + ".." +
                                  std::to_string(i.devMax) + ") for " + std::to_string(i.ms) + " ms");
                }
            } else if (i.kind == Kind::Button) {
                if (!s.buttons || i.index >= s.buttonCount) {
                    if (!i.announced) { i.announced = true; log.push_back("inject: button " + std::to_string(i.index) + " on " + dev + " is outside the " + std::to_string(s.buttonCount) + " buttons the game reads; not delivered"); }
                    continue;
                }
                s.buttons[i.index] = i.value ? 0x80 : 0;
                if (!i.announced) { i.announced = true; log.push_back("inject: button " + std::to_string(i.index) + " on " + dev + (i.value ? " down" : " up") + " for " + std::to_string(i.ms) + " ms"); }
            } else {
                if (!s.povs) continue;
                s.povs[i.index] = (unsigned long)i.value;   // -1 is centred (0xFFFFFFFF)
                if (!i.announced) { i.announced = true; log.push_back("inject: hat " + std::to_string(i.index) + " on " + dev + " = " + std::to_string(i.value) + " for " + std::to_string(i.ms) + " ms"); }
            }
        }
    }

private:
    struct Item {
        bool active = false;
        std::string dev;
        Kind kind = Kind::None;
        int index = -1, ms = 0;
        long value = 0, bindMin = 0, bindMax = 0, devMin = 0, devMax = 0;
        uint64_t until = 0;
        bool announced = false, ranged = false, rangeOk = false;
    };
    std::mutex m_;
    std::vector<Item> items_;
    std::atomic<int> active_{0};
};

}} // namespace dbce::controls
