// Pedal rest shaping (controls/dbce_shape.hpp) applied to one just-read GetDeviceState buffer. Shared by the fence
// (FencedDevice::GetDeviceState) and the force layer's GetDeviceState hook, after any test injection, so an injected raw
// sample is shaped exactly like a physical one. Standard formats only (c_dfDIJoystick / c_dfDIJoystick2 read at their
// own size); a device with shapes read any other way is reported unavailable, never shaped by guesswork.
#pragma once
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <windows.h>
#include <dinput.h>
#include "../controls/dbce_shape.hpp"

namespace dbce { namespace shape {

// format: 1 c_dfDIJoystick, 2 c_dfDIJoystick2, 0 any other (dbce_joyformat.hpp standardFormat). range(axis, min, max)
// reads the range the game set (DIPROP_RANGE).
template <class Range>
dbce::controls::ShapeResult applyToState(const dbce::controls::ShapeTable &table, const std::string &instance, int format,
                                         DWORD cb, LPVOID data, Range range)
{
    dbce::controls::ShapeResult r;
    const int needed = table.countFor(instance);
    if (!needed) return r;
    const bool standard = (format == 1 && cb == sizeof(DIJOYSTATE)) || (format == 2 && cb == sizeof(DIJOYSTATE2));
    if (!data || !standard) { r.unavailable = needed; return r; }
    dbce::controls::StateView v;
    v.axes = (long *)&((DIJOYSTATE *)data)->lX;
    return table.apply(instance, v, range);
}

}} // namespace dbce::shape
