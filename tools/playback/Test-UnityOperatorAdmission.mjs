import assert from 'node:assert/strict';
import { inspectUnityAdmission, readUnityOperatorAdmission } from './UnityOperatorAdmission.mjs';

const start = Date.parse('2026-10-16T07:15:37Z');
const request = '36ce1df29fe64c00a3d6d6aaeb86fd7a';
const iso = offset => new Date(start + offset).toISOString();
function fixture(offset = 0) {
  return {
    schema: 'woden.shared-launch-custody@1', requestId: request, launchAdopted: true,
    game: { pid: 123, startUtc: '2026-10-16T07:15:19.2438100Z', executable: 'D:\\game.exe', integrityMatchesSteam: true },
    foreground: {
      window: 456, processId: 123, createdUtc: '2026-10-16T07:15:19.2438100Z', executablePath: 'D:\\game.exe',
      status: 'awaiting-desktop-client-click', deadlineUtc: iso(1000), foregroundCheckedUtc: iso(offset),
      clientSelection: { state: 'selected', class: 'UnityWndClass', title: 'Game', window: 456 },
    },
  };
}
const stale = fixture(-3000);
assert.deepEqual(inspectUnityAdmission(fixture(), request, start).failedChecks, []);
assert.deepEqual(inspectUnityAdmission(stale, request, start).failedChecks, ['freshness']);
// A real slow publication must not force another game launch.
let tick = start, reads = 0, waits = 0;
let pendingSnapshot, pendingDetails, pendingCount = 0;
const resumed = await readUnityOperatorAdmission('', request, {
  read: async () => JSON.stringify(++reads < 3 ? stale : fixture(200)),
  now: () => tick, wait: async ms => { tick += ms; waits++; },
  onPending: async (details, text) => { pendingSnapshot = text; pendingDetails = details; pendingCount++; },
});
assert.equal(resumed.game.pid, 123);
assert.equal(reads, 3); assert.equal(waits, 2);
assert.equal(pendingCount, 1);
assert.equal(pendingSnapshot, JSON.stringify(stale));
assert.equal(pendingDetails.checkedUtc, iso(0));
// A frozen producer expires at its existing deadline, preserving the failed read.
tick = start;
await assert.rejects(readUnityOperatorAdmission('', request, {
  read: async () => JSON.stringify(stale), now: () => tick, wait: async ms => { tick += ms; },
}), error => error.details.failedChecks.includes('deadline') && error.snapshotText === JSON.stringify(stale) &&
  error.firstFailedDetails.failedChecks.join() === 'freshness' && error.firstFailedDetails.checkedUtc === iso(0));
assert.equal(tick, start + 1000);
for (const mutate of [
  c => { c.game.pid = c.foreground.processId = 789; },
  c => { c.foreground.deadlineUtc = iso(5000); },
]) {
  reads = 0; tick = start;
  const changed = fixture(100); mutate(changed);
  await assert.rejects(readUnityOperatorAdmission('', request, {
    read: async () => JSON.stringify(++reads === 1 ? stale : changed),
    now: () => tick, wait: async ms => { tick += ms; },
  }), error => error.details.failedChecks.some(x => x.endsWith('Changed')));
}
for (const [field, mutate] of [
  ['request', c => { c.requestId = 'different'; }],
  ['birth', c => { c.foreground.createdUtc = '2026-10-16T07:15:19.2438101Z'; }],
  ['executable', c => { c.foreground.executablePath = 'D:\\other.exe'; }],
  ['clientWindow', c => { c.foreground.clientSelection.window = 999; }],
  ['status', c => { c.foreground.status = 'unmet'; }],
]) {
  const wrong = fixture(); mutate(wrong);
  await assert.rejects(readUnityOperatorAdmission('', request, {
    read: async () => JSON.stringify(wrong), now: () => start,
    wait: async () => assert.fail('A permanent refusal must not wait'),
  }), error => error.details.failedChecks.includes(field));
}
await assert.rejects(readUnityOperatorAdmission('', request, {
  read: async () => '{malformed', now: () => start,
}), error => error.details.failedChecks.join() === 'parse' && error.snapshotText === '{malformed');
await assert.rejects(readUnityOperatorAdmission('', request, {
  read: async () => { throw new Error('fixture unreadable'); }, now: () => start,
}), error => error.details.failedChecks.join() === 'read' && error.snapshotText === undefined);
console.log('PASS: freshness recovery, original expiry, identity/deadline drift, request/birth/path/window/status refusals. No OS calls.');
