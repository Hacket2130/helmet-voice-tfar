using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Web.Script.Serialization;

// Opt-in network acceptance check, run only after R12 is published. Loading the
// setup assembly does not launch its GUI. Every downloaded file stays in work.
internal static class PublishedReleaseCheck
{
    private static object Field(object value, string name)
    {
        if (value == null) throw new InvalidDataException("Missing result: " + name);
        FieldInfo field = value.GetType().GetField(name);
        if (field == null) throw new InvalidDataException("Missing installer contract field: " + name);
        return field.GetValue(value);
    }

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidDataException(message);
    }

    private static string ChildOf(string root, string child)
    {
        string full = Path.GetFullPath(child);
        Require(full.StartsWith(root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar,
            StringComparison.OrdinalIgnoreCase), "Result escaped the isolated work directory.");
        return full;
    }

    private static int VerifyStandaloneFiles(object installed, string testRoot)
    {
        string release = ChildOf(testRoot, (string)Field(installed, "ReleaseDirectory"));
        string mod = ChildOf(release, (string)Field(installed, "ModDirectory"));
        string plugin = ChildOf(mod, (string)Field(installed, "PluginInstaller"));
        Require(Path.GetFileName(mod) == "@TFV_TFAR_Addon", "Unexpected mod directory.");
        Require(plugin.Equals(Path.Combine(mod, "teamspeak", "task_force_vanguard.ts3_plugin"),
            StringComparison.OrdinalIgnoreCase), "Unexpected companion package path.");
        foreach (string directory in Directory.GetDirectories(release))
            Require(!Path.GetFileName(directory).StartsWith("@", StringComparison.Ordinal) ||
                directory.Equals(mod, StringComparison.OrdinalIgnoreCase), "A replacement mod was installed.");

        var files = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string path in Directory.GetFiles(mod, "*", SearchOption.AllDirectories))
        {
            Require((File.GetAttributes(path) & FileAttributes.ReparsePoint) == 0, "Unexpected linked mod file.");
            string relative = path.Substring(mod.Length + 1).Replace('\\', '/');
            Require(files.Add(relative), "Duplicate mod file.");
            string extension = Path.GetExtension(relative);
            Require(!extension.Equals(".pbo", StringComparison.OrdinalIgnoreCase) || relative == "addons/tfv_core.pbo",
                "The public package contains another PBO.");
            Require(!extension.Equals(".dll", StringComparison.OrdinalIgnoreCase) || relative == "tfv_helmet_voice_x64.dll",
                "The public package contains another native extension.");
            Require(!extension.Equals(".ts3_plugin", StringComparison.OrdinalIgnoreCase) || relative == "teamspeak/task_force_vanguard.ts3_plugin",
                "The public package contains another TeamSpeak plugin package.");
        }
        Require(files.Contains("addons/tfv_core.pbo") && files.Contains("tfv_helmet_voice_x64.dll") &&
            files.Contains("teamspeak/task_force_vanguard.ts3_plugin"), "Standalone runtime files are missing.");
        Require(Directory.GetFiles(Path.Combine(mod, "keys"), "*.bikey").Length > 0, "Arma signing key is missing.");

        var packageFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using (ZipArchive archive = ZipFile.OpenRead(plugin))
        {
            foreach (ZipArchiveEntry entry in archive.Entries)
            {
                string name = entry.FullName.Replace('\\', '/');
                if (name == "plugins/" && entry.Length == 0) continue;
                Require(name == "package.ini" || name == "plugins/task_force_vanguard_win64.dll",
                    "The public companion package would install a foreign file: " + name);
                Require(packageFiles.Add(name) && entry.Length > 0, "Duplicate or empty companion package entry.");
            }
        }
        Require(packageFiles.Count == 2, "Standalone companion package is incomplete.");
        return files.Count;
    }

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length < 1 || args.Length > 2)
                throw new ArgumentException("Supply the project workspace root and optionally the reviewed R12 setup path.");
            string project = Path.GetFullPath(args[0]);
            Require(File.Exists(Path.Combine(project, "standalone", "installer", "Core.cs")), "Expected standalone workspace source missing.");
            string setup = args.Length == 2 ? Path.GetFullPath(args[1]) : Path.Combine(project, "outputs",
                "Task-force-Vanguard-TFAR-addon-R12-Release", "Task-force-Vanguard-TFAR-addon-Setup-R12.exe");
            ChildOf(Path.Combine(project, "outputs"), setup);
            string testRoot = Path.Combine(project, "work", "net12-" + Guid.NewGuid().ToString("N").Substring(0, 8));
            Require(!Directory.Exists(testRoot) && !File.Exists(testRoot), "Isolated test path already exists.");

            Type core = Assembly.LoadFrom(setup).GetType("VanguardAddonSetup.InstallerCore", true);
            object bundled = core.GetMethod("LoadBundledManifest").Invoke(null, null);
            Require(Object.Equals(Field(bundled, "schemaVersion"), 2) &&
                Object.Equals(Field(bundled, "product"), "task-force-vanguard-tfar-addon") &&
                Object.Equals(Field(bundled, "releaseNumber"), 12) &&
                Object.Equals(Field(bundled, "displayVersion"), "R12") &&
                Object.Equals(Field(bundled, "pluginVersion"), "0.12.0"), "Expected reviewed standalone R12 setup.");
            object latest = core.GetMethod("GetLatest").Invoke(null, null);
            foreach (string field in new[] { "schemaVersion", "product", "releaseNumber", "displayVersion", "pluginVersion",
                "sha256", "packageBytes", "packageUrl", "releasePageUrl" })
                Require(Object.Equals(Field(latest, field), Field(bundled, field)), "Public feed mismatch: " + field);

            object installed = core.GetMethod("InstallOnline").Invoke(null, new object[] { testRoot, latest, null });
            object readback = core.GetMethod("ReadState").Invoke(null, new object[] { testRoot });
            foreach (string field in new[] { "ReleaseNumber", "DisplayVersion", "ReleaseDirectory", "ModDirectory", "PluginInstaller" })
                Require(Object.Equals(Field(installed, field), Field(readback, field)), "Online install/readback mismatch: " + field);
            Require(Object.Equals(Field(installed, "ReleaseNumber"), 12), "Wrong installed revision.");
            int fileCount = VerifyStandaloneFiles(readback, testRoot);
            var report = new Dictionary<string, object> {
                { "CheckedUtc", DateTime.UtcNow.ToString("o") },
                { "Feed", core.GetField("DefaultFeedUrl").GetRawConstantValue() },
                { "SchemaVersion", Field(latest, "schemaVersion") }, { "Product", Field(latest, "product") },
                { "Revision", Field(latest, "releaseNumber") }, { "PluginVersion", Field(latest, "pluginVersion") },
                { "PackageSHA256", Field(latest, "sha256") }, { "PackageBytes", Field(latest, "packageBytes") },
                { "NetworkInstall", "PASS" }, { "VerifiedReadback", "PASS" },
                { "StandaloneFileCollisionCheck", "PASS" }, { "ModFileCount", fileCount },
                { "TestRoot", testRoot }, { "SetupPath", setup }, { "LiveTeamSpeakOrArmaChanged", false }
            };
            string json = new JavaScriptSerializer().Serialize(report);
            File.WriteAllText(Path.Combine(testRoot, "published-check.json"), json);
            Console.WriteLine(json);
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
