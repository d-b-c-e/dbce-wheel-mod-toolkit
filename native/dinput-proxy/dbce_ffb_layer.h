// Shared force layer for DirectInput proxies (dinput8.dll mods): what the game asks of the wheel, and the player's
// control over it, in one place for every proxy game.
//
//  - Force log (bounded): device properties, cooperative level, acquisition, every effect's type and parameters, starts,
//    stops and the wheel's resting axes, muted or not.
//  - FFB Off/On (STD-004, UX-05): Off stops the effects the game has running and starts none; On starts them again.
//    The F6 panel's Stop FFB and F8 turn it Off. Saved in [FFB] Enabled of the mod's ini.
//  - Strength and effect-gain emulation (optional per game): the wheel is told gain 10000 with the game's own effect gain,
//    times Strength/50, applied to each effect's force (dbce_ffb_gain.hpp). For games that silence or scale effects with
//    DIEFFECT.dwGain, which the MOZA R12 ignores (Rush Rally 3, 2026-10-08). Saved in [FFB] Strength.
//  - The F6 panel's input rule: while the panel is open the game gets no keyboard, mouse or wheel buttons (axes pass).
//  - The panel's FFB page body (DrawPanelPage).
// The test switch (Options::mute) keeps every effect from starting whatever the setting says. While FFB is off or muted,
// the wheel's auto-centring spring is never switched on either (it is a force).
#pragma once
#include <windows.h>

namespace dbce { namespace ffb {

struct Options {
    const char *iniPath = nullptr;           // [FFB] Enabled (default 1), Strength (default 50)
    void (*log)(const char *line) = nullptr;
    bool mute = false;                       // test switch: no effect ever starts
    bool gainEmulation = false;              // apply effect gain and Strength in the proxy (see above)
    const char *wheelNote = nullptr;         // optional line under the FFB page's effect list (Advanced)
};

// Once, before DirectInput is used (not from DllMain).
void Init(const Options &options);

// After a successful DirectInput8Create (IDirectInput8A or W: the same table layout). Devices it creates are hooked.
void HookDirectInput(void *directInput8);
// For a proxy that wraps devices itself: hook one created device.
void HookDevice(void *device);

bool On();
void SetOn(bool on);
int Strength();
void SetStrength(int percent);
bool Muted();

// The F6 panel's FFB page (dbce_panel widgets): FFB Off/On, Strength (with gain emulation), status; Advanced lists the
// game's effects.
void DrawPanelPage(bool advanced);

}} // namespace dbce::ffb
