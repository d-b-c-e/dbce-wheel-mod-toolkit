# Template: Unity (IL2CPP) + BepInEx 6

Pick this when the game was compiled to **IL2CPP** - a `GameAssembly.dll` beside
the exe is the tell. Most Mono-era advice is wrong here, which is why this is
its own template.

Vendor: `-Parts native,dotnet`

Working example: iracing-arcade-wheel, iRacing Arcade, Unity 6.

Budget most of the time for finding the car state and confirming which hooks
actually fire. IL2CPP inlines, so a Harmony patch can attach to a method the
game never calls - and a hook that never fires looks exactly like one that
fires and does nothing.
