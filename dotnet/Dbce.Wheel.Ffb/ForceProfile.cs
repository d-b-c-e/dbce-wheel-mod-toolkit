using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace Dbce.Wheel.Ffb
{
    /// <summary>
    /// A named, versioned set of every parameter that decides how a game feels:
    /// the <see cref="ForceModelSettings"/> and the <see cref="ForceShaper"/>
    /// conditioning chain together, as data.
    /// </summary>
    /// <remarks>
    /// <para>
    /// The point of versioning is that a tune can be swapped without rebuilding
    /// a game and without losing the one it replaced. A consumer names a profile
    /// (<c>arcade-outrun@1</c>); to try a different feel you add a new version,
    /// point the game's config at it, and drive both. Comparing two builds is
    /// how tuning stops being reproducible.
    /// </para>
    /// <para>
    /// <b>A published version is immutable.</b> Editing <c>arcade-outrun@1</c>
    /// makes every earlier report about it a lie, including the one in the case
    /// file. Add <c>@2</c> instead; <c>inherits</c> keeps that to the lines that
    /// actually changed.
    /// </para>
    /// <para>
    /// The same files are parsed by <c>native/forcemodel/force_profile.h</c>, and
    /// a conformance test pins the two readers to identical results. Add a key in
    /// one and you must add it in the other.
    /// </para>
    /// </remarks>
    public sealed class ForceProfile
    {
        /// <summary>Family name, e.g. <c>arcade-outrun</c>. Lower case, no spaces.</summary>
        public string Name = "";
        /// <summary>Version within the family. Immutable once published.</summary>
        public int Version = 1;
        /// <summary>What this tune is and where it came from. Shown in panels and traces.</summary>
        public string Description = "";
        /// <summary><c>name@version</c>, the id a game names in its config.</summary>
        public string Id => Name + "@" + Version.ToString(CultureInfo.InvariantCulture);

        public ForceModelSettings Model = new ForceModelSettings();
        public ForceShaper Shaper = new ForceShaper();

        /// <summary>Keys the file carried that this build does not understand. Never silently dropped.</summary>
        public readonly List<string> UnknownKeys = new List<string>();

        // ---------------------------------------------------------------- keys
        // The vocabulary. Keep in step with native/forcemodel/force_profile.h; the
        // conformance test fails if the two disagree.
        internal static readonly string[] KeyNames =
        {
            "model.spring.strength", "model.spring.fullSpeedMps",
            "model.damper.strength", "model.damper.staticFraction",
            "model.lateral.weight", "model.lateral.forceReference", "model.lateral.gReference",
            "model.lateral.trailFloor", "model.lateral.trailSlipSpan", "model.lateral.gripLoss",
            "model.load.weightTransfer", "model.load.transferMin", "model.load.transferMax",
            "model.texture.strength", "model.texture.hz",
            "model.impact.strength", "model.impact.seconds",
            "model.shift.strength", "model.shift.seconds",
            "shaper.strength", "shaper.invert", "shaper.deadzone",
            "shaper.attackSmoothing", "shaper.decaySmoothing", "shaper.softSaturation",
            "shaper.slewPerSecond", "shaper.outputDeadband",
            "shaper.fadeStartKmh", "shaper.fadeFullKmh", "shaper.rampSeconds", "shaper.peakLimit",
        };

        private bool Set(string key, string value)
        {
            switch (key)
            {
                case "model.spring.strength":       Model.SpringStrength = F(value); return true;
                case "model.spring.fullSpeedMps":   Model.SpringFullSpeedMps = F(value); return true;
                case "model.damper.strength":       Model.DamperStrength = F(value); return true;
                case "model.damper.staticFraction": Model.DamperStaticFraction = F(value); return true;
                case "model.lateral.weight":        Model.SteeringWeight = F(value); return true;
                case "model.lateral.forceReference":Model.LateralForceReference = F(value); return true;
                case "model.lateral.gReference":    Model.LateralGReference = F(value); return true;
                case "model.lateral.trailFloor":    Model.TrailFloor = F(value); return true;
                case "model.lateral.trailSlipSpan": Model.TrailSlipSpan = F(value); return true;
                case "model.lateral.gripLoss":      Model.GripLoss = F(value); return true;
                case "model.load.weightTransfer":   Model.WeightTransfer = F(value); return true;
                case "model.load.transferMin":      Model.WeightTransferMin = F(value); return true;
                case "model.load.transferMax":      Model.WeightTransferMax = F(value); return true;
                case "model.texture.strength":      Model.TextureStrength = F(value); return true;
                case "model.texture.hz":            Model.TextureHz = F(value); return true;
                case "model.impact.strength":       Model.ImpactStrength = F(value); return true;
                case "model.impact.seconds":        Model.ImpactSeconds = F(value); return true;
                case "model.shift.strength":        Model.ShiftPulseStrength = F(value); return true;
                case "model.shift.seconds":         Model.ShiftPulseSeconds = F(value); return true;
                case "shaper.strength":             Shaper.Strength = (int)Math.Round(F(value)); return true;
                case "shaper.invert":               Shaper.Invert = B(value); return true;
                case "shaper.deadzone":             Shaper.Deadzone = F(value); return true;
                case "shaper.attackSmoothing":      Shaper.AttackSmoothing = F(value); return true;
                case "shaper.decaySmoothing":       Shaper.DecaySmoothing = F(value); return true;
                case "shaper.softSaturation":       Shaper.SoftSaturation = F(value); return true;
                case "shaper.slewPerSecond":        Shaper.SlewPerSecond = F(value); return true;
                case "shaper.outputDeadband":       Shaper.OutputDeadband = F(value); return true;
                case "shaper.fadeStartKmh":         Shaper.FadeStartKmh = F(value); return true;
                case "shaper.fadeFullKmh":          Shaper.FadeFullKmh = F(value); return true;
                case "shaper.rampSeconds":          Shaper.RampSeconds = F(value); return true;
                case "shaper.peakLimit":            Shaper.PeakLimit = F(value); return true;
                default: return false;
            }
        }

        /// <summary>Current value of a key, for round-tripping and for the conformance test.</summary>
        public string Get(string key)
        {
            switch (key)
            {
                case "model.spring.strength":       return S(Model.SpringStrength);
                case "model.spring.fullSpeedMps":   return S(Model.SpringFullSpeedMps);
                case "model.damper.strength":       return S(Model.DamperStrength);
                case "model.damper.staticFraction": return S(Model.DamperStaticFraction);
                case "model.lateral.weight":        return S(Model.SteeringWeight);
                case "model.lateral.forceReference":return S(Model.LateralForceReference);
                case "model.lateral.gReference":    return S(Model.LateralGReference);
                case "model.lateral.trailFloor":    return S(Model.TrailFloor);
                case "model.lateral.trailSlipSpan": return S(Model.TrailSlipSpan);
                case "model.lateral.gripLoss":      return S(Model.GripLoss);
                case "model.load.weightTransfer":   return S(Model.WeightTransfer);
                case "model.load.transferMin":      return S(Model.WeightTransferMin);
                case "model.load.transferMax":      return S(Model.WeightTransferMax);
                case "model.texture.strength":      return S(Model.TextureStrength);
                case "model.texture.hz":            return S(Model.TextureHz);
                case "model.impact.strength":       return S(Model.ImpactStrength);
                case "model.impact.seconds":        return S(Model.ImpactSeconds);
                case "model.shift.strength":        return S(Model.ShiftPulseStrength);
                case "model.shift.seconds":         return S(Model.ShiftPulseSeconds);
                case "shaper.strength":             return Shaper.Strength.ToString(CultureInfo.InvariantCulture);
                case "shaper.invert":               return Shaper.Invert ? "true" : "false";
                case "shaper.deadzone":             return S(Shaper.Deadzone);
                case "shaper.attackSmoothing":      return S(Shaper.AttackSmoothing);
                case "shaper.decaySmoothing":       return S(Shaper.DecaySmoothing);
                case "shaper.softSaturation":       return S(Shaper.SoftSaturation);
                case "shaper.slewPerSecond":        return S(Shaper.SlewPerSecond);
                case "shaper.outputDeadband":       return S(Shaper.OutputDeadband);
                case "shaper.fadeStartKmh":         return S(Shaper.FadeStartKmh);
                case "shaper.fadeFullKmh":          return S(Shaper.FadeFullKmh);
                case "shaper.rampSeconds":          return S(Shaper.RampSeconds);
                case "shaper.peakLimit":            return S(Shaper.PeakLimit);
                default: throw new ArgumentException("unknown profile key: " + key, nameof(key));
            }
        }

        private static float F(string v)
        {
            float f;
            return float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out f) ? f : 0f;
        }
        private static bool B(string v)
        {
            v = v.Trim();
            return v.Equals("true", StringComparison.OrdinalIgnoreCase) || v == "1" ||
                   v.Equals("yes", StringComparison.OrdinalIgnoreCase);
        }
        private static string S(float f) => f.ToString("0.######", CultureInfo.InvariantCulture);

        // ---------------------------------------------------------------- parsing

        /// <summary>Every profile id defined in the text, in file order.</summary>
        public static List<string> ListIds(string text)
        {
            var ids = new List<string>();
            foreach (var line in SplitLines(text))
            {
                string t = Strip(line);
                if (t.Length > 2 && t[0] == '[' && t[t.Length - 1] == ']')
                    ids.Add(t.Substring(1, t.Length - 2).Trim());
            }
            return ids;
        }

        /// <summary>
        /// Reads one profile out of the text. Resolves <c>inherits</c> first, so a
        /// version can state only what it changes. Throws if the id is absent or
        /// the inheritance chain loops.
        /// </summary>
        public static ForceProfile Parse(string text, string id)
        {
            var profile = new ForceProfile();
            var seen = new List<string>();
            ApplySection(text, id, profile, seen);

            int at = id.LastIndexOf('@');
            if (at <= 0) throw new FormatException("profile id must be name@version: " + id);
            profile.Name = id.Substring(0, at);
            int v;
            if (!int.TryParse(id.Substring(at + 1), NumberStyles.Integer, CultureInfo.InvariantCulture, out v))
                throw new FormatException("profile version must be an integer: " + id);
            profile.Version = v;
            return profile;
        }

        private static void ApplySection(string text, string id, ForceProfile into, List<string> seen)
        {
            if (seen.Contains(id)) throw new FormatException("profile inherits itself: " + string.Join(" -> ", seen) + " -> " + id);
            seen.Add(id);

            var body = SectionBody(text, id);
            if (body == null) throw new FormatException("no such profile: " + id);

            // Resolve the parent before applying our own keys, so ours win.
            string parent = null, description = null;
            var pairs = new List<KeyValuePair<string, string>>();
            foreach (var line in body)
            {
                string t = Strip(line);
                if (t.Length == 0) continue;
                int eq = t.IndexOf('=');
                if (eq <= 0) continue;
                string key = t.Substring(0, eq).Trim();
                string val = t.Substring(eq + 1).Trim();
                if (key.Equals("inherits", StringComparison.OrdinalIgnoreCase)) { parent = val; continue; }
                if (key.Equals("description", StringComparison.OrdinalIgnoreCase)) { description = val; continue; }
                pairs.Add(new KeyValuePair<string, string>(key, val));
            }

            if (parent != null) ApplySection(text, parent, into, seen);
            if (description != null) into.Description = description;
            foreach (var kv in pairs)
                if (!into.Set(kv.Key, kv.Value) && !into.UnknownKeys.Contains(kv.Key))
                    into.UnknownKeys.Add(kv.Key);
        }

        // The LAST definition wins, so the user file (appended to the shipped one
        // by LoadFrom) overrides a profile of the same id without editing ours.
        private static List<string> SectionBody(string text, string id)
        {
            List<string> found = null, body = null;
            foreach (var line in SplitLines(text))
            {
                string t = Strip(line);
                if (t.Length > 2 && t[0] == '[' && t[t.Length - 1] == ']')
                {
                    if (body != null) { found = body; body = null; }
                    string here = t.Substring(1, t.Length - 2).Trim();
                    if (string.Equals(here, id, StringComparison.OrdinalIgnoreCase)) body = new List<string>();
                    continue;
                }
                if (body != null) body.Add(line);
            }
            return body ?? found;
        }

        private static IEnumerable<string> SplitLines(string text) =>
            text.Replace("\r\n", "\n").Replace('\r', '\n').Split('\n');

        /// <summary>Strips a trailing comment (# or ;) and surrounding whitespace.</summary>
        private static string Strip(string line)
        {
            int cut = -1;
            for (int i = 0; i < line.Length; i++)
                if (line[i] == '#' || line[i] == ';') { cut = i; break; }
            return (cut >= 0 ? line.Substring(0, cut) : line).Trim();
        }

        /// <summary>Reads a profile from a file. <paramref name="id"/> is <c>name@version</c>.</summary>
        public static ForceProfile Load(string path, string id) => Parse(File.ReadAllText(path), id);

        /// <summary>The file a game ships; overwritten when the toolkit is updated.</summary>
        public const string ShippedFileName = "force-profiles.ini";
        /// <summary>The player's own tunes; never overwritten, and its sections win.</summary>
        public const string UserFileName = "force-profiles.user.ini";

        /// <summary>
        /// Reads <paramref name="id"/> from <c>force-profiles.ini</c> in
        /// <paramref name="directory"/>, with <c>force-profiles.user.ini</c>
        /// layered on top when it exists, so a player's edits survive an update
        /// and a profile of the same id in the user file wins outright.
        /// </summary>
        /// <returns>The profile, or null when neither file defines the id.</returns>
        public static ForceProfile LoadFrom(string directory, string id, out string diagnostic)
        {
            diagnostic = null;
            try
            {
                string shipped = Path.Combine(directory, ShippedFileName);
                string user = Path.Combine(directory, UserFileName);
                var sb = new StringBuilder();
                if (File.Exists(shipped)) sb.Append(File.ReadAllText(shipped)).Append('\n');
                if (File.Exists(user)) sb.Append(File.ReadAllText(user)).Append('\n');
                if (sb.Length == 0) { diagnostic = "no " + ShippedFileName + " in " + directory; return null; }

                var profile = Parse(sb.ToString(), id);
                if (profile.UnknownKeys.Count > 0)
                    diagnostic = "profile " + id + " has key(s) this build does not understand: " +
                                 string.Join(", ", profile.UnknownKeys.ToArray());
                return profile;
            }
            catch (Exception ex)
            {
                diagnostic = ex.Message;
                return null;
            }
        }

        /// <summary>Every profile id available in a directory, shipped and user.</summary>
        public static List<string> AvailableIn(string directory)
        {
            var all = new List<string>();
            foreach (var name in new[] { ShippedFileName, UserFileName })
            {
                string path = Path.Combine(directory, name);
                if (!File.Exists(path)) continue;
                foreach (var id in ListIds(File.ReadAllText(path)))
                    if (!all.Contains(id)) all.Add(id);
            }
            return all;
        }

        /// <summary>Renders this profile as a section, every key explicit. Useful for a support bundle.</summary>
        public string ToIni()
        {
            var sb = new StringBuilder();
            sb.Append('[').Append(Id).Append(']').Append('\n');
            if (!string.IsNullOrEmpty(Description)) sb.Append("description = ").Append(Description).Append('\n');
            foreach (var key in KeyNames) sb.Append(key).Append(" = ").Append(Get(key)).Append('\n');
            return sb.ToString();
        }

        /// <summary>A one-line summary for a trace or a log header.</summary>
        public override string ToString() =>
            Id + (string.IsNullOrEmpty(Description) ? "" : " (" + Description + ")");
    }
}
