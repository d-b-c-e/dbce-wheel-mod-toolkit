# Template: Unity (Mono) + Unity Mod Manager

Pick this when the game runs a **Mono** runtime, so Harmony patches behave and
reflection works normally. Check with `dbce-mod-toolkit`'s engine
fingerprinter; a `Managed` folder full of ordinary assemblies is the tell.

Vendor: `-Parts native,dotnet`

Working example: [art-of-sim-rally](https://github.com/d-b-c-e/art-of-sim-rally),
art of rally, Unity 2019.4.

Copy `CLAUDE.md` into the new repo and work through its TODOs. The hard part is
never the toolkit wiring; it is finding the car state and confirming what its
fields mean.
