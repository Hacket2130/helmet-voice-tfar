using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Web.Script.Serialization;

namespace VanguardAddonSetup
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
                if (!File.Exists(Path.Combine(workspace, "standalone", "installer", "Core.cs")))
                    throw new ArgumentException("Run tests with the Vanguard addon workspace as the current directory.");
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
            Check(bundled.releaseNumber == 12 && bundled.displayVersion == "R12", "Bundled revision");
            Check(bundled.schemaVersion == 2 && bundled.product == "task-force-vanguard-tfar-addon" && bundled.pluginVersion == "0.12.0", "Standalone bundle identity");
            Check(InstallerCore.IsNewer(bundled, null), "No installation permits install");
            Check(!InstallerCore.IsNewer(bundled, new InstalledRelease { ReleaseNumber = 12 }), "Same revision is current");
            Check(!InstallerCore.IsNewer(bundled, new InstalledRelease { ReleaseNumber = 13 }), "Older revision is not newer");
            Check(InstallerCore.IsNewer(bundled, new InstalledRelease { ReleaseNumber = 9 }), "Numeric revision comparison");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.product = "another-product"; }, "Wrong product");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.schemaVersion = 1; }, "Legacy replacement schema rejected");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.schemaVersion = 3; }, "Unknown schema");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.product = "helmet-voice-tfar"; }, "Legacy replacement product rejected");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.releaseNumber = 0; }, "Zero revision");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.displayVersion = "../../bad"; }, "Unsafe version");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.pluginVersion = "bad\r\nversion"; }, "Unsafe plugin version");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.pluginVersion = "1.0\n"; }, "Trailing newline in plugin version");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.sha256 = new string('a', 63); }, "Short hash");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.sha256 = new string('z', 64); }, "Nonhex hash");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.packageBytes = InstallerCore.MaxPackageBytes + 1; }, "Oversized package");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.packageBytes = 31; }, "Tiny package");
            string[] badUrls = {
                "http://github.com/Hacket2130/helmet-voice-tfar/releases/download/R12/test.zip",
                "https://github.com.evil.example/Hacket2130/helmet-voice-tfar/releases/download/R12/test.zip",
                "https://github.com/another/helmet-voice-tfar/releases/download/R12/test.zip",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/../download/R12/test.zip",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R12/%2e%2e/test.zip",
                "https://user@github.com/Hacket2130/helmet-voice-tfar/releases/download/R12/test.zip",
                "https://github.com:444/Hacket2130/helmet-voice-tfar/releases/download/R12/test.zip",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R12/test.zip#fragment",
                "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R12/test.exe"
            };
            foreach (string badUrl in badUrls)
                BadManifest(bundled, delegate(ReleaseManifest m) { m.packageUrl = badUrl; }, "Unauthorized package URL");
            BadManifest(bundled, delegate(ReleaseManifest m) { m.releasePageUrl = "https://example.com/release"; }, "Unauthorized release page");
            string serialized = Json.Serialize(bundled);
            Reject(delegate { InstallerCore.ParseManifest(Encoding.UTF8.GetBytes(serialized.Replace("\"releaseNumber\":12", "\"releaseNumber\":12.5"))); }, "Fractional revision");
            Reject(delegate { InstallerCore.ParseManifest(Encoding.UTF8.GetBytes(serialized.Replace("\"releaseNumber\":12", "\"releaseNumber\":\"12\""))); }, "String revision");
            Reject(delegate { InstallerCore.ParseManifest(new byte[InstallerCore.MaxManifestBytes + 1]); }, "Large feed");
            Reject(delegate { InstallerCore.ParseManifest(new byte[] { 255, 255, 255 }); }, "Invalid UTF8");
            Reject(delegate { InstallerCore.ParseManifest(Encoding.UTF8.GetBytes("[]")); }, "Wrong JSON root");
            Reject(delegate { using (var input = new MemoryStream(new byte[65])) InstallerCore.ReadBounded(input, 64); }, "Bounded manifest stream");
            InstallerCore.ValidateNetworkUrl(new Uri("https://release-assets.githubusercontent.com/asset?signed=1"));
            Check(true, "GitHub release CDN accepted");
            Reject(delegate { InstallerCore.ValidateNetworkUrl(new Uri("http://release-assets.githubusercontent.com/asset")); }, "HTTP redirect rejected");
            Reject(delegate { InstallerCore.ValidateNetworkUrl(new Uri("https://example.com/asset")); }, "Foreign redirect rejected");
            Reject(delegate { InstallerCore.ValidateNetworkUrl(new Uri("https://github.com/another/repo/releases/download/R12/file.zip")); }, "Foreign repository redirect rejected");
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
            BadZip(new string[] { ".tfv-addon-release.json" }, 0, "Reserved metadata");
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
            Reject(delegate { InstallerCore.ValidateAddonFileName("addons/tfar_core.pbo"); }, "Stock TFAR PBO rejected");
            Reject(delegate { InstallerCore.ValidateAddonFileName("task_force_radio_pipe_x64.dll"); }, "Stock TFAR pipe rejected");
            Reject(delegate { InstallerCore.ValidateAddonFileName("teamspeak/task_force_radio.ts3_plugin"); }, "Stock TFAR package rejected");
            string collision = Unique("stock-collision.ts3_plugin");
            File.WriteAllBytes(collision, CompanionPackage(true));
            Reject(delegate { InstallerCore.ValidateCompanionPackage(collision); }, "Nested TeamSpeak stock DLL collision rejected");
        }

        private static void TransactionTests()
        {
            string root = Unique("transaction");
            string archiveOld = MakePackage(12, false);
            ReleaseManifest oldRelease = PackageManifest(archiveOld, 12);
            Check(InstallerCore.ReadState(root) == null, "Missing state is uninstalled");
            InstalledRelease installed = InstallerCore.InstallArchive(root, oldRelease, archiveOld, null);
            Check(installed.ReleaseNumber == 12 && File.Exists(installed.PluginInstaller), "Install valid package");
            string originalState = File.ReadAllText(Path.Combine(root, "installed.json"));
            DateTime stamp = File.GetLastWriteTimeUtc(installed.PluginInstaller);
            InstalledRelease repeated = InstallerCore.InstallArchive(root, oldRelease, archiveOld, null);
            Check(repeated.ReleaseDirectory == installed.ReleaseDirectory && File.GetLastWriteTimeUtc(installed.PluginInstaller) == stamp, "Idempotent installation preserves files");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState, "Idempotent installation preserves state");

            string archiveNew = MakePackage(13, false);
            ReleaseManifest newRelease = PackageManifest(archiveNew, 13);
            ReleaseManifest badHash = Clone(newRelease);
            badHash.sha256 = new string('0', 64);
            Reject(delegate { InstallerCore.InstallArchive(root, badHash, archiveNew, null); }, "Bad package hash");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState, "Bad hash preserves state");
            ReleaseManifest tooSmall = Clone(newRelease);
            tooSmall.packageBytes--;
            Reject(delegate { InstallerCore.InstallArchive(root, tooSmall, archiveNew, null); }, "Oversized download");
            ReleaseManifest tooLarge = Clone(newRelease);
            tooLarge.packageBytes++;
            Reject(delegate { InstallerCore.InstallArchive(root, tooLarge, archiveNew, null); }, "Truncated download");
            string incomplete = MakePackage(13, true);
            ReleaseManifest incompleteManifest = PackageManifest(incomplete, 13);
            Reject(delegate { InstallerCore.InstallArchive(root, incompleteManifest, incomplete, null); }, "Incomplete package rejected");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState && !Directory.Exists(Path.Combine(root, "Releases", "R13")), "Incomplete install preserves state and release history");

            // A real File.Replace failure is injected by a read handle that denies deletion.
            using (FileStream lockedState = new FileStream(Path.Combine(root, "installed.json"), FileMode.Open, FileAccess.Read, FileShare.Read))
                Reject(delegate { InstallerCore.InstallArchive(root, newRelease, archiveNew, null); }, "Atomic state replacement failure");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == originalState, "State failure leaves previous current release intact");
            Check(Directory.Exists(Path.Combine(root, "Releases", "R13")), "Verified promoted release retained for retry");
            InstalledRelease updated = InstallerCore.InstallArchive(root, newRelease, archiveNew, null);
            Check(updated.ReleaseNumber == 13 && InstallerCore.ReadState(root).ReleaseNumber == 13, "Retry selects verified promoted release");
            Check(File.Exists(installed.PluginInstaller), "Earlier release retained after update");
            InstalledRelease noDowngrade = InstallerCore.InstallArchive(root, oldRelease, archiveOld, null);
            Check(noDowngrade.ReleaseNumber == 13 && InstallerCore.ReadState(root).ReleaseNumber == 13, "Older installer cannot downgrade current release");
            ReleaseManifest conflict = Clone(newRelease);
            conflict.sha256 = new string('1', 64);
            Reject(delegate { InstallerCore.InstallArchive(root, conflict, archiveNew, null); }, "Same revision different archive conflicts");
            string currentState = File.ReadAllText(Path.Combine(root, "installed.json"));
            File.AppendAllText(updated.PluginInstaller, "tampered");
            Reject(delegate { InstallerCore.ReadState(root); }, "Modified installed file detected");
            Reject(delegate { InstallerCore.InstallArchive(root, newRelease, archiveNew, null); }, "Tampered release never clobbered");
            Check(File.ReadAllText(Path.Combine(root, "installed.json")) == currentState && File.ReadAllText(updated.PluginInstaller).EndsWith("tampered", StringComparison.Ordinal), "Tamper failure preserves state and existing files");

            string unlistedRoot = Unique("unlisted");
            InstalledRelease unlisted = InstallerCore.InstallArchive(unlistedRoot, oldRelease, archiveOld, null);
            File.WriteAllText(Path.Combine(unlisted.ModDirectory, "unexpected.dll"), "unlisted");
            Reject(delegate { InstallerCore.ReadState(unlistedRoot); }, "Unlisted mod file detected");

            string changedManifestRoot = Unique("changed-manifest");
            InstalledRelease changed = InstallerCore.InstallArchive(changedManifestRoot, oldRelease, archiveOld, null);
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
            Check(bundled.ReleaseNumber == 12 && Directory.Exists(bundled.ModDirectory), "Actual standalone R12 bundle installed in sandbox");
            Check(Path.GetFileName(bundled.ModDirectory) == "@TFV_TFAR_Addon" && !File.Exists(Path.Combine(bundled.ModDirectory, "addons", "tfar_core.pbo")), "Standalone addon keeps stock TFAR separate");
            InstallerCore.ValidateCompanionPackage(bundled.PluginInstaller);
            Check(true, "Actual TeamSpeak companion has no stock DLL collisions");
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
                "addons/tfv_core.pbo", "tfv_helmet_voice_x64.dll",
                "teamspeak/task_force_vanguard.ts3_plugin", "keys/test.bikey"
            };
            using (FileStream output = File.Create(archive))
            using (ZipArchive zip = new ZipArchive(output, ZipArchiveMode.Create))
            {
                for (int i = 0; i < files.Length; i++)
                {
                    if (incomplete && i == 0) continue;
                    byte[] bytes = files[i].EndsWith(".ts3_plugin", StringComparison.Ordinal) ? CompanionPackage(false) : Encoding.UTF8.GetBytes("test release " + revision + " / " + files[i]);
                    string hash;
                    using (var sha = System.Security.Cryptography.SHA256.Create()) hash = BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "");
                    records.Add(new { Path = files[i].Replace('/', '\\'), SHA256 = hash, Bytes = bytes.Length });
                    using (Stream entry = zip.CreateEntry("@TFV_TFAR_Addon/" + files[i]).Open()) entry.Write(bytes, 0, bytes.Length);
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
            manifest.pluginVersion = "0." + revision + ".0";
            manifest.packageBytes = new FileInfo(archive).Length;
            manifest.sha256 = InstallerCore.HashFile(archive);
            manifest.packageUrl = "https://github.com/Hacket2130/helmet-voice-tfar/releases/download/R" + revision + "/package.zip";
            manifest.releasePageUrl = "https://github.com/Hacket2130/helmet-voice-tfar/releases/tag/R" + revision;
            return manifest;
        }

        private static ReleaseManifest Clone(ReleaseManifest manifest) { return Json.Deserialize<ReleaseManifest>(Json.Serialize(manifest)); }

        private static byte[] CompanionPackage(bool stockCollision)
        {
            using (var bytes = new MemoryStream())
            {
                using (var zip = new ZipArchive(bytes, ZipArchiveMode.Create, true))
                {
                    using (var writer = new StreamWriter(zip.CreateEntry("package.ini").Open(), new UTF8Encoding(false)))
                        writer.Write("[Package]\nName = Task force Vanguard TFAR addon\nType = Plugin\nVersion = 0.12.0\n");
                    using (var writer = new StreamWriter(zip.CreateEntry("plugins/task_force_vanguard_win64.dll").Open(), new UTF8Encoding(false))) writer.Write("test companion");
                    if (stockCollision)
                        using (var writer = new StreamWriter(zip.CreateEntry("plugins/task_force_radio_win64.dll").Open(), new UTF8Encoding(false))) writer.Write("must reject");
                }
                return bytes.ToArray();
            }
        }
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
