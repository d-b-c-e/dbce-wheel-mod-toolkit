# Template: native `dinput8.dll` proxy

Pick this when there is no managed runtime to hook: a native game you sit
beside. Windows loads your `dinput8.dll` instead of the system one, you forward
every call to the real thing, and in between you hook the game.

Vendor: `-Parts include,native-<arch>,profiles` - and match the architecture to
the game, not to your machine. OutRun 2006 is 32-bit.

Working examples: OutRun2006Tweaks-FFB (x86, produces force),
milestone-wheel-tools (x64, taps the game's own force).

Three files must reach the game folder: your `dinput8.dll`, `WheelFfb.dll`, and
`force-profiles.ini`. Copy all three as a post-build step; a deploy that misses
one loses force feedback silently.
