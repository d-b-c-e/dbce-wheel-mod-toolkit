# Experimental external-session constant FFB

Local candidate **0.16.0-external.1**, native **0.10.0 / 55 exports**. Based on
preserved POV source `397a5f22af38b2459f18ffa2e1d685ee838cce8f`, on branch
`codex/external-session-ffb`. This is an offline experimental implementation,
not an official release, deployment, game integration or hardware acceptance.
The immutable built-source receipt is `dist/EXTERNAL-HANDOFF.json`; final
archive/payload hashes belong there, separately from this source document.

## Boundary and ownership

Use a dedicated device-owner process with a real top-level HWND created by that
process. Enter on its window thread, before any concurrent toolkit use and with
no open readers, effects or legacy exit/watchdog guard. An existing idle shared
DirectInput instance is allowed. The owner handles every DirectInput read and
effect; UI/virtual-pad processes communicate over consumer-owned bounded IPC.

`ExternalFfbEnter` installs an owner-window guard and a 5 ms watchdog, verifies
own-process/top-level/thread identity, and marks the HWND with a property to
detect reuse. It pins the DLL for the life of the dedicated process. Subsequent
legacy Init cannot replace that HWND. Successful opt-in is sticky across Close,
Free, Panic, driver failures and failed Opens. If guard startup fails after the
latch, the process remains unable to open external output; restart that owner.
There is no supported switch back to legacy continuous FFB in this process.

Every legacy create, nonzero update/play, start, reacquire, selection change and
autocentre change is blocked. Legacy cancellation/zero/release functions revoke
external authority; Panic and Free request Close. They do not offer the new
asynchronous status contract: the external owner must use Close/GetStatus before
destroying its HWND. Consumers that never Enter retain the old force behavior,
including the same-process foreground guards; they cannot use older calls to
bypass this external contract.

## ABI and results

The authoritative declaration is
[`external_ffb.h`](../native/wheelffb/include/external_ffb.h), included by
`wheelffb.h`. All eight additive exports are Cdecl and use explicit integer
results, with fixed-width structs packed to 8 on both x86 and x64.

```c
int ExternalFfbEnter(HWND ownerWindow);
int ExternalFfbOpen(const ExternalFfbConfigV1*, const wchar_t* targetImagePath,
    unsigned int pathChars, unsigned long long* session);
int ExternalFfbArm(unsigned long long session, ExternalFfbArmTicketV1* ticket);
int ExternalFfbRenewClient(unsigned long long session, const ExternalFfbClientFrameV1*);
int ExternalFfbSubmitConstant(unsigned long long session, const ExternalFfbForceFrameV1*);
int ExternalFfbDisarm(unsigned long long session, unsigned int reason);
int ExternalFfbClose(unsigned long long session);
int ExternalFfbGetStatus(unsigned long long session, ExternalFfbStatusV1* status);
```

| Struct | Bytes | Selected offsets |
|---|---:|---|
| Process | 24 | handle 0, creation FILETIME 8, PID 16, reserved 20 |
| Config | 112 | target 8, client 32, producer 56, GUID 80, limits 96 |
| Arm ticket | 56 | generation 8, nonce 16/24, issued QPC 32, expiry 40, frequency 48 |
| Client frame | 40 | generation 8, sequence 16, capture QPC 24, frequency 32 |
| Force frame | 64 | generation 8, nonce 16/24, sequence 32, capture 40, frequency 48, magnitude 56, reserved 60 |
| Status | 104 | session 8, generation 16, state 24, reason 28, cancel 32, flags 36, source/client sequence 40/48, deadlines 56/64, wheel/start QPC 72/80, HRESULT 88, reserved 92, frequency 96 |

Input pointers must address their declared structs/buffers; caller memory must
remain valid for the call and cannot overlap output. Config/force/client structs
are copied before blocking driver calls. Config, ticket and status use exact
`cbSize` and `version=1`; frames use exact `cbSize`. All reserved fields must be
zero and all flag bits known. Native cannot protect against arbitrary code or
invalid pointers in its own process. Path extent is 1..32767 UTF-16 characters,
excluding the terminator, with no embedded NUL.

| Result | Value | Meaning |
|---|---:|---|
| OK | 1 | Operation accepted; not a physical-output claim |
| WAITING | 2 | Arm issued or fresh source accepted before client eligibility; no pulse |
| APPLIED | 3 | One finite Start returned successfully and authority remained valid on return |
| CANCELED | 4 | Cancellation API completed, or a zero source sample stopped its effect |
| CLOSED | 5 | Resources closed and cleanup APIs succeeded |
| PENDING | 6 | Revocation already recorded; driver/cleanup remains in flight |
| INVALID | -1 | Bad argument, layout, flags, bounds or process identity/path |
| WRONG_MODE / WRONG_SESSION | -2 / -3 | Missing opt-in or stale/unknown session |
| STALE | -4 | Inactive generation, wrong challenge, non-increasing sequence/capture, bad age/frequency |
| BUSY | -5 | Device transaction/reentry in progress, or ownership transition incomplete |
| FAULT | -6 | Driver/setup/cleanup failure; inspect status |
| INELIGIBLE | -7 | Input, focus or other live authority failed |
| UNAVAILABLE | -8 | Optional API absent or guard/random-source startup unavailable |

