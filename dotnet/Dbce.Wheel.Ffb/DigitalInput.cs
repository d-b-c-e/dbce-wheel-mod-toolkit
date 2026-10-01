using System;
using System.Collections.Generic;
namespace Dbce.Wheel.Ffb
{
    /// <summary>Stable physical-button and exact eight-way POV binding indices.</summary>
    public static class DigitalInput
    {
        public const int PhysicalButtonCount = 128;
        public const int ButtonCount = 160;
        public static bool Valid(int button) => button >= 0 && button < ButtonCount;
        public static string Label(int button)
        {
            if (!Valid(button)) return "Invalid control";
            if (button < PhysicalButtonCount) return "Button " + (button + 1);
            string[] directions = { "Up", "Up-right", "Right", "Down-right", "Down", "Down-left", "Left", "Up-left" };
            return "HAT " + ((button - PhysicalButtonCount) / 8 + 1) + " " + directions[(button - PhysicalButtonCount) % 8];
        }
    }
    /// <summary>Each assigned action can consume a physical press once, independently.</summary>
    public sealed class DigitalInputEdges
    {
        private readonly HashSet<string> pending = new HashSet<string>();
        private readonly HashSet<string> consumed = new HashSet<string>();
        private static string Key(Guid guid, int button) => guid.ToString("D") + ":" + button;
        public void Observe(Guid guid, byte[] current, byte[] previous)
        {
            int count = Math.Min(DigitalInput.ButtonCount, Math.Min(current.Length, previous.Length));
            for (int i = 0; i < count; i++) {
                string key = Key(guid, i);
                if (current[i] == 0) {
                    pending.Remove(key);
                    consumed.RemoveWhere(x => x.StartsWith(key + ":", StringComparison.Ordinal));
                } else if (previous[i] == 0) {
                    pending.Add(key);
                    consumed.RemoveWhere(x => x.StartsWith(key + ":", StringComparison.Ordinal));
                }
            }
        }
        public bool Consume(Guid guid, int button, string action)
        {
            string key = Key(guid, button);
            return pending.Contains(key) && consumed.Add(key + ":" + action);
        }
        public void Disconnect(Guid guid)
        {
            string prefix = guid.ToString("D") + ":";
            pending.RemoveWhere(x => x.StartsWith(prefix, StringComparison.Ordinal));
            consumed.RemoveWhere(x => x.StartsWith(prefix, StringComparison.Ordinal));
        }
        public void Clear() { pending.Clear(); consumed.Clear(); }
    }
}
