import { readFile } from 'node:fs/promises';

// Read-only admission for the supported Sky operator. This never activates a
// window or sends input. Process-birth strings are compared without converting
// producer Int64 ticks to JavaScript numbers.
export function inspectUnityAdmission(c, expectedRequestId, now = Date.now()) {
  const f = c?.foreground;
  const client = f?.client ?? f?.clientSelection;
  const woden = c?.schema === 'woden.shared-launch-custody@1';
  const stamp = f?.updatedUtc ?? f?.foregroundCheckedUtc ?? client?.checkedUtc;
  const deadline = Date.parse(f?.deadlineUtc);
  const age = now - Date.parse(stamp);
  const checks = {
    schema: woden || c?.schema === 'dbce.desktop-client-click-admission@1',
    request: typeof expectedRequestId === 'string' && !!expectedRequestId && c?.requestId === expectedRequestId,
    launchAdopted: c?.launchAdopted === true,
    integrity: c?.game?.integrityMatchesSteam === true,
    window: !!f?.window,
    selectedClient: client?.state === 'selected' && client.class === 'UnityWndClass' && !!client.title,
    clientWindow: !!client && client.window === f?.window,
    process: !!c?.game?.pid && f?.processId === c.game.pid,
    birth: typeof c?.game?.startUtc === 'string' && f?.createdUtc === c.game.startUtc,
    executable: typeof f?.executablePath === 'string' && typeof c?.game?.executable === 'string' &&
      f.executablePath.toLowerCase() === c.game.executable.toLowerCase(),
    activationLimit: woden || f?.maximumDesktopClientClicks === 1,
    status: ['awaiting-desktop-client-click', 'observed'].includes(f?.status),
    deadline: Number.isFinite(deadline) && now < deadline,
    freshness: f?.status === 'observed' || (Number.isFinite(age) && age >= 0 && age <= 2000),
  };
  return {
    failedChecks: Object.keys(checks).filter(key => !checks[key]),
    checkedUtc: new Date(now).toISOString(),
    observationUtc: stamp ?? null,
    observationAgeMs: Number.isFinite(age) ? age : null,
    deadlineUtc: f?.deadlineUtc ?? null,
  };
}

function refusal(details, snapshotText) {
  const error = new Error(`Unity operator admission refused: ${JSON.stringify(details)}`);
  error.details = details;
  // Preserve the exact read that failed, not a later overwritten producer file.
  error.snapshotText = snapshotText;
  return error;
}

export async function readUnityOperatorAdmission(path, expectedRequestId, {
  read = () => readFile(path, 'utf8'),
  now = Date.now,
  wait = ms => new Promise(resolve => setTimeout(resolve, ms)),
  onPending = () => {},
} = {}) {
  let originalDeadline;
  let identity;
  for (;;) {
    const snapshotText = await read();
    const c = JSON.parse(snapshotText);
    const check = inspectUnityAdmission(c, expectedRequestId, now());
    const f = c?.foreground;
    const currentIdentity = JSON.stringify([c?.requestId, c?.game?.pid, c?.game?.startUtc,
      c?.game?.executable, f?.window]);
    if (originalDeadline !== undefined && f?.deadlineUtc !== originalDeadline)
      check.failedChecks.push('deadlineChanged');
    if (identity !== undefined && currentIdentity !== identity)
      check.failedChecks.push('identityChanged');
    if (!check.failedChecks.length) return c;
    // A stale read is temporary, not an activation attempt. Wait for the next
    // publication only within the original producer deadline. All other
    // failures stop immediately; no target substitution or timer extension.
    if (check.failedChecks.length !== 1 || check.failedChecks[0] !== 'freshness')
      throw refusal(check, snapshotText);
    originalDeadline ??= f.deadlineUtc;
    identity ??= currentIdentity;
    onPending(check);
    const remaining = Date.parse(originalDeadline) - now();
    if (remaining <= 0) throw refusal({ ...check, failedChecks: ['deadline', 'freshness'] }, snapshotText);
    await wait(Math.min(100, remaining));
  }
}