`Status.state` distinguishes Prepared, Arming, Armed, Revoked, Faulted and
SessionClosed. `reason` records the concrete eligibility/cancellation fault.
`cancellation` is None, Requested, ApiDone or ApiFailed. DriverBusy and
ClosePending distinguish pending work; **PhysicalUnverified is always set**.
`driverHResult` retains the most recent driver failure, including restoration
failure; a successful later Stop does not erase its original fault. A closed
session with ApiFailed returns FAULT again on repeated Close. GetStatus never
enters the device gate, but does observe expiry/identity and may revoke policy.
Session 0 may query OwnerOnly status before Open. Positive values are not all
equivalent: never treat Pending/Waiting as output or completed cleanup.

## Pinning and source/client authority

Open receives live local process handles for target game, UI/client and real
producer. Native rejects x86 high handle bits before conversion, rejects pseudo
handles, duplicates each with query/synchronize access, and verifies live PID
and exact creation FILETIME. It compares the target's queried full image path
case-insensitively to the supplied path. The target must be outside the owner
process. Handles stay pinned until cleanup; foreground PID must match that
still-live target, so PID reuse does not follow a replacement process.
The caller retains ownership of its original handles and may close them after
Open returns. This is an identity boundary, not a hostile-caller security sandbox.

The consumer additionally verifies supported game executable build/hash,
authenticates both IPC peers against its pinned identities, enforces epochs,
uses bounded latest-only mailboxes, and discards old queued packets. Native
cannot observe whether a car-state assertion is true or whether a helper copied
an old frame. A future real producer is required; probe logs, launcher timers,
invented car physics and restamped cached samples are not valid sources.

Session configuration fixes exact wheel GUID and magnitude ceiling (0..10000),
source age (10..150 ms), client age (10..250 ms), and pulse maximum (1..50 ms,
no greater than source age). There is one idle constant effect on X; no periodic,
condition or continuous effect surface, speculative axis fallback or reacquire
retry. Magnitude 0 may open for diagnostics but cannot Arm nonzero output.

Arm requires a successful shared-wheel sample less than 50 ms old, live owner
and processes, actual target foreground, completed cancellation, and no driver
fault. It issues a new generation plus a random 128-bit nonce and bounded initial
challenge expiry. **Arm itself refreshes neither source nor client authority.**
The producer must receive this ticket and echo it at its actual subsequent
car-state capture. A helper echo on cached game data proves nothing.

Each frame uses the same boot's QPC frequency and original capture timestamp;
the accepted ordering ambiguity is at most one QPC tick into the future or
before the ticket issue. No milliseconds of future grace or receipt-time
restamping are permitted. Deadline intervals are overflow checked and half-open:
`now >= deadline` expires. Capture and sequence must both strictly increase.
Standby/hibernate elapsed time expires prior leases; return requires a new Arm.

Initial source echo must arrive before the ticket expiry. Once a timely echo is
accepted, that initial deadline no longer applies, even if client eligibility
has not yet arrived. Missing initial client expires at issued QPC + client age.
Thereafter each actual source/client capture owns its own bounded deadline.
The adapter observes **old deadlines before any replacement sample**, so a
stalled loop cannot revive an expired generation by sending a fresh packet.
Client renewal never starts or replays output; source-first startup needs another
fresh source after client becomes eligible. Every accepted source permits at
most one pulse. A Busy frame is not queued and must not be retried as a pulse;
send the next genuine capture.

Source attestations require local player, driving, known supported mode and
all required inputs valid. Client requires FFB opted in and required input
health. These assertions come from the consumer/producer; native independently
checks process/window/foreground/age/identity/sequence, magnitude, and its own
required wheel read. A missing required input neutralizes virtual output and
revokes FFB. Re-arm is an explicit API handshake after fresh verified eligibility,
not a new user permission dialog. Use per-action/pedal release masks for input
rearm and valid steering; never require every held switch or gear to release.

## Device transitions, cancellation and limits

One shared DirectInput instance is retained. Open uses the exact GUID, validates
the returned device identity/capabilities, rejects known virtual devices, uses
the validated own HWND, reads the previous autocentre setting, disables it while
unacquired, acquires, stops/disables actuators and creates one idle finite effect.
An unreadable/unknown autocentre baseline fails before changing that setting.
Readers must be closed before Open. Explicitly enumerate before opening input
readers in external mode; automatic nested enumeration is not used there.

