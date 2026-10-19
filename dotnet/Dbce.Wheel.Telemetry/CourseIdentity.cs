using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Dbce.Wheel.Telemetry
{
    /// <summary>Driving direction of a course. A reversed stage is a different course.</summary>
    public enum CourseDirection : byte
    {
        Forward = 0,
        Reverse = 1,
    }

    /// <summary>
    /// Stable identity of one drivable course: game, venue, route or layout, direction and,
    /// for user-made or generated courses, a hash of the route itself.
    /// </summary>
    /// <remarks>
    /// <para>
    /// The id is <c>dbce.&lt;game&gt;.&lt;venue&gt;.&lt;route&gt;.&lt;fwd|rev&gt;[.&lt;hash&gt;]</c>.
    /// SimHub uses the id as a file name (<c>MapRecords\&lt;id&gt;.shtl</c>) after splitting it on
    /// invalid file-name characters, so ':' or spaces would silently merge distinct ids.
    /// Tokens are therefore limited to <c>[a-z0-9-]</c> and '.' is the only separator.
    /// </para>
    /// <para>
    /// Never build an id from a localized display name or from the course length alone:
    /// SimHub's DiRT Rally 2.0 reader keys maps by length, so two equal-length stages share
    /// one map.
    /// </para>
    /// </remarks>
    public sealed class CourseIdentity
    {
        /// <summary>SimHub's <c>EnvironmentId</c> is a zero-terminated UTF8[128].</summary>
        public const int MaxIdBytes = 127;

        public const int MaxTokenLength = 40;
        public const int HashLength = 16;
        public const string Prefix = "dbce";

        public string Game { get; }
        public string Venue { get; }
        public string Route { get; }
        public CourseDirection Direction { get; }

        /// <summary>Lowercase hex from <see cref="HashPath"/>, or null for authored courses.</summary>
        public string ContentHash { get; }

        public string Id { get; }

        private CourseIdentity(string game, string venue, string route, CourseDirection direction, string hash)
        {
            Game = game;
            Venue = venue;
            Route = route;
            Direction = direction;
            ContentHash = hash;
            Id = Prefix + "." + game + "." + venue + "." + route + "." + (direction == CourseDirection.Forward ? "fwd" : "rev")
                 + (hash == null ? "" : "." + hash);
        }

        public override string ToString() => Id;

        public static bool TryCreate(string game, string venue, string route, CourseDirection direction,
            string contentHash, out CourseIdentity identity, out string error)
        {
            identity = null;
            if (!IsToken(game))  { error = "game is not a course token: " + Describe(game); return false; }
            if (!IsToken(venue)) { error = "venue is not a course token: " + Describe(venue); return false; }
            if (!IsToken(route)) { error = "route is not a course token: " + Describe(route); return false; }
            if (direction != CourseDirection.Forward && direction != CourseDirection.Reverse)
            {
                error = "unknown direction " + (int)direction;
                return false;
            }
            if (contentHash != null && !IsHash(contentHash))
            {
                error = "content hash must be " + HashLength + " lowercase hex characters";
                return false;
            }
            var candidate = new CourseIdentity(game, venue, route, direction, contentHash);
            if (Encoding.UTF8.GetByteCount(candidate.Id) > MaxIdBytes)
            {
                error = "id is longer than " + MaxIdBytes + " bytes";
                return false;
            }
            identity = candidate;
            error = null;
            return true;
        }

        public static CourseIdentity Create(string game, string venue, string route, CourseDirection direction,
            string contentHash = null)
        {
            if (!TryCreate(game, venue, route, direction, contentHash, out var identity, out var error))
                throw new ArgumentException(error);
            return identity;
        }

        public static bool TryParse(string id, out CourseIdentity identity)
        {
            identity = null;
            if (id == null) return false;
            string[] parts = id.Split('.');
            if ((parts.Length != 5 && parts.Length != 6) || parts[0] != Prefix) return false;
            CourseDirection direction;
            if (parts[4] == "fwd") direction = CourseDirection.Forward;
            else if (parts[4] == "rev") direction = CourseDirection.Reverse;
            else return false;
            return TryCreate(parts[1], parts[2], parts[3], direction, parts.Length == 6 ? parts[5] : null,
                out identity, out _);
        }

        /// <summary>1..40 characters of [a-z0-9-], not starting or ending with '-'.</summary>
        public static bool IsToken(string s)
        {
            if (string.IsNullOrEmpty(s) || s.Length > MaxTokenLength || s[0] == '-' || s[s.Length - 1] == '-')
                return false;
            foreach (char c in s)
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
                    return false;
            return true;
        }

        /// <summary>
        /// Turns display text into a token: lowercase ASCII letters and digits, every other run
        /// of characters becomes one '-'. Returns null when nothing usable is left.
        /// </summary>
        /// <remarks>
        /// Adapters call this explicitly on stable internal names (asset or scene names), never
        /// on localized UI text. Distinct inputs can slug to the same token ("SS 1" and "ss-1"),
        /// so an adapter must not slug two names that only differ in punctuation.
        /// </remarks>
        public static string Slug(string text)
        {
            if (text == null) return null;
            var sb = new StringBuilder(text.Length);
            bool pendingDash = false;
            foreach (char raw in text)
            {
                char c = char.ToLowerInvariant(raw);
                bool keep = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
                if (!keep)
                {
                    pendingDash = sb.Length > 0;
                    continue;
                }
                if (pendingDash) sb.Append('-');
                pendingDash = false;
                sb.Append(c);
            }
            string token = sb.Length > MaxTokenLength ? sb.ToString(0, MaxTokenLength).TrimEnd('-') : sb.ToString();
            return token.Length == 0 ? null : token;
        }

        /// <summary>
        /// 64-bit FNV-1a over the exact float32 bits of each point's north, east and up, in
        /// driving order, as 16 lowercase hex characters.
        /// </summary>
        /// <remarks>
        /// Games store authored positions as float32, so hashing those bits is deterministic
        /// for the same asset and changes when any node moves or the order is reversed. Hash
        /// the authored (local, unshifted) positions, not positions after a floating-origin
        /// shift.
        /// </remarks>
        public static string HashPath(IList<CourseVector> points)
        {
            if (points == null) throw new ArgumentNullException(nameof(points));
            ulong h = 14695981039346656037UL;
            foreach (var p in points)
            {
                h = Mix(h, (float)p.North);
                h = Mix(h, (float)p.East);
                h = Mix(h, (float)p.Up);
            }
            return h.ToString("x16", CultureInfo.InvariantCulture);
        }

        private static ulong Mix(ulong h, float v)
        {
            uint bits = (uint)BitConverter.ToInt32(BitConverter.GetBytes(v), 0);
            for (int i = 0; i < 4; i++)
            {
                h ^= (bits >> (8 * i)) & 0xFF;
                h *= 1099511628211UL;
            }
            return h;
        }

        private static bool IsHash(string s)
        {
            if (s.Length != HashLength) return false;
            foreach (char c in s)
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                    return false;
            return true;
        }

        private static string Describe(string s) => s == null ? "null" : "\"" + s + "\"";
    }
}
