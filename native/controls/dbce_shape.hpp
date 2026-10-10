// Pedal rest shaping (STD-033, docs/controls-contract.md). Some games' pedal filters read only the device's nominal end
// as released (V-Rally 4's MaxDefaultStick / InvertMaxDefaultStick), so a pedal that rests inside its range (the owner's
// brake rests at 1743 of 0..65535) reads slightly pressed at rest. A proxy that sees GetDeviceState can replace that
// axis's sample with the one the game's own filter reads as the contract's normalized value (normalize() in
// dbce_controls.hpp): released at the captured rest, full at the captured full end, linear between.
// Header-only, no Windows calls; test_shape.cpp checks it on x64 and x86. Opt-in per game: an empty table changes nothing.
#pragma once
#include "dbce_controls.hpp"
#include "dbce_inject_table.hpp"   // StateView
#include <cmath>
#include <string>
#include <vector>

namespace dbce { namespace controls {

// The end the game's pedal filter reads as released: the minimum when the captured travel rises from it and the player
// did not invert the pedal, else the maximum. V-Rally's adapter picks its filter by the same rule (vr_controls.hpp).
inline bool releasedAtMin(const Binding &b) { return (b.travel > 0) != b.inverted; }

// A pedal needs a shape when its rest is not the end its travel starts from: only then does the game's end-anchored
// filter differ from the contract (with the rest at that end, shapePedal() is the identity).
inline bool needsShape(const std::string &action, const Binding &b)
{
    return b.kind == Kind::Axis && shapeOf(action) == Shape::Pedal && b.rest != (b.travel > 0 ? b.min : b.max);
}

// A sample in the range the game set on the axis (gmin..gmax) -> the sample the game's filter reads as the contract's
// normalized value. Widened arithmetic throughout; rounded half away from zero; clamped to gmin..gmax. Assumes
// fits(action, b) == "" (ShapeTable::add checks it).
inline long shapePedal(const std::string &action, const Binding &b, long raw, long gmin, long gmax)
{
    if (gmax <= gmin) return raw;
    const long captured = rescale(raw, gmin, gmax, b.min, b.max);
    const double n = normalize(action, b, captured);
    const double t = releasedAtMin(b) ? n : 1.0 - n;
    const double r = std::round(double(gmin) + t * (double(gmax) - double(gmin)));
    if (r <= double(gmin)) return gmin;
    if (r >= double(gmax)) return gmax;
    return (long)r;
}

struct PedalShape {
    std::string instance;   // the binding's dev, lowercase "{...}" as parse() stores it
    int axis = 0;           // DIJOYSTATE axis 0-7 (X Y Z Rx Ry Rz Slider0 Slider1)
    std::string action;
    Binding binding;
};

struct ShapeResult {
    int shaped = 0;
    int unavailable = 0;   // a shape this device needs whose game range could not be read or is not increasing: that
                           // axis is left as read, and the caller must report it (never as a shaped/calibrated value)
};

// Built once per process (from the profile at game start), then only read. A hook publishes it by setting a pointer
// once, after it is complete, and never replaces or frees it while the process can still read devices.
class ShapeTable {
public:
    // A pedal axis binding that needs a shape; anything else is refused with a reason (nothing added).
    bool add(const std::string &action, const Binding &b, std::string &why)
    {
        why.clear();
        if (b.kind != Kind::Axis || shapeOf(action) != Shape::Pedal) { why = "not a pedal axis"; return false; }
        const std::string f = fits(action, b);
        if (!f.empty()) { why = f; return false; }
        if (b.max <= b.min || b.rest < b.min || b.rest > b.max) { why = "rest outside the range"; return false; }
        if (!needsShape(action, b)) { why = "rest at the travel's start: no shape needed"; return false; }
        if (b.index < 0 || b.index > 7) { why = "bad-index"; return false; }
        for (auto &s : shapes_)
            if (s.instance == b.dev && s.axis == b.index) { why = "this axis is already shaped (" + s.action + ")"; return false; }
        shapes_.push_back({b.dev, (int)b.index, action, b});
        return true;
    }
    bool any() const { return !shapes_.empty(); }
    const std::vector<PedalShape> &shapes() const { return shapes_; }

    // Shapes this device's axes in a just-read standard state (c_dfDIJoystick/2 only: the caller checks the format).
    // range(axis, min, max) reports the range the game set on the axis (DIPROP_RANGE). A range that cannot be read or is
    // not increasing is never assumed: that axis is left as read and counted unavailable.
    template <class Range>
    ShapeResult apply(const std::string &instance, StateView &v, Range range) const
    {
        ShapeResult r;
        for (auto &s : shapes_) {
            if (s.instance != instance) continue;
            long mn = 0, mx = 0;
            if (!v.axes || !range(s.axis, mn, mx) || mx <= mn) { ++r.unavailable; continue; }
            v.axes[s.axis] = shapePedal(s.action, s.binding, v.axes[s.axis], mn, mx);
            ++r.shaped;
        }
        return r;
    }

private:
    std::vector<PedalShape> shapes_;
};

}} // namespace dbce::controls
