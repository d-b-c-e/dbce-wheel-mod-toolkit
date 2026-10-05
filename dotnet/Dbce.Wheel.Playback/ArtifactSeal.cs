using System;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace Dbce.Wheel.Playback
{
    /// <summary>A completion record written only after every capture stream has closed.
    /// Relative names are validated and reparse points are rejected before reading any payload.</summary>
    public static class ArtifactSeal
    {
        public static string Hash(string path)
        {
            using (var sha = SHA256.Create()) using (var stream = File.OpenRead(path))
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }
        public static void Complete(string directory, string[] files)
        {
            var text = new StringBuilder("dbce-playback-seal-v1\n");
            foreach (var file in files.OrderBy(n => n, StringComparer.Ordinal))
            {
                CheckName(file);
                var path = SafePath(directory, file);
                text.Append(file).Append('\t').Append(new FileInfo(path).Length).Append('\t').Append(Hash(path)).Append('\n');
            }
            string temp = Path.Combine(directory, "complete.tsv.tmp");
            using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.Write))
            using (var writer = new StreamWriter(stream, new UTF8Encoding(false))) writer.Write(text);
            File.Move(temp, Path.Combine(directory, "complete.tsv"));
        }
        public static void Verify(string directory, params string[] required)
        {
            var lines = File.ReadAllLines(Path.Combine(directory, "complete.tsv"));
            if (lines.Length < 2 || lines[0] != "dbce-playback-seal-v1") throw new InvalidDataException("Unknown or empty completion seal");
            var names = new System.Collections.Generic.HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var line in lines.Skip(1))
            {
                var p = line.Split('\t');
                if (p.Length != 3) throw new InvalidDataException("Malformed completion seal");
                CheckName(p[0]);
                if (!names.Add(p[0])) throw new InvalidDataException("Duplicate sealed file");
                var path = SafePath(directory, p[0]);
                if (!long.TryParse(p[1], out var size) || new FileInfo(path).Length != size || Hash(path) != p[2])
                    throw new InvalidDataException("Capture changed or truncated: " + p[0]);
            }
            foreach (var file in required) if (!names.Contains(file)) throw new InvalidDataException("Unsealed required stream: " + file);
        }
        private static void CheckName(string name)
        {
            if (string.IsNullOrEmpty(name) || Path.IsPathRooted(name) ||
                name.IndexOfAny(new[] { '\\', ':', '\t', '\r', '\n' }) >= 0 ||
                name.Split('/').Any(p => p.Length == 0 || p == "." || p == ".." || p.EndsWith(".") || p.EndsWith(" ")))
                throw new InvalidDataException("Invalid sealed file name");
        }
        private static string SafePath(string directory, string name)
        {
            CheckName(name);
            string root = Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            string path = Path.GetFullPath(Path.Combine(directory, name));
            if (!path.StartsWith(root, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Sealed path escaped capture");
            for (string cursor = path; cursor != null && cursor.Length >= root.Length - 1; cursor = Path.GetDirectoryName(cursor))
                if ((File.GetAttributes(cursor) & FileAttributes.ReparsePoint) != 0) throw new InvalidDataException("Sealed path is a reparse point");
            return path;
        }
    }
}
