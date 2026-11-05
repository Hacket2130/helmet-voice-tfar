using System;
using System.IO;
using System.Reflection;
using System.Collections.Generic;
using System.Web.Script.Serialization;

internal static class PublishedReleaseCheck
{
    private static object Field(object value, string name) { return value.GetType().GetField(name).GetValue(value); }

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length != 1) throw new ArgumentException("Supply the project workspace root.");
            string project = Path.GetFullPath(args[0]);
            string setup = Path.Combine(project, "outputs", "Task-force-Vanguard-TFAR-addon-R11", "Task-force-Vanguard-TFAR-addon-Setup-R11.exe");
            if (!File.Exists(Path.Combine(project, "installer", "Core.cs"))) throw new ArgumentException("Expected workspace source missing.");
            string testRoot = Path.Combine(project, "work", "net-" + Guid.NewGuid().ToString("N").Substring(0, 8));
            Type core = Assembly.LoadFrom(setup).GetType("HelmetVoiceSetup.InstallerCore", true);
            object bundled = core.GetMethod("LoadBundledManifest").Invoke(null, null);
            object latest = core.GetMethod("GetLatest").Invoke(null, null);
            foreach (string field in new[] { "releaseNumber", "sha256", "packageBytes", "packageUrl" })
                if (!Object.Equals(Field(latest, field), Field(bundled, field))) throw new InvalidDataException("Public feed mismatch: " + field);
            object installed = core.GetMethod("InstallOnline").Invoke(null, new object[] { testRoot, latest, null });
            object readback = core.GetMethod("ReadState").Invoke(null, new object[] { testRoot });
            if (!Object.Equals(Field(installed, "ReleaseNumber"), Field(latest, "releaseNumber")) ||
                !Object.Equals(Field(readback, "ReleaseNumber"), Field(latest, "releaseNumber"))) throw new InvalidDataException("Online install/readback revision mismatch.");
            var report = new Dictionary<string, object> {
                { "CheckedUtc", DateTime.UtcNow.ToString("o") },
                { "Feed", core.GetField("DefaultFeedUrl").GetRawConstantValue() },
                { "Revision", Field(latest, "releaseNumber") },
                { "PackageSHA256", Field(latest, "sha256") },
                { "PackageBytes", Field(latest, "packageBytes") },
                { "NetworkInstall", "PASS" }, { "VerifiedReadback", "PASS" },
                { "TestRoot", testRoot }, { "LiveTeamSpeakOrArmaChanged", false }
            };
            string json = new JavaScriptSerializer().Serialize(report);
            File.WriteAllText(Path.Combine(project, "work", "vanguard-installer-build", "published-check.json"), json);
            Console.WriteLine(json);
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