After Open, reopen saved GUIDs using the coherent POV API. The wheel slot shares
the FFB handle. Successful reads update health using **read-start QPC**, and a
read taking 50 ms or longer is ineligible. Failed or wrong-identity wheel reads
revoke; they never reacquire the FFB device. Read validity still gates virtual
output: zero-filled axes on failure can mean pressed pedals. Other required
device health remains the consumer's responsibility. Optional readers can keep
working with FFB Off.

All raw input open/read/close/enumeration/metadata paths serialize with output
and cleanup. A logical in-flight guard also rejects same-thread reentry from a
driver that pumps HWND messages: recursive CRITICAL_SECTION acquisition alone
would permit releasing an effect inside its own Start call. Reentrant input
closes are deferred; Close/Disarm immediately revoke policy and report Pending.
Independent policy revocation never waits for the device lock. Watchdog only
tries that lock for COM cancellation, avoiding concurrent driver destruction.

Every potentially blocking setup/output step is followed by a fresh authority
check. Submit stops the old effect, sets signed magnitude with absolute direction,
finite duration, no trigger and zero start delay; reads back the duration/trigger/
delay; enables actuators; checks authority and remaining lease again; then starts
one iteration. A pulse longer than the remaining lease is dropped, not retried.
Authority is rechecked after Start too. Cancellation stops every effect family,
then issues STOPALL and ACTUATORSOFF. No stale recreation or reacquire is attempted.

Orderly Close and failed-Open unwind release the effect, Unacquire, restore the
**exact captured** autocentre baseline last, and release the device. If Unacquire
fails, no unsafe acquired-device property write is attempted; report ApiFailed.
Restored OEM autocentre may exert torque. Ordinary revocation keeps actuators
disabled while retaining the acquired wheel and requires a new Arm; driver
fault requires Close/Open. Panic/Free/owner close request full cleanup. Closing
or changing FFB invalidates shared wheel slots: neutralize output, serialize the
transition, close/reopen affected readers by saved GUID, and resume only after
fresh successful samples. An FFB Off toggle must not lose driving input by
keeping a slot tied to a released force handle.

Keep the owner process and HWND alive until cleanup status is final. Native
defers WM_CLOSE while cleanup is pending; keep pumping/status-polling and repost
that close after cleanup completes. This does not delay forced OS termination.
Native
cannot prevent a caller's immediate hard kill/window destruction. It cannot
interrupt a blocked driver call, guarantee watchdog scheduling, or guarantee
that a driver honors finite duration, Stop or actuator commands. A Start may
begin inside a blocking driver call after policy has already been revoked.
Finite duration bounds the requested effect once the driver starts it, subject
to driver behavior; **there is no hard wall-clock stop guarantee** through a
driver stall or owner crash. Hardware acceptance must verify real duration,
focus/disconnect/process-loss/close behavior and OEM baseline restoration with
an attended test. The current fake tests do not establish any of that.

## Managed binding and verification

`WheelFfbNative.SupportsExternalSessions` requires all eight optional exports.
Use EnterExternalSession, OpenExternalSession, ArmExternalSession,
RenewExternalClient, SubmitExternalConstant, DisarmExternalSession,
CloseExternalSession and GetExternalStatus. The wrapper sets v1 struct headers,
preserves explicit flags/reserved fields/results, and never calls legacy Init or
continuous output as fallback. `Ready` remains the legacy API's readiness flag;
external eligibility comes from the explicit status. Module binding remains
process-lifetime; Shutdown is not a DLL reload or completed external Close.

Offline gates include x64/x86 production-path fake COM/platform tests, source-
first/client-first challenge ordering, exact expiry/stalled-loop replacement,
invalid layouts/age/identity, one-pulse behavior, same-thread and cross-thread
blocked-driver/reader close, coercion/failure before Start, all legacy families,
autocentre restoration and input-only reopening. Managed fixtures verify all
struct sizes/offsets and every new call on x64 CoreCLR and x86 .NET Framework,
partial capability rejection, and existing native 0.5/0.6.1/0.7/0.8/0.9 loading.
Existing POV, burst/constant/logging, model/packet/proxy, sync and managed suites
remain regression gates. `package.ps1 -Offline` skips real device discovery.
Smoke builds cover C header/loading and external negative opt-in calls; **smoke
execution is not authorized or performed**. No game, device acquisition,
enumeration, shown window, virtual pad, input injection or real torque is used.

The pre-existing all-headers packaging omission for `impact_mixer.h` remains
outside scope. This candidate packages matching `wheelffb.h` and `external_ffb.h`;
the native+dotnet payload is independently hash checked against its clean source.

Sources: [DirectInput cooperative-level requirements](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417921(v=vs.85)),
[QPC ordering and suspend behavior](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps),
[process creation identity](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes),
[finite effects](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417540(v=vs.85)).
[dbce-wheel-mod-toolkit / Sonic coordination, 2026-09-28]
