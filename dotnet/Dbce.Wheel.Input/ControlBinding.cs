// STD-033 schema 1. Vendorable source; no devices, OS input, file I/O or force calls.
// Run the native/controls/controls-vectors.txt cases in every consuming implementation.
#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Dbce.Wheel.Input
{
    public enum ControlKind { Axis, Button, Hat }
    public enum ControlShape { Unknown, Centred, Pedal, Digital }

    public sealed class ControlBinding
    {
        public ControlKind Kind { get; private set; }
        public int Index { get; private set; }
        public int Angle { get; private set; } = -1;
        public Guid Device { get; private set; }
        public Guid Product { get; private set; }
        public int Minimum { get; private set; }
        public int Maximum { get; private set; }
        public int Rest { get; private set; }
        public int Travel { get; private set; }
        public bool Inverted { get; private set; }
        public bool Calibrated { get; private set; }
        public string Name { get; private set; } = "";
        private ControlBinding() { }

        public static ControlShape ShapeOf(string action)
        {
            switch (action)
            {
                case "steer": return ControlShape.Centred;
                case "throttle": case "brake": case "clutch": case "handbrake": return ControlShape.Pedal;
                case "shiftUp": case "shiftDown": case "neutral": case "reverse":
                case "confirm": case "back": case "start": case "select":
                case "navUp": case "navDown": case "navLeft": case "navRight":
                case "camera": case "lookBack": case "reset": case "horn": case "modMenu": case "muteFfb":
                case "gear1": case "gear2": case "gear3": case "gear4": case "gear5": case "gear6": case "gear7": case "gear8":
                    return ControlShape.Digital;
                default: return ControlShape.Unknown;
            }
        }
        static bool Integer(string value, out int number)
        {
            number = 0;
            if (value.Length == 0 || value.Length > 11) return false;
            int start = value[0] == '-' ? 1 : 0;
            if (start == value.Length) return false;
            for (int i = start; i < value.Length; i++) if (value[i] < '0' || value[i] > '9') return false;
            return int.TryParse(value, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out number);
        }
        static bool GuidValue(string value, out Guid guid)
        {
            guid = Guid.Empty;
            return value.Length == 38 && Guid.TryParseExact(value, "B", out guid);
        }

        public static bool TryParse(string text, out ControlBinding? binding, out string reason)
        {
            binding = null;
            var b = new ControlBinding();
            var tokens = new List<string>();
            reason = "bad-kind";
            if (text == null) return false;
            // A binding is one line. In particular, a device name cannot introduce another INI key.
            for (int p = 0; p < text.Length; p++)
                if (char.IsControl(text[p]) && text[p] != '\t') { reason = "bad-name"; return false; }
            for (int i = 0; i < text.Length;)
            {
                while (i < text.Length && (text[i] == ' ' || text[i] == '\t')) i++;
                if (i >= text.Length) break;
                int j = i;
                if (text.IndexOf("name=\"", i, StringComparison.Ordinal) == i)
                {
                    j = i + 6;
                    var name = new StringBuilder();
                    bool closed = false;
                    while (j < text.Length)
                    {
                        char c = text[j++];
                        if (c == '\\' && j < text.Length && (text[j] == '"' || text[j] == '\\')) { name.Append(text[j++]); continue; }
                        if (c == '"') { closed = true; break; }
                        name.Append(c);
                    }
                    if (!closed) { reason = "bad-name"; return false; }
                    tokens.Add("name=\"" + name);
                }
                else
                {
                    while (j < text.Length && text[j] != ' ' && text[j] != '\t') j++;
                    tokens.Add(text.Substring(i, j - i));
                }
                i = j;
            }
            if (tokens.Count == 0) return false;
            switch (tokens[0])
            {
                case "axis": b.Kind = ControlKind.Axis; break;
                case "button": b.Kind = ControlKind.Button; break;
                case "hat": b.Kind = ControlKind.Hat; break;
                default: return false;
            }
            reason = "bad-index";
            if (tokens.Count < 2 || !Integer(tokens[1], out int index) || index < 0 ||
                index > (b.Kind == ControlKind.Axis ? 7 : b.Kind == ControlKind.Hat ? 3 : 127)) return false;
            b.Index = index;
            int k = 2;
            if (b.Kind == ControlKind.Hat)
            {
                reason = "bad-angle";
                if (tokens.Count < 3 || !Integer(tokens[2], out int angle) || angle < 0 || angle > 35999) return false;
                b.Angle = angle; k = 3;
            }
            var seen = new HashSet<string>(StringComparer.Ordinal);
            for (; k < tokens.Count; k++)
            {
                string token = tokens[k];
                int eq = token.IndexOf('=');
                string key = eq < 0 ? token : token.Substring(0, eq);
                string value = eq < 0 ? "" : token.Substring(eq + 1);
                if (key == "inverted" && eq < 0 || key == "calibrated" && eq < 0 ||
                    key == "dev" || key == "prod" || key == "range" || key == "rest" || key == "travel" || token.StartsWith("name=\"", StringComparison.Ordinal))
                    if (!seen.Add(key)) { reason = "duplicate"; return false; }
                if (token.StartsWith("name=\"", StringComparison.Ordinal)) { b.Name = token.Substring(6); continue; }
                if (token == "inverted") { b.Inverted = true; continue; }
                if (token == "calibrated") { b.Calibrated = true; continue; }
                if (eq <= 0) { reason = "bad-attribute"; return false; }
                switch (key)
                {
                    case "dev": case "prod":
                        if (!GuidValue(value, out var guid)) { reason = "bad-guid"; return false; }
                        if (key == "dev") b.Device = guid; else b.Product = guid;
                        break;
                    case "range":
                        int dots = value.IndexOf("..", StringComparison.Ordinal);
                        if (dots < 0 || !Integer(value.Substring(0, dots), out int min) ||
                            !Integer(value.Substring(dots + 2), out int max) || max <= min) { reason = "bad-range"; return false; }
                        b.Minimum = min; b.Maximum = max;
                        break;
                    case "rest":
                        if (!Integer(value, out int rest)) { reason = "bad-rest"; return false; }
                        b.Rest = rest;
                        break;
                    case "travel":
                        if (value != "+1" && value != "-1") { reason = "bad-travel"; return false; }
                        b.Travel = value == "+1" ? 1 : -1;
                        break;
                    // Unknown key=value attributes are intentionally ignored by schema 1.
                }
            }
            if (!seen.Contains("dev")) { reason = "missing-dev"; return false; }
            if (!seen.Contains("prod")) { reason = "missing-prod"; return false; }
            if (b.Kind == ControlKind.Axis)
            {
                foreach (string key in new[] { "range", "rest", "travel" })
                    if (!seen.Contains(key)) { reason = "missing-" + key; return false; }
                if (b.Rest < b.Minimum || b.Rest > b.Maximum) { reason = "bad-rest"; return false; }
            }
            else if (seen.Contains("range") || seen.Contains("rest") || seen.Contains("travel") || b.Inverted)
            { reason = "axis-only"; return false; }
            binding = b; reason = ""; return true;
        }

        public string Fits(string action)
        {
            switch (ShapeOf(action))
            {
                case ControlShape.Centred: return Kind == ControlKind.Axis ? "" : "wrong-kind";
                case ControlShape.Pedal:
                    if (Kind == ControlKind.Button) return "";
                    if (Kind != ControlKind.Axis) return "wrong-kind";
                    return (Travel > 0 ? Maximum : Minimum) == Rest ? "rest-at-full" : "";
                case ControlShape.Digital: return Kind == ControlKind.Button || Kind == ControlKind.Hat ? "" : "wrong-kind";
                default: return "unknown-action";
            }
        }
        static double Clamp(double n, double min, double max) => Math.Max(min, Math.Min(max, n));
        public static bool DirectInputButtonDown(int value) => (value & 0x80) != 0;
        /// <summary>STD-033 circular hat match; reusable by adapters retaining an older binding store.</summary>
        public static bool HatPressed(int raw, int angle)
        {
            if (angle < 0 || angle >= 36000 || raw < 0 || (raw & 0xffff) == 0xffff) return false;
            int d = Math.Abs(raw % 36000 - angle);
            return Math.Min(d, 36000 - d) <= 4500;
        }
        public double Normalize(string action, int raw)
        {
            if (Fits(action) != "") throw new ArgumentException("Binding does not fit action", nameof(action));
            if (Kind == ControlKind.Button) return raw != 0 ? 1 : 0;
            if (Kind == ControlKind.Hat)
                return HatPressed(raw, Angle) ? 1 : 0;
            if (ShapeOf(action) == ControlShape.Centred)
            {
                double n = Clamp((raw - ((double)Minimum + Maximum) / 2) / (((double)Maximum - Minimum) / 2), -1, 1);
                return Inverted ? -n : n;
            }
            double full = Travel > 0 ? Maximum : Minimum;
            double pedal = Clamp(((double)raw - Rest) / (full - Rest), 0, 1);
            return Inverted ? 1 - pedal : pedal;
        }
        public int Denormalize(string action, double n)
        {
            if (Fits(action) != "") throw new ArgumentException("Binding does not fit action", nameof(action));
            if (double.IsNaN(n) || double.IsInfinity(n)) throw new ArgumentOutOfRangeException(nameof(n));
            if (Kind == ControlKind.Button) return n >= .5 ? 1 : 0;
            if (Kind == ControlKind.Hat) return n >= .5 ? Angle : -1;
            double v;
            if (ShapeOf(action) == ControlShape.Centred)
                v = ((double)Minimum + Maximum) / 2 + Clamp(Inverted ? -n : n, -1, 1) * (((double)Maximum - Minimum) / 2);
            else
            {
                n = Clamp(n, 0, 1);
                v = Rest + (Inverted ? 1 - n : n) * ((double)(Travel > 0 ? Maximum : Minimum) - Rest);
            }
            return (int)Clamp(Math.Round(v, MidpointRounding.AwayFromZero), Minimum, Maximum);
        }
        public static int Rescale(int value, int fromMin, int fromMax, int toMin, int toMax)
        {
            if (fromMax <= fromMin || toMax < toMin) return toMin;
            double t = ((double)value - fromMin) / ((double)fromMax - fromMin);
            return (int)Clamp(Math.Round(toMin + t * ((double)toMax - toMin), MidpointRounding.AwayFromZero), toMin, toMax);
        }
        public override string ToString()
        {
            string N(int v) => v.ToString(CultureInfo.InvariantCulture);
            var s = new StringBuilder(Kind.ToString().ToLowerInvariant()).Append(' ').Append(N(Index));
            if (Kind == ControlKind.Hat) s.Append(' ').Append(N(Angle));
            s.Append(" dev=").Append(Device.ToString("B")).Append(" prod=").Append(Product.ToString("B"));
            if (Kind == ControlKind.Axis)
            {
                s.Append(" range=").Append(N(Minimum)).Append("..").Append(N(Maximum)).Append(" rest=").Append(N(Rest))
                    .Append(Travel > 0 ? " travel=+1" : " travel=-1");
                if (Inverted) s.Append(" inverted");
            }
            if (Calibrated) s.Append(" calibrated");
            if (Name.Length != 0) s.Append(" name=\"").Append(Name.Replace("\\", "\\\\").Replace("\"", "\\\"")).Append('"');
            return s.ToString();
        }
    }
}
