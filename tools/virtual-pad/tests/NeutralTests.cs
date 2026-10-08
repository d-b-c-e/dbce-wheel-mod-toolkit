int checks = 0;
void Check(bool condition, string why) { if (!condition) throw new Exception(why); checks++; }
var neutral = new PadReading(true, 0, 0, 0, 0, 0, 0, 0);
foreach (var bad in new[] {
    neutral with { Connected = false }, neutral with { Buttons = 1 },
    neutral with { LeftTrigger = 1 }, neutral with { RightTrigger = 1 },
    neutral with { LeftX = 1 }, neutral with { LeftY = 1 },
    neutral with { RightX = 1 }, neutral with { RightY = 1 } })
    Check(!bad.Neutral, "non-neutral field accepted");
Check(neutral.Neutral, "neutral refused");
long time = 0; int primes = 0, reads = 0;
int result = NeutralStartup.Ensure(() => 2, _ => { reads++; return neutral; },
    () => primes++, ms => time += ms, () => time);
Check(result == 2 && reads == 3 && primes == 0, "index and three readbacks");
time = 0; primes = 0; reads = 0;
result = NeutralStartup.Ensure(() => 0,
    _ => { reads++; return primes == 0 ? neutral with { LeftX = -3356, LeftY = -1869, RightX = -3255, RightY = -848 } : neutral; },
    () => primes++, ms => time += ms, () => time);
Check(result == 0 && primes == 1 && reads == 4, "actual startup blob requires changed report and neutral readback");
time = 0; primes = 0;
result = NeutralStartup.Ensure(() => primes == 0 ? null : 1, _ => neutral,
    () => primes++, ms => time += ms, () => time);
Check(result == 1 && primes == 1, "late index notification");
time = 0; primes = 0; bool refused = false;
try { NeutralStartup.Ensure(() => 0, _ => neutral with { RightTrigger = 1 },
    () => primes++, ms => time += ms, () => time); }
catch (InvalidOperationException) { refused = true; }
Check(refused && time == 2000 && primes == 100, "permanent non-neutral state must time out");
time = 0; refused = false;
try { NeutralStartup.Ensure(() => 4, _ => neutral, () => {}, ms => time += ms, () => time); }
catch (InvalidOperationException) { refused = true; }
Check(refused && time == 0, "invalid index refused");
time = 0; reads = 0; primes = 0;
NeutralStartup.Ensure(() => 0, _ => ++reads == 2 ? neutral with { Buttons = 1 } : neutral,
    () => primes++, ms => time += ms, () => time);
Check(reads == 5 && primes == 1, "interrupted neutral sequence restarts qualification");
Console.WriteLine($"PASS {checks} startup/readback checks; no controller or hardware access");
