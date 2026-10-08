using System.Diagnostics;
using System.Runtime.InteropServices;

internal readonly record struct PadReading(bool Connected, ushort Buttons, byte LeftTrigger,
    byte RightTrigger, short LeftX, short LeftY, short RightX, short RightY)
{
    internal bool Neutral => Connected && Buttons == 0 && LeftTrigger == 0 && RightTrigger == 0 &&
        LeftX == 0 && LeftY == 0 && RightX == 0 && RightY == 0;
}

internal static class NeutralStartup
{
    // ViGEmBus initially exposes a USB blob with small nonzero stick values,
    // while its duplicate-report cache is zero. A zero-only submit may be
    // discarded. One signed integer unit on LX changes the report, then the
    // full neutral report replaces it. Never press a button or trigger here.
    internal static int Ensure(Func<int?> index, Func<int, PadReading> read,
        Action primeAndNeutral, Action<int> wait, Func<long> milliseconds)
    {
        long start = milliseconds();
        int consecutive = 0, previousIndex = -1;
        while (milliseconds() - start < 2000)
        {
            int? slot = index();
            if (slot.HasValue && (slot < 0 || slot > 3))
                throw new InvalidOperationException("Virtual controller returned an invalid XInput index");
            if (slot.HasValue && read(slot.Value).Neutral)
            {
                consecutive = previousIndex == slot ? consecutive + 1 : 1;
                previousIndex = slot.Value;
                if (consecutive == 3) return slot.Value;
            }
            else
            {
                consecutive = 0; previousIndex = -1;
                primeAndNeutral();
            }
            wait(20);
        }
        throw new InvalidOperationException("Virtual controller did not report neutral within two seconds");
    }
}

internal static class XInputReadback
{
    [StructLayout(LayoutKind.Sequential)]
    private struct State
    {
        public uint Packet;
        public ushort Buttons;
        public byte LeftTrigger, RightTrigger;
        public short LeftX, LeftY, RightX, RightY;
    }
    [DllImport("xinput1_4.dll", ExactSpelling = true)]
    [DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
    private static extern uint XInputGetState(uint index, out State state);

    internal static PadReading Read(int index)
    {
        uint result = XInputGetState((uint)index, out var s);
        return new PadReading(result == 0, s.Buttons, s.LeftTrigger, s.RightTrigger,
            s.LeftX, s.LeftY, s.RightX, s.RightY);
    }
}
