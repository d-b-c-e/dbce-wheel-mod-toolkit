# Camera action contract compatibility

Append-only Core actions31..41: cameraForward, cameraBack, cameraUp, cameraDown, cameraLeft, cameraRight, cameraTiltDown, cameraTiltUp, cameraZoomIn, cameraZoomOut, cameraReset. All Digital. ZoomIn narrows and ZoomOut widens (STD-048). Legacy indexes0..30 retain their exact meaning.

Observer writes DomainVersion3/42 actions; reads2/31 with the absent camera tail neutral. Version2 cannot carry extended edge bits or device indexes. Recordings use their exact named channel set: only a wholly absent camera extension is legacy; any partial or unknown extension refuses. New recordings round-trip42 actions. The explicit legacy writer rejects live/nonzero extended actions; ABI2 capture uses its old31-action channel schema, so existing readers do not receive a new schema under the old facade.

The DLL facade remains ABI2,31 actions,296 bytes and the same exported names. It refuses camera IDs in profile/capability/inspection, marks extended Core slots unsupported, masks high edge bits, and copies only the original prefix. Managed/Unity libraries and consumer DLL pins are unchanged. Source consumers opt into42 by vendoring the reviewed Core/contract, independently of the DLL facade.

Fixtures were frozen from commit be4a46d9ac3add19836f104de90c63c3f74078e2. prepare_baseline.py extracts its exact old codec headers and the actual tick_impl ABI2 serialization fragment, then baseline_main.cpp generates three consecutive source frames as old named channels, observer2 records and296-byte facade records. This is a CPU serializer comparison against pinned pre-change production code, not a baseline DLL hardware run or gameplay replay. Provenance pins the original source and extracted fragment hashes plus every fixture hash. No recorded owner/game data is present.

Reproduce baseline fixtures (new destination required):

```powershell
python -I native/shim/tests/camera_actions/prepare_baseline.py --out build/camera-baseline-new
cmake -S build/camera-baseline-new -B build/camera-baseline-new/build -G "Visual Studio 17 2022" -A x64 -T ClangCL
cmake --build build/camera-baseline-new/build --config Release --parallel 2
build/camera-baseline-new/build/Release/camera_baseline.exe build/camera-baseline-new/fixtures
```

Focused checks (x64; repeat configure/build/ctest with Win32 for the retained ordinary-only facade):

```powershell
cmake -S native/shim/tests/camera_actions -B build/camera-actions-x64 -G "Visual Studio 17 2022" -A x64 -T ClangCL
cmake --build build/camera-actions-x64 --config Release --parallel 2
ctest --test-dir build/camera-actions-x64 -C Release -V --output-on-failure
```

Results2026-11-02: x64 compatibility830/Core192/ABI293 pass3/3; x86 Core192/ABI244 pass2/2. Actual Core resolves all eleven high-index button edges; original three frames preserve exact named channels and facade bytes; all44 partial field omissions per frame refuse; observer2 compatibility, observer3 round-trip and maximum transport density pass. Existing fake-COM ABI checks exercise the built DLL exports and ordinary/latched/capture lifecycle without opening real devices. An initial new-test range-loop copy warning under/WX was corrected; failed logs retained. Local logs/CPU lease receipts: build/camera-actions-validation. Branch awaits lili input-contract review before merge or consumer adoption. No hardware/game/GPU/backend/window initialization or owner install.
