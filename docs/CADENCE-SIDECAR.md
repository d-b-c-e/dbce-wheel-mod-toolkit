# Pure cadence sidecar source candidate

`dotnet/Dbce.Wheel.Ffb.Cadence` compiles the existing reviewed `CadenceImpactMixer.cs` and `ImpactMixer.cs` directly into a separate netstandard2.0 assembly. Neither algorithm is copied or edited. This permits a consumer retaining an older `Dbce.Wheel.Ffb` device assembly to reference the unique `CadenceImpactMixer` type without replacing device, input or telemetry binaries.

Both assemblies expose `ImpactMixer`; consumers must not refer to that duplicated type without an assembly alias. Woden's candidate refers only to `CadenceImpactMixer`, which resolves its envelope helper inside the sidecar. The sidecar has no native imports, project/package dependency or collision detector. It is an offline source candidate, excluded from existing release packaging, with no ABI or profile change. Review independently before publication or consumer packaging.

Source baseline is `4ef65c160146a847e9c88a51dda1dfc746db141d`. Woden's isolated actual-controller fixture compiles and loads this sidecar alongside its pinned legacy FFB assembly, proving the assembly seam in the test runtime. Its optional production plugin build also compiles against this seam; that is not a Unity or physical acceptance test.
