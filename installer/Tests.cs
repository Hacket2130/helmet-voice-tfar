using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Web.Script.Serialization;

namespace HelmetVoiceSetup
{
    // All generated test artifacts remain in one unique child of the supplied workspace
    // work directory. No live TeamSpeak, Arma, Steam, registry, or network calls are used.
    internal static class InstallerTests
    {
        private static int passed;
        private static int artifactNumber;
        private static string tests;
        private static readonly JavaScriptSerializer Json = new JavaScriptSerializer();

        public static int Main(string[] args)
        {
            try
            {
                if (args.Length != 1) throw new ArgumentException("Usage: InstallerTests.exe <absolute workspace work test root>");
                string workspace = Path.GetFullPath(Environment.CurrentDirectory).TrimEnd('\\', '/');
                string work = Path.Combine(workspace, "work");
                if (!File.Exists(Path.Combine(workspace, "installer", "Core.cs")) || !File.Exists(Path.Combine(workspace, "outputs", "HelmetVoice-TFAR-R11.zip")))
                    throw new ArgumentException("Run tests with the Helmet Voice workspace as the current directory.");
                string requested = InstallerCore.NormalizeRoot(args[0]);
                if (!requested.StartsWith(work + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                    throw new ArgumentException("The test root must be inside this workspace's work directory.");
                InstallerCore.EnsureNoReparse(requested);
                tests = Path.Combine(requested, "run-" + Guid.NewGuid().ToString("N").Substring(0, 8));
                if (Directory.Exists(tests) || File.Exists(tests)) throw new IOException("Test artifact collision; retry with a fresh test root.");
                Directory.CreateDirectory(tests);
                ManifestTests();
                ArchiveTests();
                TransactionTests();
                BundleTests();
                Console.WriteLine("PASS: " + passed + " assertions. Artifacts: " + tests);
                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine("FAIL after " + passed + " assertions: " + ex);
                return 1;
            }
        }

        private static void ManifestTests()
        {
            ReleaseManifest bundled = InstallerCore.LoadBundledManifest();
            Check(bundled.releaseNumber == 11 && bundled.displayVersion == "R11", "Bundled revision");
            Check(bundled.packageBytes == 54021360 && bundled.sha256.Equals("711223E98346B869C3F544BB8CE2CF04E9563436EC62C4B1F15482A13F6919AC", StringComparison.OrdinalIgnoreCase), "Approved bundle identity");
            Check(InstallerCore.IsNewer(bundled, null), "No installation permits install");
            Check(!InstallerCore.IsNewer(bundled, new InstalledRelease { ReleaseNumber = 11 }), "Same revision is current");
            Check(!InstallerCore.IsNewer(bundled, new InstalledRelease { ReleaseNumber = 12 }), "Older revision is not newer");
            Check(InstallerCore.IsNewer(bundled, new InstalledRelease { ReleaseNumber = 9 }), "Numeric revision comparison");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.product = "another-product"; }, "Wrong product");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.schemaVersion = 2; }, "Unknown schema");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.releaseNumber = 0; }, "Zero revision");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.displayVersion = "../../bad"; }, "Unsafe version");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.pluginVersion = "bad\r\nversion"; }, "Unsafe plugin version");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.pluginVersion = "1.0\n"; }, "Trailing newline in plugin version");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.sha256 = new string('a', 63); }, "Short hash");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.sha256 = new string('z', 64); }, "Nonhex hash");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.packageBytes = InstallerCore.MaxPackageBytes + 1; }, "Oversized package");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.packageBytes = 31; }, "Tiny package");
            string[] badUrls = {
                "http://github.com/Hacket2130/helmet-voice-tfar/releases/download/R11/test.zip",
                "https://github.com.evil.example/Hacket2130/helmet-voice-tfar/releases/download/R11/test.zip",
                "https://github.com/another/helmet-voice-tfar/releases/download/R11/test.zip",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/../download/R11/test.zip",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R11/%2e%2e/test.zip",
                "https://user@github.com/Hacket2130/helmet-voice-tfar/releases/download/R11/test.zip",
                "https://github.com:444/Hacket2130/helmet-voice-tfar/releases/download/R11/test.zip",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R11/test.zip#fragment",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R11/test.exe"
            };
            foreach (string badUrl in badUrls)
                BadManifest(bundled, delegate(ReleaseManifest m) { m.packageUrl = badUrl; }, "Unauthorized package URL");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.releasePageUrl = "https://example.com/release"; }, "Unauthorized release page");
            string serialized = Json.Serialize(bundled);
            Reject(delegate { InstallerCore.ParseManifest(Encoding.UTF8.GetBytes(serialized.Replace("\"releaseNumber\":11", "\"releaseNumber\":11.5"))); }, "Fractional revision");
            Reject(delegate { InstallerCore.ParseManifest(Encoding.UTF8.GetBytes(serialized.Replace("\"releaseNumber\":11", "\"releaseNumber\":\"11\""))); }, "String revision");
            Reject(delegate { InstallerCore.ParseManifest(new byte[InstallerCore.MaxManifestBytes + 1]); }, "Large feed");
            Reject(delegate { InstallerCore.ParseManifest(new byte[] { 255, 255, 255 }); }, "Invalid UTF8");
            Reject(delegate { InstallerCore.ParseManifest(Encoding.UTF8.GetBytes("[]")); }, "Wrong JSON root");
            Reject(delegate { using (var input = new MemoryStream(new byte[65])) InstallerCore.ReadBounded(input, 64); }, "Bounded manifest stream");
            InstallerCore.ValidateNetworkUrl(new Uri("https://release-assets.githubusercontent.com/asset?signed=1"));
            Check(true, "GitHub release CDN accepted");
            Reject(delegate { InstallerCore.ValidateNetworkUrl(new Uri("http://release-assets.githubusercontent.com/asset")); }, "HTTP redirect rejected");
            Reject(delegate { InstallerCore.ValidateNetworkUrl(new Uri("https://example.com/asset")); }, "Foreign redirect rejected");
            Reject(delegate { InstallerCore.ValidateNetworkUrl(new Uri("https://github.com/another/repo/releases/download/R11/file.zip")); }, "Foreign repository redirect rejected");
        }

        private static void ArchiveTests()
        {
            string[] unsafePaths = {
                "../escape.txt", "folder/../../escape.txt", "folder\\..\\escape.txt", "/absolute.txt", "C:/absolute.txt",
                "\\\\server\\share\\file", "file:stream", "folder/file:stream", "CON", "nul.txt", "folder/LPT1.txt", "COM1",
                "COM\u00b9.txt", "folder/PRN.txt", "folder/./file", "folder//file", "folder /file", "folder./file", "a|b", "a?b", "folder//"
            };
            foreach (string unsafePath in unsafePaths)
            {
                if (unsafePath.IndexOf('|') >= 0)
                    Reject(delegate { InstallerCore.NormalizeRelative(unsafePath, false); }, "Invalid Windows path character");
                else BadZip(new string[] { unsafePath }, 0, "Unsafe archive path: " + unsafePath);
            }
            BadZip(new string[] { "same.txt", "SAME.TXT" }, 0, "Case duplicate");
            BadZip(new string[] { "same.txt", "same.txt" }, 0, "Exact duplicate");
            BadZip(new string[] { "folder", "folder/file" }, 0, "File-directory collision");
            BadZip(new string[] { ".helmet-voice-release.json" }, 0, "Reserved metadata");
            BadZip(new string[] { "symlink" }, unchecked((int)0xA1FF0000), "Unix symbolic link");
            BadZip(new string[] { "reparse" }, (int)FileAttributes.ReparsePoint, "Windows reparse entry");
            BadZip(new string[] { "device" }, unchecked((int)0x21B60000), "Unix device");
            var tooMany = new string[InstallerCore.MaxEntries + 1];
            for (int i = 0; i < tooMany.Length; i++) tooMany[i] = "f" + i;
            BadZip(tooMany, 0, "ZIP entry count limit");
            string expanded = Unique("expanded.zip");
            Zip(expanded, new string[] { "large.bin" }, 0);
            byte[] expandedBytes = File.ReadAllBytes(expanded);
            for (int i = 0; i <= expandedBytes.Length - 24; i++)
                if (expandedBytes[i] == 0x50 && expandedBytes[i + 1] == 0x4b && expandedBytes[i + 2] == 0x01 && expandedBytes[i + 3] == 0x02)
                {
                    // Central-directory uncompressed size: one byte beyond our 2 GiB cap.
                    Array.Copy(BitConverter.GetBytes((uint)InstallerCore.MaxExpandedBytes + 1), 0, expandedBytes, i + 24, 4);
                    break;
                }
            File.WriteAllBytes(expanded, expandedBytes);
            string expandedDestination = Unique("expanded-extract");
            Directory.CreateDirectory(expandedDestination);
            Reject(delegate { InstallerCore.ExtractArchive(expanded, expandedDestination); }, "ZIP expanded-size limit");
            Check(Directory.GetFileSystemEntries(expandedDestination).Length == 0, "Oversized ZIP rejected before extraction");
            string archive = Unique("valid.zip");
            Zip(archive, new string[] { "folder/", "folder/file.txt" }, 0);
            string destination = Unique("valid-extract");
            Directory.CreateDirectory(destination);
            InstallerCore.ExtractArchive(archive, destination);
            Check(File.ReadAllText(Path.Combine(destination, "folder", "file.txt")) == "test", "Valid ZIP extraction");
            Reject(delegate { InstallerCore.ExtractArchive(archive, destination); }, "Extraction cannot overwrite existing files");
            Reject(delegate { InstallerCore.NormalizeRoot("C:\\"); }, "Drive-root install rejected");
            Reject(delegate { InstallerCore.NormalizeRoot("relative"); }, "Relative install rejected");
            Reject(delegate { InstallerCore.NormalizeRoot("C:relative"); }, "Drive-relative install rejected");
            Reject(delegate { InstallerCore.Child(tests, "../escape"); }, "Containment guard");
        }

        private static void TransactionTests()
        {
            string root = Unique("transaction");
            string archive11 = MakePackage(11, false);
            ReleaseManifest r11 = PackageManifest(archive11, 11);
            Check(InstallerCore.ReadState(root) == null, "Missing state is uninstalled");
            InstalledRelease installed = InstallerCore.InstallArchive(root, r11, archive11, null);
            Check(installed.ReleaseNumber == 11 && File.Exists(installed.PluginInstaller), "Install valid package");
            string originalState = File.ReadAllText(Path.Combine(root, "installed.json"));
            DateTime stamp = File.GetLastWriteTimeUtc(installed.PluginInstaller);
            InstalledRelease repeated = InstallerCore.InstallArchive(root, r11, archive11, null);
            Check(repeated.ReleaseDirectory == installed.ReleaseDirectory && File.GetLastWriteTimeUtc(installed.PluginInstaller) == stamp, "Idempotent installation preserves files");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState, "Idempotent installation preserves state");

            string archive12 = MakePackage(12, false);
            ReleaseManifest r12 = PackageManifest(archive12, 12);
            ReleaseManifest badHash = Clone(r12);
            badHash.sha256 = new string('0', 64);
            Reject(delegate { InstallerCore.InstallArchive(root, badHash, archive12, null); }, "Bad package hash");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState, "Bad hash preserves state");
            ReleaseManifest tooSmall = Clone(r12);
            tooSmall.packageBytes--;
            Reject(delegate { InstallerCore.InstallArchive(root, tooSmall, archive12, null); }, "Oversized download");
            ReleaseManifest tooLarge = Clone(r12);
            tooLarge.packageBytes++;
            Reject(delegate { InstallerCore.InstallArchive(root, tooLarge, archive12, null); }, "Truncated download");
            string incomplete = MakePackage(12, true);
            ReleaseManifest incompleteManifest = PackageManifest(incomplete, 12);
            Reject(delegate { InstallerCore.InstallArchive(root, incompleteManifest, incomplete, null); }, "Incomplete package rejected");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState && !Directory.Exists(Path.Combine(root, "Releases", "R12")), "Incomplete install preserves state and release history");

            // A real File.Replace failure is injected by a read handle that denies deletion.
            using (FileStream lockedState = new FileStream(Path.Combine(root, "installed.json"), FileMode.Open, FileAccess.Read, FileShare.Read))
                Reject(delegate { InstallerCore.InstallArchive(root, r12, archive12, null); }, "Atomic state replacement failure");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState, "State failure leaves previous current release intact");
            Check(Directory.Exists(Path.Combine(root, "Releases", "R12")), "Verified promoted release retained for retry");
            InstalledRelease updated = InstallerCore.InstallArchive(root, r12, archive12, null);
            Check(updated.ReleaseNumber == 12 && InstallerCore.ReadState(root).ReleaseNumber == 12, "Retry selects verified promoted release");
            Check(File.Exists(installed.PluginInstaller), "Earlier release retained after update");
            InstalledRelease noDowngrade = InstallerCore.InstallArchive(root, r11, archive11, null);
            Check(noDowngrade.ReleaseNumber == 12 && InstallerCore.ReadState(root).ReleaseNumber == 12, "Older installer cannot downgrade current release");
            ReleaseManifest conflict = Clone(r12);
            conflict.sha256 = new string('1', 64);
            Reject(delegate { InstallerCore.InstallArchive(root, conflict, archive12, null); }, "Same revision different archive conflicts");
            string currentState = File.ReadAllText(Path.Combine(root, "installed.json"));
            File.AppendAllText(updated.PluginInstaller, "tampered");
            Reject(delegate { InstallerCore.ReadState(root); }, "Modified installed file detected");
            Reject(delegate { InstallerCore.InstallArchive(root, r12, archive12, null); }, "Tampered release never clobbered");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == currentState && File.ReadAllText(updated.PluginInstaller).EndsWith("tampered", StringComparison.Ordinal), "Tamper failure preserves state and existing files");

            string unlistedRoot = Unique("unlisted");
            InstalledRelease unlisted = InstallerCore.InstallArchive(unlistedRoot, r11, archive11, null);
            File.WriteAllText(Path.Combine(unlisted.ModDirectory, "unexpected.dll"), "unlisted");
            Reject(delegate { InstallerCore.ReadState(unlistedRoot); }, "Unlisted mod file detected");

            string changedManifestRoot = Unique("changed-manifest");
            InstalledRelease changed = InstallerCore.InstallArchive(changedManifestRoot, r11, archive11, null);
            File.AppendAllText(Path.Combine(changed.ReleaseDirectory, "Validation", "manifest.json"), " ");
            Reject(delegate { InstallerCore.ReadState(changedManifestRoot); }, "Modified validation manifest detected");

            string badStateRoot = Unique("bad-state");
            Directory.CreateDirectory(badStateRoot);
            File.WriteAllText(Path.Combine(badStateRoot, "installed.json"), "{\"schemaVersion\":1,\"releaseNumber\":-1,\"releaseMetadataSha256\":\"bad\"}");
            Reject(delegate { InstallerCore.ReadState(badStateRoot); }, "Invalid state rejected");
            Check(Directory.GetDirectories(Path.Combine(root, ".staging")).Length == 0, "Failed staging transactions cleaned safely");
        }

        private static void BundleTests()
        {
            string root = Unique("actual-bundle");
            InstalledRelease bundled = InstallerCore.InstallBundled(root, delegate(string message) { Console.WriteLine(message); });
            Check(bundled.ReleaseNumber == 11 && Directory.Exists(bundled.ModDirectory), "Actual approved R11 bundle installed in sandbox");
            Check(InstallerCore.HashFile(bundled.PluginInstaller) == "31AD957291A969FB29F08910FC67C907B6B3F6069EE505A8131F78C0C962AC69", "Actual TeamSpeak installer digest");
            string before = File.ReadAllText(Path.Combine(root, "installed.json"));
            Check(InstallerCore.InstallBundled(root, null).ReleaseDirectory == bundled.ReleaseDirectory, "Actual bundle idempotency");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == before, "Actual bundle state unchanged on repeat");
            Check(InstallerCore.ReadState(root).PluginInstaller == bundled.PluginInstaller, "Actual bundle state verification");
        }

        private static string MakePackage(int revision, bool incomplete)
        {
            string archive = Unique("package-" + revision + ".zip");
            var records = new List<object>();
            string[] files = {
                "addons/tfar_core.pbo", "addons/dtr_core.pbo", "task_force_radio_pipe.dll", "task_force_radio_pipe_x64.dll",
                "teamspeak/task_force_radio_dtr.ts3_plugin", "keys/test.bikey"
            };
            using (FileStream output = File.Create(archive))
            using (ZipArchive zip = new ZipArchive(output, ZipArchiveMode.Create))
            {
                for (int i = 0; i < files.Length; i++)
                {
                    if (incomplete && i == 0) continue;
                    byte[] bytes = Encoding.UTF8.GetBytes("test release " + revision + " / " + files[i]);
                    string hash;
                    using (var sha = System.Security.Cryptography.SHA256.Create()) hash = BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "");
                    records.Add(new { Path = files[i].Replace('/', '\\'), SHA256 = hash, Bytes = bytes.Length });
                    using (Stream entry = zip.CreateEntry("@TFAR_DTR/" + files[i]).Open()) entry.Write(bytes, 0, bytes.Length);
                }
                byte[] manifest = Encoding.UTF8.GetBytes(Json.Serialize(records));
                using (Stream entry = zip.CreateEntry("Validation/manifest.json").Open()) entry.Write(manifest, 0, manifest.Length);
            }
            return archive;
        }

        private static ReleaseManifest PackageManifest(string archive, int revision)
        {
            ReleaseManifest manifest = Clone(InstallerCore.LoadBundledManifest());
            manifest.releaseNumber = revision;
            manifest.displayVersion = "R" + revision;
            manifest.pluginVersion = "1.0.341-dtr." + revision;
            manifest.packageBytes = new FileInfo(archive).Length;
            manifest.sha256 = InstallerCore.HashFile(archive);
            manifest.packageUrl = "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R" + revision + "/package.zip";
            manifest.releasePageUrl = "https://github.com/Hacket2130/helmet-voice-tfar/releases/tag/R" + revision;
            return manifest;
        }

        private static ReleaseManifest Clone(ReleaseManifest manifest) { return Json.Deserialize<ReleaseManifest>(Json.Serialize(manifest)); }
        private static string Unique(string name) { return Path.Combine(tests, "t" + (++artifactNumber).ToString("D3") + Path.GetExtension(name)); }

        private static void BadManifest(ReleaseManifest manifest, Action<ReleaseManifest> change, string label)
        {
            ReleaseManifest candidate = Clone(manifest);
            change(candidate);
            Reject(delegate { InstallerCore.ParseManifest(Encoding.UTF8.GetBytes(Json.Serialize(candidate))); }, label);
        }

        private static void BadZip(string[] paths, int attributes, string label)
        {
            string archive = Unique("malicious.zip");
            Zip(archive, paths, attributes);
            string destination = Unique("malicious-extract");
            Directory.CreateDirectory(destination);
            Reject(delegate { InstallerCore.ExtractArchive(archive, destination); }, label);
            Check(Directory.GetFileSystemEntries(destination).Length == 0, "Unsafe ZIP rejected before extraction");
        }

        private static void Zip(string archive, string[] paths, int attributes)
        {
            using (FileStream output = File.Create(archive))
            using (ZipArchive zip = new ZipArchive(output, ZipArchiveMode.Create))
                foreach (string path in paths)
                {
                    ZipArchiveEntry entry = zip.CreateEntry(path);
                    entry.ExternalAttributes = attributes;
                    if (!path.EndsWith("/", StringComparison.Ordinal) && !path.EndsWith("\\", StringComparison.Ordinal))
                        using (StreamWriter writer = new StreamWriter(entry.Open(), new UTF8Encoding(false))) writer.Write("test");
                }
        }

        private static void Check(bool condition, string label)
        {
            if (!condition) throw new Exception("Assertion failed: " + label);
            passed++;
        }

        private static void Reject(Action action, string label)
        {
            try { action(); }
            catch (InvalidDataException) { passed++; return; }
            catch (IOException) { passed++; return; }
            catch (UnauthorizedAccessException) { passed++; return; }
            throw new Exception("Expected rejection: " + label);
        }
    }
}
