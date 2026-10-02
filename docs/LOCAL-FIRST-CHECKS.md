# Local-first checks and release qualification

[dbce-wheel-mod-toolkit, 2026-10-02] Anthony's Actions-minute cap is a reason to
build and verify locally, then upload qualified assets. This candidate changes
the sole hosted workflow to `workflow_dispatch` only. Job contents remain intact;
no dispatch, billing, permissions, secrets, repository privacy or device settings
are changed. Manual hosted runs are optional and still consume allowance.

## Local entry point

Use an isolated clean checkout and PowerShell 7.2 or newer. The runner requires
Git, the repository's compatible .NET SDK, MSVC/Windows SDK for Windows checks,
and GCC plus Python 3.9+ for the portable lane. Explicit tool paths are supported:

```powershell
./tools/local/Invoke-LocalChecks.ps1 -PlanPath ./tools/local/wheel-windows.json
./tools/local/Invoke-LocalChecks.ps1 -PlanPath ./tools/local/wheel-portable.json -Python <python-path> -Gxx <g++-path>
```

By default each run creates a new ignored `build/local-checks/<unique-id>`
directory. `-OutputDirectory` selects a new directory; an existing one is refused.
The runner starts each native command or script as a separate process, records
stdout/stderr, exit code, executable SHA-256 and elapsed time, and stops on the
first failure without retrying. Later steps remain `not-run`. A 15-minute step
timeout kills only that launched process tree and records failure. Failed tool
resolution, script exceptions and provenance refusal also produce a failed
receipt when the new output directory can be created. No execution-policy
override or automatic elevation is requested. A denied script is a blocker;
do not retry it via another host, policy override or alternate implementation.

`receipt.json` binds the plan and runner hashes, before/after Git commit/tree,
clean status, OS/process architecture, PowerShell version, per-step executable
hashes, logs and their hashes, and requested built-artifact hashes. Dirty or
changed source fails qualification. Checks never declare a release qualified:
`releaseQualified` stays false until separate packaging/asset gates are complete.
Keep receipts/logs private unless their paths and contents have been reviewed.

| Hosted check | Local plan coverage |
|---|---|
| Managed solution tests | Windows `managed-tests`; selected SDK captured by `dotnet-sdk-info` |
| MSVC model conformance | Windows builder also runs events, signals and recovery |
| Vendoring/status/status-exit/binding fixtures | Separate Windows PowerShell 7 processes |
| x64 export/header and x86 builds | Windows native builds; both DLL hashes recorded |
| Burst, lock publication, worker drain and pre-init watchdog | Both Windows architectures |
| Actual-DLL empty lifecycle/restart/unload | Both Windows architectures; no device APIs |
| GCC model conformance/events/signals/recovery | Portable compiler build and test steps |
| ROM-free replay fixtures | Portable Python unittest discovery |

Run the portable plan on a qualified local Linux host to match Linux runtime
coverage. Running GCC on Windows is useful compiler evidence but does not prove
Linux behavior; the receipt records the actual platform. A skipped unittest stays
visible in its log and must be assessed rather than described as executed.
No fixture opens a physical device or starts a game. No app, GPU, registry tune or
consumer settings are applied by these plans. The consumer fixture's old
`-ExecutionPolicy Bypass` subprocess flags are removed; its assertions are intact.

## Reusable pattern for the four repositories

The runner accepts a repository-owned JSON plan with schema
`dbce.local-check-plan`, version 1, a lane and nonempty unique `steps`.
Each step has an `id`, executable `program` and string-array `arguments`; commands
are executed directly without a pipeline or shell reinterpretation. Batch steps
explicitly invoke `cmd.exe`. Tokens are `{root}`, `{output}`, `{pwsh}`, `{dotnet}`,
`{python}`, `{gxx}`, `{git}` and `{cmd}`. `{root}` is `-RepositoryRoot`; `{output}`
is the fresh run directory. Optional `artifacts` are relative root paths to hash.
Copy/adapt the runner and a small reviewed plan in each repository before use;
do not assume another repository's plan, SDK or packaging checks apply unchanged.

