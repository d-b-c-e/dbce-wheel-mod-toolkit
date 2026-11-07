using System.IO.Abstractions;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.RegularExpressions;
using VictorBush.Ego.NefsLib.IO;
using VictorBush.Ego.NefsLib.ArchiveSource;
using VictorBush.Ego.NefsLib.Item;
using VictorBush.Ego.NefsLib.Progress;

// Read-only archive intake. No writer, native DLL, game hook or game launch.
try
{
var headless = args.Length > 0 && args[0] == "--headless";
if ((!headless && args.Length is < 2 or > 3) || (headless && args.Length is < 4 or > 5))
{
    Console.Error.WriteLine("NefsInspect <archive> <new output directory> [extract path regex]");
    Console.Error.WriteLine("NefsInspect --headless <exe> <game.dat> <new output directory> [extract path regex]");
    return 2;
}
var source = Path.GetFullPath(args[headless ? 2 : 0]);
var output = Path.GetFullPath(args[headless ? 3 : 1]);
if (!File.Exists(source)) throw new FileNotFoundException("Archive missing", source);
if (Directory.Exists(output) || File.Exists(output)) throw new IOException("Output already exists");
// Never write into the game/archive's directory tree.
var sourceParent = Path.GetDirectoryName(source)! + Path.DirectorySeparatorChar;
if ((output + Path.DirectorySeparatorChar).StartsWith(sourceParent, StringComparison.OrdinalIgnoreCase))
    throw new IOException("Output must be outside the archive directory tree");
var filterIndex = headless ? 4 : 2;
var filter = args.Length > filterIndex ? new Regex(args[filterIndex], RegexOptions.IgnoreCase, TimeSpan.FromSeconds(1)) : null;
using var deadline = new CancellationTokenSource(TimeSpan.FromMinutes(5));
var progress = new NefsProgress(deadline.Token);
var fs = new FileSystem();
var before = new FileInfo(source);
long sourceLength = before.Length;
DateTime sourceTime = before.LastWriteTimeUtc;
NefsArchiveSource archiveSource = NefsArchiveSource.Standard(source);
string? headerSha256 = null;
if (headless)
{
    var headerPath = Path.GetFullPath(args[1]);
    using var header = File.OpenRead(headerPath);
    headerSha256 = Convert.ToHexString(SHA256.HashData(header));
    var found = await new NefsExeHeaderFinder(fs).FindHeadersAsync(headerPath, Path.GetDirectoryName(source)!, false, progress);
    archiveSource = found.Single(s => string.Equals(Path.GetFullPath(s.FilePath), source, StringComparison.OrdinalIgnoreCase));
}
var archive = await new NefsReader(fs).ReadArchiveAsync(archiveSource, progress);
var items = archive.Items.EnumerateById().ToArray();
var rows = items.Select(i => new {
    id = i.Id.ToString(), path = archive.Items.GetItemFilePath(i.Id),
    bytes = i.ExtractedSize, kind = i.Type.ToString()
}).ToArray();
Directory.CreateDirectory(output);
await File.WriteAllTextAsync(Path.Combine(output, "index.json"), JsonSerializer.Serialize(new {
    source, sourceLength, sourceTime, headerSha256,
    headerSource = headless ? (HeadlessSource)archiveSource : null,
    archiveVersion = archive.Header.Version.ToString(),
    headerSize = archive.Header.Size, encryptedHeader = archive.Header.IsEncrypted, items = rows
}, new JsonSerializerOptions { WriteIndented = true }));
var selected = items.Where(i => i.Type == NefsItemType.File &&
    filter?.IsMatch(archive.Items.GetItemFilePath(i.Id)) == true).ToArray();
if (selected.Length > 512 || selected.Any(i => i.ExtractedSize > 64 * 1024 * 1024) ||
    selected.Sum(i => (long)i.ExtractedSize) > 256L * 1024 * 1024)
    throw new IOException("Selection exceeds 512 files / 64 MiB per file / 256 MiB total; narrow the regex");
var payloadRoot = Path.Combine(output, "files") + Path.DirectorySeparatorChar;
var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
var plans = selected.Select(item => {
    var relative = archive.Items.GetItemFilePath(item.Id).Replace('\\', '/');
    if (Path.IsPathRooted(relative) || relative.Contains(':')) throw new IOException("Rooted archive path");
    var destination = Path.GetFullPath(Path.Combine(payloadRoot, relative));
    if (!destination.StartsWith(payloadRoot, StringComparison.OrdinalIgnoreCase) || !seen.Add(destination))
        throw new IOException("Unsafe or duplicate archive path");
    return (item, relative, destination);
}).ToArray();
var transformer = new NefsTransformer(fs);
var extracted = new List<object>();
foreach (var (item, relative, destination) in plans)
{
    Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
    // output is fresh and every destination prevalidated; library reads archive data only.
    await transformer.DetransformAsync(item.DataSource, destination, 0, progress);
    if (new FileInfo(destination).Length != item.ExtractedSize) throw new IOException("Extracted size mismatch");
    using var data = File.OpenRead(destination);
    extracted.Add(new { path = relative, bytes = item.ExtractedSize, sha256 = Convert.ToHexString(SHA256.HashData(data)) });
}
var after = new FileInfo(source);
if (after.Length != sourceLength || after.LastWriteTimeUtc != sourceTime) throw new IOException("Source changed during read");
await File.WriteAllTextAsync(Path.Combine(output, "complete.json"), JsonSerializer.Serialize(new {
    status = "read", source, itemCount = items.Length, extracted,
    sourceLengthAndTimestampUnchanged = true, sourceWritten = false
}, new JsonSerializerOptions { WriteIndented = true }));
Console.WriteLine($"Indexed {items.Length} items; extracted {extracted.Count}; source opened read-only.");
return 0;
}
catch (Exception error)
{
    Console.Error.WriteLine($"NeFS inspection failed: {error.Message}");
    return 1;
}
