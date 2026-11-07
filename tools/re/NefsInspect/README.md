# Read-only NeFS inspection

Lists an EGO archive and optionally extracts selected files into a fresh private
directory. Supports ordinary `.nefs` files and the `.dat` files whose headers
live in the executable. It never calls an archive writer or loads the game.
Extracted game content must stay outside source control.

Uses the MIT-licensed [EGO NeFS library](https://github.com/EgoEngineModding/ego.nefsedit),
tested at `163c463e6e0c5792fef1a20eca5e08d8fb2128b9`. Clone that source outside
this repository and check out the exact commit, then build with .NET 8 or newer:

```powershell
$nefs = '<private path>/ego.nefsedit'
git clone https://github.com/EgoEngineModding/ego.nefsedit.git $nefs
git -C $nefs checkout 163c463e6e0c5792fef1a20eca5e08d8fb2128b9
dotnet build tools/re/NefsInspect -c Release "-p:NefsSource=$nefs"

$tool = 'tools/re/NefsInspect/bin/Release/net8.0/NefsInspect.dll'
$game = 'D:/Program Files (x86)/Steam/steamapps/common/DiRT 4'
$evidence = '<new private evidence directory>'
dotnet $tool "$game/game.nefs" "$evidence/game-index"
dotnet $tool "$game/game.nefs" "$evidence/cameras" 'cars[\\/]generics[\\/].*cameras.*\.xml$'
dotnet $tool --headless "$game/dirt4.exe" "$game/game.dat" "$evidence/dat-index"
dotnet $tool --headless "$game/dirt4.exe" "$game/game_2.dat" "$evidence/dat2-index"
```

`index.json` records all item names, IDs and sizes, plus archive/header provenance.
`complete.json` is written after successful extraction and lists content hashes.
Missing completion means the attempt did not complete. Source length/time are
checked before/after reading; this is not a full archive hash comparison. The
intake script records the executable hash separately.

Output must be new and outside the archive directory. Extraction refuses path
escapes, duplicate output names, more than 512 files, files larger than 64 MiB,
or more than 256 MiB total. Select a narrower regex after a refusal and use a new
result directory. Duplicate names can be real archive entries: DiRT4 contains
two `audio/audio_banks_main.xml` items. The tool does not arbitrarily pick one.

October 7 qualification: DiRT4 build4002786 yielded 3,734 `game.nefs` items,
1,666 `game.dat` items and 550 `game_2.dat` items; selected camera/render XML and
three shader packs extracted with exact expected sizes. No game/archive edits,
install or launch. See `docs/research/2026-10-07-dirt-forza-intake.md` in the hub.