| Repository | Relevant plan additions and packaging gates |
|---|---|
| Wheel toolkit | The two included plans; exact native ABI/version and managed/component versions; licenses/notices; no private captures |
| Triple-screen toolkit | Its geometry project `tests/Dbce.TripleScreen.Core.Tests/Dbce.TripleScreen.Core.Tests.csproj`; contract/version checks; source/package license and identity checks |
| Triple-screen optimizer | Pinned .NET 10 solution build/test; `tools/tests/Test-VerifySetup.ps1`, `Test-PortablePackage.ps1 -ResultsPath <fresh-absolute-path>`, and `Test-FixtureExit.ps1`; verify the exact toolkit gitlink and qualified portable archive |
| VR optimizer | Pinned .NET 10 core tests `src/VrGameOptimizer.Core.Tests/VrGameOptimizer.Core.Tests.csproj`, app build, setup/package verification and qualified portable archive; no UI launch, tuning or deployment |

These are adaptation instructions, not edits or execution in the other three
repositories. Preserve their owner work, locks, instructions and artifact gates.
WPF/UI builds do not authorize UI execution or automatic deployment. Add any
newly reviewed recovery fixture modes to its eventual integrated local plan;
the present canonical-base plan cannot claim checks for absent source changes.
Keep plan arguments and SDK/compiler choices in version control and record
toolchain versions needed for asset provenance, not just a compiler launcher hash.

## Publication without an intermediate hosted run

GitHub documents the `push` event as using the tip commit pushed to the ref,
including workflows outside the default branch. A manual workflow is invoked
through `workflow_dispatch`, whose workflow must exist on the default branch.
Sources: [workflow events](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows),
[manual invocation](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow).

1. Independently review this complete candidate, then refresh refs and inspect
   every actual workflow trigger. The inspected canonical base has only
   `.github/workflows/checks.yml`. Stop if another automatic workflow or owner
   change alters the publication assumptions. Do not change billing or security.
2. Publish only a final reviewed ref whose entire workflow set is manual-only.
   Push that final tip directly; never create an intermediate remote ref at its
   automatic-trigger base, nor push old approved tips first. Do not open a PR or
   dispatch a workflow during the transition. Review/reconcile separately before
   any default-branch promotion; no protection rule change or bypass is implied.
3. Preserve recovery `995e4eb032900d7981ba87f1487bb476184eb6a3` and diagnostic
   `1c30d397276aeee805e6517edb0f233a170a325d`, their exact trees and complete bundles.
   Their current trees still have automatic triggers. Prepare distinct descendants
   carrying the reviewed manual-only/local-check changes, qualify them locally
   (including their new tests) and independently review each resulting tree.
   Do not silently merge, rewrite or publish either frozen approved tip.
4. Build assets locally only from the reconciled/reviewed source with qualified
   relevant lane receipts. Verify asset source revision, ABI/component versions,
   completeness, licenses/notices, privacy and packaging assertions; hash exact
   upload bytes and retain the source/test/package receipts. Reinspect tag/release
   workflow triggers before any tag or upload. Upload only qualified, approved
   assets to the exact private repository. No hosted dispatch is required.

No release asset is built or uploaded by this check runner. The historical wheel
packager still contains PowerShell 5.1/Bypass invocations and default versioning;
it has not been executed or qualified in this milestone. It needs separate
review before use in this policy-respecting local workflow. Do not relabel the
canonical native-0.6.0/41-export source as the native-0.8.0/46-export v0.15.0 release.
Installed adoption, physical acceptance and consumer deployment remain separate.
This plan supersedes the earlier allowance-dependent publication ordering while
preserving its frozen source identities. No publication has occurred here.
