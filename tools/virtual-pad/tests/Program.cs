int checks = 0;
foreach (var item in new[] { ("200", 200), ("ms=200", 200), ("MS=200", 200), ("0", 0), ("ms=0", 0), ("60000", 60000) })
{
    if (CommandDuration.Read(new[] { "press", "A", item.Item1 }, 2, 150) != item.Item2) throw new Exception(item.Item1);
    checks++;
}
if (CommandDuration.Read(new[] { "press", "A" }, 2, 150) != 150) throw new Exception("missing default");
checks++;
foreach (var value in new[] { "ms=", "oops", "-1", "ms=-1", "1.5", "60001", "2147483648", "+1", "ms=200x" })
{
    try { CommandDuration.Read(new[] { "press", "A", value }, 2, 150); }
    catch (FormatException) { checks++; continue; }
    throw new Exception("accepted invalid duration " + value);
}
Console.WriteLine($"PASS {checks} duration checks; no controller library or device access");
