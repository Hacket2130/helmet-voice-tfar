using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace HelmetVoiceSetup
{
    public sealed class ReleaseManifest
    {
        public int schemaVersion;
        public string product;
        public int releaseNumber;
        public string displayVersion;
        public string pluginVersion;
        public string packageUrl;
        public string sha256;
        public long packageBytes;
        public string releasePageUrl;
        public string notes;
    }

    public sealed class InstalledRelease
    {
        public int ReleaseNumber;
        public string DisplayVersion;
        public string ReleaseDirectory;
        public string ModDirectory;
        public string PluginInstaller;
    }

    // No code in this class starts TeamSpeak, changes a live game install, or runs downloads.
    // A release becomes current only after its archive and mod files have been verified.
    public static class InstallerCore
    {
        public const string DefaultFeedUrl = "https://github.com/Hacket2130/helmet-voice-tfar/releases/latest/download/update.json";
        internal const long MaxPackageBytes = 512L * 1024 * 1024;
        internal const long MaxExpandedBytes = 2L * 1024 * 1024 * 1024;
        internal const int MaxEntries = 10000;
        internal const int MaxManifestBytes = 64 * 1024;
        private const string RepoPath = "/Hacket2130/helmet-voice-tfar/releases/";
        private const string MetadataName = ".helmet-voice-release.json";
        private static readonly string[] RequiredModFiles = {
            "addons/tfar_core.pbo", "addons/dtr_core.pbo", "task_force_radio_pipe.dll",
            "task_force_radio_pipe_x64.dll", "teamspeak/task_force_radio_dtr.ts3_plugin"
        };

        private sealed class InstalledState
        {
            public int schemaVersion;
            public int releaseNumber;
            public string releaseMetadataSha256;
        }

        private sealed class ReleaseMetadata
        {
            public int schemaVersion;
            public ReleaseManifest manifest;
            public string validationSha256;
        }

        private sealed class FileRecord
        {
            public string Path { get; set; }
            public string SHA256 { get; set; }
            public long Bytes { get; set; }
        }

        private static JavaScriptSerializer Serializer()
        {
            return new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024, RecursionLimit = 20 };
        }

        public static ReleaseManifest LoadBundledManifest()
        {
            using (Stream stream = Resource("BundledManifest.json"))
                return ParseManifest(ReadBounded(stream, MaxManifestBytes));
        }

        public static ReleaseManifest GetLatest()
        {
            using (HttpWebResponse response = OpenHttps(new Uri(DefaultFeedUrl)))
            using (Stream stream = response.GetResponseStream())
                return ParseManifest(ReadBounded(stream, MaxManifestBytes));
        }

        public static InstalledRelease ReadState(string root)
        {
            root = NormalizeRoot(root);
            string statePath = Child(root, "installed.json");
            EnsureNoReparse(statePath);
            if (!File.Exists(statePath))
            {
                if (Directory.Exists(statePath)) throw Invalid("The installed state path is a directory.");
                return null;
            }
            InstalledState state = ReadJson<InstalledState>(statePath, MaxManifestBytes);
            if (state == null || state.schemaVersion != 1 || state.releaseNumber < 1 || !IsHash(state.releaseMetadataSha256))
                throw Invalid("The installed state is invalid. Earlier release folders have been retained.");
            string releasePath = ReleasePath(root, state.releaseNumber);
            string metadataPath = Child(releasePath, MetadataName);
            RequireFile(metadataPath);
            if (!EqualHash(HashFile(metadataPath), state.releaseMetadataSha256))
                throw Invalid("The installed release metadata has changed. No files were replaced.");
            ReleaseMetadata metadata = VerifyRelease(releasePath, null);
            if (metadata.manifest.releaseNumber != state.releaseNumber)
                throw Invalid("The installed state and release version disagree.");
            return Describe(releasePath, metadata.manifest);
        }

        public static InstalledRelease InstallBundled(string root, Action<string> progress)
        {
            ReleaseManifest manifest = LoadBundledManifest();
            return Install(root, manifest, delegate(Stream output) {
                using (Stream input = Resource("BundledRelease.zip"))
                    CopyExact(input, output, manifest.packageBytes);
            }, progress);
        }

        public static InstalledRelease InstallOnline(string root, ReleaseManifest manifest, Action<string> progress)
        {
            manifest = ParseManifest(Encoding.UTF8.GetBytes(Serializer().Serialize(manifest)));
            return Install(root, manifest, delegate(Stream output) {
                Report(progress, "Downloading " + manifest.displayVersion + " from GitHub Releases...");
                using (HttpWebResponse response = OpenHttps(new Uri(manifest.packageUrl)))
                {
                    if (response.ContentLength >= 0 && response.ContentLength != manifest.packageBytes)
                        throw Invalid("The download length does not match the release manifest.");
                    using (Stream input = response.GetResponseStream())
                        CopyExact(input, output, manifest.packageBytes);
                }
            }, progress);
        }

        // Shared offline path permits tests to exercise the exact production transaction.
        internal static InstalledRelease InstallArchive(string root, ReleaseManifest manifest, string archive, Action<string> progress)
        {
            return Install(root, manifest, delegate(Stream output) {
                using (Stream input = File.OpenRead(archive)) CopyExact(input, output, manifest.packageBytes);
            }, progress);
        }

        internal static bool IsNewer(ReleaseManifest available, InstalledRelease installed)
        {
            ValidateManifest(available);
            return installed == null || available.releaseNumber > installed.ReleaseNumber;
        }

        private static InstalledRelease Install(string root, ReleaseManifest manifest, Action<Stream> getArchive, Action<string> progress)
        {
            // Snapshot the public object so the UI cannot change URLs or hashes mid-install.
            manifest = ParseManifest(Encoding.UTF8.GetBytes(Serializer().Serialize(manifest)));
            root = NormalizeRoot(root);
            SafeCreateDirectory(root);
            string lockPath = Child(root, ".install.lock");
            EnsureNoReparse(lockPath);
            using (FileStream installLock = new FileStream(lockPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None))
            {
                InstalledRelease current = ReadState(root);
                if (current != null && current.ReleaseNumber > manifest.releaseNumber)
                {
                    Report(progress, "Keeping the newer installed release " + current.DisplayVersion + ".");
                    return current;
                }
                string target = ReleasePath(root, manifest.releaseNumber);
                EnsureNoReparse(target);
                if (Directory.Exists(target))
                {
                    Report(progress, "Verifying the existing " + manifest.displayVersion + " installation...");
                    VerifyRelease(target, manifest);
                    InstalledRelease existing = Describe(target, manifest);
                    if (current == null || current.ReleaseNumber != existing.ReleaseNumber) WriteState(root, target, manifest);
                    return existing;
                }
                if (File.Exists(target)) throw Invalid("A file already occupies the release directory. No files were replaced.");

                string stagingParent = Child(root, ".staging");
                SafeCreateDirectory(stagingParent);
                string stage = Child(stagingParent, Guid.NewGuid().ToString("N"));
                SafeCreateDirectory(stage);
                try
                {
                    string archive = Child(stage, "release.zip");
                    Report(progress, "Reading and verifying " + manifest.displayVersion + "...");
                    using (FileStream output = new FileStream(archive, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                    {
                        getArchive(output);
                        output.Flush(true);
                    }
                    VerifyArchive(archive, manifest);
                    string payload = Child(stage, "payload");
                    SafeCreateDirectory(payload);
                    Report(progress, "Extracting the verified package...");
                    ExtractArchive(archive, payload);
                    string validationHash = ValidateModFiles(payload);
                    ReleaseMetadata metadata = new ReleaseMetadata {
                        schemaVersion = 1, manifest = manifest, validationSha256 = validationHash
                    };
                    WriteNewJson(Child(payload, MetadataName), metadata);
                    VerifyRelease(payload, manifest);
                    SafeCreateDirectory(Child(root, "Releases"));
                    EnsureNoReparse(target);
                    if (Directory.Exists(target) || File.Exists(target)) throw Invalid("The release directory appeared during installation. Nothing was overwritten.");
                    Directory.Move(payload, target);
                    // Promotion precedes the atomic state switch. A failed state write leaves the
                    // prior current release intact and this verified directory available to retry.
                    Report(progress, "Selecting the verified " + manifest.displayVersion + " release...");
                    WriteState(root, target, manifest);
                    return Describe(target, manifest);
                }
                finally { CleanupStage(stagingParent, stage); }
            }
        }

        internal static ReleaseManifest ParseManifest(byte[] utf8)
        {
            if (utf8 == null || utf8.Length == 0 || utf8.Length > MaxManifestBytes) throw Invalid("The update manifest is empty or too large.");
            try
            {
                string json = new UTF8Encoding(false, true).GetString(utf8).TrimStart('\uFEFF');
                Dictionary<string, object> map = Serializer().DeserializeObject(json) as Dictionary<string, object>;
                if (map == null) throw Invalid("The update manifest must be a JSON object.");
                ReleaseManifest manifest = new ReleaseManifest {
                    schemaVersion = IntField(map, "schemaVersion"), product = StringField(map, "product"),
                    releaseNumber = IntField(map, "releaseNumber"), displayVersion = StringField(map, "displayVersion"),
                    pluginVersion = StringField(map, "pluginVersion"), packageUrl = StringField(map, "packageUrl"),
                    sha256 = StringField(map, "sha256"), packageBytes = LongField(map, "packageBytes"),
                    releasePageUrl = StringField(map, "releasePageUrl"), notes = StringField(map, "notes")
                };
                ValidateManifest(manifest);
                return manifest;
            }
            catch (InvalidDataException) { throw; }
            catch (Exception ex) { throw Invalid("The update manifest is not valid JSON: " + ex.Message); }
        }

        internal static void ValidateManifest(ReleaseManifest manifest)
        {
            if (manifest == null || manifest.schemaVersion != 1 || manifest.product != "helmet-voice-tfar")
                throw Invalid("This update manifest is not for Task force Vanguard TFAR addon or uses an unsupported format.");
            if (manifest.releaseNumber < 1 || manifest.releaseNumber > 1000000 || manifest.displayVersion != "R" + manifest.releaseNumber.ToString(CultureInfo.InvariantCulture))
                throw Invalid("The release number or display version is invalid.");
            if (String.IsNullOrWhiteSpace(manifest.pluginVersion) || manifest.pluginVersion.Length > 80 || !Regex.IsMatch(manifest.pluginVersion, "\\A[A-Za-z0-9][A-Za-z0-9._+-]*\\z"))
                throw Invalid("The plugin version is invalid.");
            if (!IsHash(manifest.sha256)) throw Invalid("The release SHA-256 is invalid.");
            if (manifest.packageBytes < 32 || manifest.packageBytes > MaxPackageBytes) throw Invalid("The release package size is outside the allowed range.");
            ValidateRepositoryUrl(manifest.packageUrl, "download/");
            Uri packageUri = new Uri(manifest.packageUrl);
            if (!packageUri.AbsolutePath.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) throw Invalid("The release package must be a ZIP archive.");
            ValidateRepositoryUrl(manifest.releasePageUrl, "tag/");
            if (manifest.notes == null || manifest.notes.Length > 16000) throw Invalid("The release notes are missing or too long.");
        }

        private static object Field(Dictionary<string, object> map, string key)
        {
            object result;
            if (!map.TryGetValue(key, out result) || result == null) throw Invalid("Missing update manifest field: " + key);
            return result;
        }

        private static string StringField(Dictionary<string, object> map, string key)
        {
            string result = Field(map, key) as string;
            if (result == null) throw Invalid("The manifest field must be text: " + key);
            return result;
        }

        private static long LongField(Dictionary<string, object> map, string key)
        {
            object value = Field(map, key);
            if (!(value is int) && !(value is long)) throw Invalid("The manifest field must be an integer: " + key);
            return Convert.ToInt64(value, CultureInfo.InvariantCulture);
        }

        private static int IntField(Dictionary<string, object> map, string key)
        {
            long value = LongField(map, key);
            if (value < Int32.MinValue || value > Int32.MaxValue) throw Invalid("The manifest integer is out of range: " + key);
            return (int)value;
        }

        private static Uri HttpsUri(string value)
        {
            Uri uri;
            if (String.IsNullOrEmpty(value) || value.Length > 8192 || !Uri.TryCreate(value, UriKind.Absolute, out uri) ||
                uri.Scheme != Uri.UriSchemeHttps || !uri.IsDefaultPort || uri.UserInfo.Length != 0 || uri.Fragment.Length != 0 || value.IndexOf('\\') >= 0)
                throw Invalid("Only HTTPS release URLs without credentials or fragments are allowed.");
            return uri;
        }

        private static void ValidateRepositoryUrl(string value, string suffix)
        {
            Uri uri = HttpsUri(value);
            string prefix = RepoPath + suffix;
            if (!uri.Host.Equals("github.com", StringComparison.OrdinalIgnoreCase) || !uri.AbsolutePath.StartsWith(prefix, StringComparison.Ordinal) ||
                uri.AbsolutePath.Length == prefix.Length || uri.Query.Length != 0 || value.IndexOf('%') >= 0)
                throw Invalid("Release URLs must belong to Hacket2130/helmet-voice-tfar on GitHub Releases.");
            // Check the original spelling too: Uri normalizes literal dot segments.
            string path = value.Substring(value.IndexOf("//", StringComparison.Ordinal) + 2);
            path = path.Substring(path.IndexOf('/'));
            NormalizeRelative(path.TrimStart('/'), false);
        }

        internal static void ValidateNetworkUrl(Uri uri)
        {
            HttpsUri(uri.AbsoluteUri);
            string host = uri.Host.ToLowerInvariant();
            if (host == "github.com")
            {
                if (!uri.AbsolutePath.StartsWith(RepoPath, StringComparison.Ordinal)) throw Invalid("A redirect left the authorized GitHub release repository.");
                return;
            }
            if (host != "release-assets.githubusercontent.com" && host != "objects.githubusercontent.com" && host != "github-releases.githubusercontent.com")
                throw Invalid("A release redirect used an unauthorized host.");
        }

        private static HttpWebResponse OpenHttps(Uri uri)
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            for (int redirects = 0; redirects <= 5; redirects++)
            {
                ValidateNetworkUrl(uri);
                HttpWebRequest request = (HttpWebRequest)WebRequest.Create(uri);
                request.AllowAutoRedirect = false;
                request.Timeout = 30000;
                request.ReadWriteTimeout = 30000;
                request.UserAgent = "HelmetVoiceSetup/1";
                request.Accept = "application/octet-stream, application/json";
                request.AutomaticDecompression = DecompressionMethods.None;
                HttpWebResponse response = (HttpWebResponse)request.GetResponse();
                int status = (int)response.StatusCode;
                if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
                {
                    string location = response.Headers[HttpResponseHeader.Location];
                    response.Dispose();
                    Uri next;
                    if (redirects == 5 || String.IsNullOrEmpty(location) || !Uri.TryCreate(uri, location, out next))
                        throw Invalid("The release server returned too many redirects or an invalid redirect.");
                    ValidateNetworkUrl(next);
                    uri = next;
                    continue;
                }
                if (status != 200) { response.Dispose(); throw Invalid("The release server returned an unexpected HTTP status."); }
                return response;
            }
            throw Invalid("The release server returned too many redirects.");
        }

        internal static void VerifyArchive(string archive, ReleaseManifest manifest)
        {
            ValidateManifest(manifest);
            RequireFile(archive);
            if (new FileInfo(archive).Length != manifest.packageBytes || !EqualHash(HashFile(archive), manifest.sha256))
                throw Invalid("Package verification failed: the size or SHA-256 does not match the release manifest.");
        }

        internal static void ExtractArchive(string archive, string destination)
        {
            destination = NormalizeRoot(destination);
            EnsureNoReparse(destination);
            if (!Directory.Exists(destination) || Directory.GetFileSystemEntries(destination).Length != 0)
                throw Invalid("Archives may only be extracted into a new, empty staging directory.");
            using (ZipArchive zip = ZipFile.OpenRead(archive))
            {
                if (zip.Entries.Count == 0 || zip.Entries.Count > MaxEntries) throw Invalid("The ZIP entry count is outside the allowed range.");
                var paths = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
                var names = new Dictionary<ZipArchiveEntry, string>();
                long total = 0;
                // Validate the entire directory before creating any archive-controlled path.
                foreach (ZipArchiveEntry entry in zip.Entries)
                {
                    bool directory = entry.FullName.EndsWith("/", StringComparison.Ordinal) || entry.FullName.EndsWith("\\", StringComparison.Ordinal);
                    string name = NormalizeRelative(entry.FullName, directory);
                    if (name.Equals(MetadataName, StringComparison.OrdinalIgnoreCase)) throw Invalid("The ZIP contains reserved installer metadata.");
                    int attributes = entry.ExternalAttributes;
                    int unixType = (attributes >> 16) & 0xF000;
                    if ((attributes & (int)FileAttributes.ReparsePoint) != 0 || (unixType != 0 && unixType != 0x8000 && unixType != 0x4000))
                        throw Invalid("Links and special files are not permitted in a release ZIP.");
                    if (unixType == 0x4000 && !directory) throw Invalid("A ZIP directory has an ambiguous name.");
                    if (directory && entry.Length != 0) throw Invalid("A ZIP directory contains file data.");
                    if (paths.ContainsKey(name)) throw Invalid("The ZIP contains duplicate or case-colliding paths.");
                    paths.Add(name, directory);
                    names.Add(entry, name);
                    if (entry.Length < 0 || entry.Length > MaxExpandedBytes - total) throw Invalid("The ZIP expands beyond the allowed size.");
                    total += entry.Length;
                    Child(destination, name);
                }
                foreach (string name in paths.Keys)
                {
                    string parent = name;
                    while (parent.IndexOf('/') >= 0)
                    {
                        parent = parent.Substring(0, parent.LastIndexOf('/'));
                        bool isDirectory;
                        if (paths.TryGetValue(parent, out isDirectory) && !isDirectory) throw Invalid("The ZIP uses a file as a directory.");
                    }
                }
                foreach (ZipArchiveEntry entry in zip.Entries)
                {
                    string name = names[entry];
                    string target = Child(destination, name);
                    if (paths[name]) { SafeCreateDirectory(target); continue; }
                    SafeCreateDirectory(Path.GetDirectoryName(target));
                    EnsureNoReparse(target);
                    using (Stream input = entry.Open())
                    using (FileStream output = new FileStream(target, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                        CopyExact(input, output, entry.Length);
                }
            }
        }

        internal static string NormalizeRelative(string path, bool directory)
        {
            if (String.IsNullOrEmpty(path) || path.Length > 220 || path[0] == '/' || path[0] == '\\' || path.IndexOf(':') >= 0)
                throw Invalid("A package path is absolute, too long, or contains an alternate stream.");
            string normalized = path.Replace('\\', '/');
            if (directory) normalized = normalized.Substring(0, normalized.Length - 1);
            foreach (string part in normalized.Split('/'))
            {
                if (part.Length == 0 || part == "." || part == ".." || part.EndsWith(".", StringComparison.Ordinal) || part.EndsWith(" ", StringComparison.Ordinal))
                    throw Invalid("A package path contains an unsafe segment.");
                foreach (char c in part)
                    if (c < 32 || "<>:\"|?*".IndexOf(c) >= 0) throw Invalid("A package path contains an invalid Windows character.");
                string stem = part.Split('.')[0];
                if (Regex.IsMatch(stem, "^(CON|PRN|AUX|NUL|COM[1-9\u00b9\u00b2\u00b3]|LPT[1-9\u00b9\u00b2\u00b3])$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant))
                    throw Invalid("A package path uses a reserved Windows device name.");
            }
            return normalized;
        }

        private static string ValidateModFiles(string releasePath)
        {
            string validationPath = Child(releasePath, "Validation/manifest.json");
            RequireFile(validationPath);
            FileRecord[] records = ReadJson<FileRecord[]>(validationPath, 4 * 1024 * 1024);
            if (records == null || records.Length == 0 || records.Length > MaxEntries) throw Invalid("The mod validation manifest is empty or too large.");
            string mod = Child(releasePath, "@TFAR_DTR");
            EnsureNoReparse(mod);
            if (!Directory.Exists(mod)) throw Invalid("The package does not contain @TFAR_DTR.");
            var expected = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (FileRecord record in records)
            {
                if (record == null || !IsHash(record.SHA256) || record.Bytes < 0 || record.Bytes > MaxExpandedBytes) throw Invalid("A mod validation record is invalid.");
                string relative = NormalizeRelative(record.Path, false);
                if (!expected.Add(relative)) throw Invalid("The mod validation manifest contains duplicate paths.");
                string file = Child(mod, relative);
                RequireFile(file);
                if (new FileInfo(file).Length != record.Bytes || !EqualHash(HashFile(file), record.SHA256))
                    throw Invalid("Installed file verification failed: " + relative + ". No existing release was replaced.");
            }
            foreach (string required in RequiredModFiles)
                if (!expected.Contains(required)) throw Invalid("The package is missing a required mod file: " + required);
            bool key = false;
            foreach (string path in expected) if (path.StartsWith("keys/", StringComparison.OrdinalIgnoreCase) && path.EndsWith(".bikey", StringComparison.OrdinalIgnoreCase)) key = true;
            if (!key) throw Invalid("The package is missing its Arma signing key.");
            var actual = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            CollectFiles(mod, mod, actual);
            if (!actual.SetEquals(expected)) throw Invalid("The installed mod contains files not listed in its validation manifest.");
            return HashFile(validationPath);
        }

        private static void CollectFiles(string root, string directory, HashSet<string> files)
        {
            EnsureNoReparse(directory);
            foreach (string entry in Directory.GetFileSystemEntries(directory))
            {
                EnsureNoReparse(entry);
                if (Directory.Exists(entry)) CollectFiles(root, entry, files);
                else
                {
                    if (files.Count >= MaxEntries) throw Invalid("The installed mod contains too many files.");
                    files.Add(entry.Substring(root.Length + 1).Replace('\\', '/'));
                }
            }
        }

        private static ReleaseMetadata VerifyRelease(string releasePath, ReleaseManifest expected)
        {
            EnsureNoReparse(releasePath);
            ReleaseMetadata metadata = ReadJson<ReleaseMetadata>(Child(releasePath, MetadataName), MaxManifestBytes);
            if (metadata == null || metadata.schemaVersion != 1 || !IsHash(metadata.validationSha256)) throw Invalid("This release directory has no valid installer metadata. It will not be overwritten.");
            ValidateManifest(metadata.manifest);
            if (expected != null && (metadata.manifest.releaseNumber != expected.releaseNumber || metadata.manifest.packageBytes != expected.packageBytes ||
                !EqualHash(metadata.manifest.sha256, expected.sha256) || metadata.manifest.pluginVersion != expected.pluginVersion))
                throw Invalid("A different package already occupies this release version. It will not be overwritten.");
            string validationPath = Child(releasePath, "Validation/manifest.json");
            RequireFile(validationPath);
            if (!EqualHash(HashFile(validationPath), metadata.validationSha256)) throw Invalid("The release validation manifest has changed.");
            ValidateModFiles(releasePath);
            return metadata;
        }

        private static InstalledRelease Describe(string path, ReleaseManifest manifest)
        {
            return new InstalledRelease {
                ReleaseNumber = manifest.releaseNumber, DisplayVersion = manifest.displayVersion,
                ReleaseDirectory = path, ModDirectory = Child(path, "@TFAR_DTR"),
                PluginInstaller = Child(path, "@TFAR_DTR/teamspeak/task_force_radio_dtr.ts3_plugin")
            };
        }

        private static void WriteState(string root, string releasePath, ReleaseManifest manifest)
        {
            string metadata = Child(releasePath, MetadataName);
            InstalledState state = new InstalledState { schemaVersion = 1, releaseNumber = manifest.releaseNumber, releaseMetadataSha256 = HashFile(metadata) };
            string temporary = Child(root, ".installed-" + Guid.NewGuid().ToString("N") + ".tmp");
            string target = Child(root, "installed.json");
            try
            {
                WriteNewJson(temporary, state);
                EnsureNoReparse(target);
                if (File.Exists(target)) File.Replace(temporary, target, null);
                else File.Move(temporary, target);
            }
            finally
            {
                EnsureNoReparse(temporary);
                if (File.Exists(temporary)) File.Delete(temporary);
            }
        }

        internal static string NormalizeRoot(string root)
        {
            if (String.IsNullOrWhiteSpace(root) || !Regex.IsMatch(root, "\\A[A-Za-z]:[\\\\/]")) throw Invalid("Choose an absolute local installation directory.");
            string full = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            if (full.Length <= Path.GetPathRoot(full).TrimEnd('\\', '/').Length || full.IndexOf(':', 2) >= 0) throw Invalid("The installation directory cannot be a drive root or alternate stream.");
            // Windows trims trailing dots/spaces in normal paths; reject that ambiguity.
            string relative = full.Substring(Path.GetPathRoot(full).Length);
            NormalizeRelative(relative.Replace('\\', '/'), false);
            EnsureNoReparse(full);
            return full;
        }

        private static string ReleasePath(string root, int revision)
        {
            return Child(root, "Releases/R" + revision.ToString(CultureInfo.InvariantCulture));
        }

        internal static string Child(string root, string relative)
        {
            string clean = NormalizeRelative(relative, false).Replace('/', Path.DirectorySeparatorChar);
            string full = Path.GetFullPath(Path.Combine(root, clean));
            if (!full.StartsWith(root.TrimEnd('\\', '/') + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw Invalid("A file path left the installation directory.");
            return full;
        }

        internal static void EnsureNoReparse(string path)
        {
            string current = Path.GetFullPath(path);
            while (!String.IsNullOrEmpty(current))
            {
                // GetAttributes sees directory junctions and dangling links more reliably than Exists.
                try
                {
                    if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw Invalid("Installation paths cannot contain symbolic links or directory junctions: " + current);
                }
                catch (FileNotFoundException) { }
                catch (DirectoryNotFoundException) { }
                current = Path.GetDirectoryName(current);
            }
        }

        private static void SafeCreateDirectory(string path)
        {
            EnsureNoReparse(path);
            Directory.CreateDirectory(path);
            EnsureNoReparse(path);
        }

        private static void RequireFile(string path)
        {
            EnsureNoReparse(path);
            if (!File.Exists(path)) throw Invalid("A required release file is missing: " + path);
        }

        private static T ReadJson<T>(string path, int limit)
        {
            RequireFile(path);
            try
            {
                using (Stream stream = File.OpenRead(path))
                    return Serializer().Deserialize<T>(new UTF8Encoding(false, true).GetString(ReadBounded(stream, limit)).TrimStart('\uFEFF'));
            }
            catch (InvalidDataException) { throw; }
            catch (Exception ex) { throw Invalid("Invalid release metadata in " + Path.GetFileName(path) + ": " + ex.Message); }
        }

        private static void WriteNewJson(string path, object value)
        {
            EnsureNoReparse(path);
            byte[] bytes = new UTF8Encoding(false).GetBytes(Serializer().Serialize(value));
            using (FileStream output = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                output.Write(bytes, 0, bytes.Length);
                output.Flush(true);
            }
        }

        internal static byte[] ReadBounded(Stream input, int limit)
        {
            using (MemoryStream result = new MemoryStream())
            {
                byte[] buffer = new byte[8192];
                int count;
                while ((count = input.Read(buffer, 0, buffer.Length)) != 0)
                {
                    if (result.Length + count > limit) throw Invalid("The manifest exceeds the permitted size.");
                    result.Write(buffer, 0, count);
                }
                return result.ToArray();
            }
        }

        private static void CopyExact(Stream input, Stream output, long expected)
        {
            byte[] buffer = new byte[65536];
            long copied = 0;
            int count;
            while ((count = input.Read(buffer, 0, buffer.Length)) != 0)
            {
                if (count > expected - copied) throw Invalid("A download or archive entry exceeded its declared size.");
                output.Write(buffer, 0, count);
                copied += count;
            }
            if (copied != expected) throw Invalid("A download or archive entry ended before its declared size.");
        }

        internal static string HashFile(string path)
        {
            EnsureNoReparse(path);
            using (Stream stream = File.OpenRead(path))
            using (SHA256 algorithm = SHA256.Create())
                return BitConverter.ToString(algorithm.ComputeHash(stream)).Replace("-", "");
        }

        private static bool IsHash(string hash) { return hash != null && Regex.IsMatch(hash, "\\A[0-9a-fA-F]{64}\\z"); }
        private static bool EqualHash(string a, string b) { return String.Equals(a, b, StringComparison.OrdinalIgnoreCase); }
        private static InvalidDataException Invalid(string message) { return new InvalidDataException(message); }
        private static void Report(Action<string> progress, string message) { if (progress != null) progress(message); }

        private static Stream Resource(string name)
        {
            Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(name);
            if (stream == null) throw Invalid("The installer is missing its bundled resource: " + name);
            return stream;
        }

        private static void CleanupStage(string stagingParent, string stage)
        {
            try
            {
                // Only our freshly generated direct child can be removed. A changed/reparse
                // tree is left for inspection instead of risking another location's files.
                string name = Path.GetFileName(stage);
                Guid id;
                if (!Guid.TryParseExact(name, "N", out id) || !String.Equals(Child(stagingParent, name), stage, StringComparison.OrdinalIgnoreCase)) return;
                EnsureNoReparse(stage);
                if (!Directory.Exists(stage)) return;
                VerifyNoLinksRecursively(stage);
                DeleteTreeWithoutFollowingLinks(stage);
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }

        private static void VerifyNoLinksRecursively(string path)
        {
            EnsureNoReparse(path);
            foreach (string child in Directory.GetFileSystemEntries(path))
            {
                EnsureNoReparse(child);
                if (Directory.Exists(child)) VerifyNoLinksRecursively(child);
            }
        }

        private static void DeleteTreeWithoutFollowingLinks(string path)
        {
            EnsureNoReparse(path);
            foreach (string child in Directory.GetFileSystemEntries(path))
            {
                EnsureNoReparse(child);
                if (Directory.Exists(child)) DeleteTreeWithoutFollowingLinks(child);
                else File.Delete(child);
            }
            Directory.Delete(path, false);
        }
    }
}
