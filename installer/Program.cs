using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

[assembly: AssemblyTitle("Task force Vanguard TFAR addon")]
[assembly: AssemblyDescription("Setup and optional GitHub release checker for Task force Vanguard TFAR addon")]
[assembly: AssemblyCompany("Hacket2130")]
[assembly: AssemblyProduct("Task force Vanguard TFAR addon")]
[assembly: AssemblyCopyright("Helmet Voice contributors")]
[assembly: AssemblyVersion("1.0.1.0")]
[assembly: AssemblyFileVersion("1.0.1.0")]

namespace HelmetVoiceSetup
{
    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            try
            {
                string previewRoot = null;
                if (args.Length != 0)
                {
                    if (args.Length != 2 || args[0] != "--ui-preview" || !Path.IsPathRooted(args[1]))
                        throw new ArgumentException("The optional preview command is: --ui-preview <absolute workspace folder>");
                    previewRoot = Path.GetFullPath(args[1]);
                }
                Application.Run(new SetupForm(previewRoot));
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message, "Task force Vanguard TFAR addon — setup could not start", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }

    // Settings are deliberately separate from installed.json, which belongs to Core.cs.
    // Preview mode never reads or writes these per-user settings.
    public sealed class SetupPreferences
    {
        public string InstallRoot;
        public bool CheckOnOpen = true;
    }

    internal sealed class SetupForm : Form
    {
        private const string RepositoryUrl = "https://github.com/Hacket2130/helmet-voice-tfar";
        private static readonly Color Ink = Color.FromArgb(29, 40, 55);
        private static readonly Color Muted = Color.FromArgb(92, 106, 123);
        private static readonly Color Accent = Color.FromArgb(35, 93, 153);
        private readonly bool preview;
        private readonly string preferencesPath;
        private readonly ReleaseManifest bundled;
        private SetupPreferences preferences;
        private ReleaseManifest latest;
        private InstalledRelease installed;
        private bool busy;
        private bool ready;
        private bool stateInvalid;
        private TextBox installFolder;
        private TextBox notes;
        private Label installedValue;
        private Label latestValue;
        private Label status;
        private Button browseButton;
        private Button bundledButton;
        private Button checkButton;
        private Button updateButton;
        private Button pluginButton;
        private Button modButton;
        private Button copyButton;
        private Button releaseButton;
        private Button managerButton;
        private CheckBox checkOnOpen;
        private ProgressBar progress;

        public SetupForm(string previewRoot)
        {
            // Runtime-created controls must all exist before WinForms performs its
            // first DPI scale. Scaling an empty form leaves later children unscaled.
            SuspendLayout();
            preview = previewRoot != null;
            preferencesPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HelmetVoiceTFAR", "setup-settings.json");
            preferences = preview ? new SetupPreferences { InstallRoot = previewRoot, CheckOnOpen = false } : LoadPreferences();
            bundled = InstallerCore.LoadBundledManifest();
            Text = "Task force Vanguard TFAR addon — Setup & Updates";
            Font = new Font("Segoe UI", 9.5F);
            ForeColor = Ink;
            BackColor = Color.FromArgb(245, 247, 250);
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(980, 850);
            MinimumSize = new Size(850, 650);
            BuildInterface();
            AutoScaleDimensions = new SizeF(96F, 96F);
            AutoScaleMode = AutoScaleMode.Dpi;
            ResumeLayout(true);
            RefreshInstalledState();
            ready = true;
            if (preview)
                SetStatus("Preview only — network checks, installs, and external actions are disabled.", false);
            else if (!stateInvalid)
                SetStatus(installed == null ? "Ready. Install the bundled release, or check GitHub for a newer release." : "Ready. Your existing release is selected.", false);
            UpdateButtons();
            Shown += async delegate
            {
                if (!preview && checkOnOpen.Checked) await CheckUpdatesAsync();
            };
            FormClosing += OnFormClosing;
        }

        private void BuildInterface()
        {
            Panel viewport = new Panel { Dock = DockStyle.Fill, AutoScroll = true };
            TableLayoutPanel layout = new TableLayoutPanel();
            layout.Dock = DockStyle.Top;
            layout.AutoSize = true;
            layout.AutoSizeMode = AutoSizeMode.GrowAndShrink;
            layout.Padding = new Padding(24, 18, 24, 18);
            layout.ColumnCount = 1;
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            layout.RowCount = 8;
            for (int i = 0; i < 8; i++) layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            viewport.Controls.Add(layout);
            Controls.Add(viewport);

            TableLayoutPanel heading = MakeStack();
            heading.Margin = new Padding(0, 0, 0, 18);
            Label title = MakeLabel("Task force Vanguard TFAR addon", 20, FontStyle.Bold, Ink);
            Label subtitle = MakeLabel("TFAR Beta for Arma 3  ·  TeamSpeak 3  ·  Setup & Updates", 10, FontStyle.Regular, Muted);
            subtitle.Margin = new Padding(0, 3, 0, 0);
            heading.Controls.Add(title, 0, 0);
            heading.Controls.Add(subtitle, 0, 1);
            layout.Controls.Add(heading, 0, 0);

            TableLayoutPanel versions = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, ColumnCount = 3, BackColor = Color.White, Padding = new Padding(12, 10, 12, 10), Margin = new Padding(0, 0, 0, 16) };
            for (int i = 0; i < 3; i++) versions.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.333F));
            AddVersion(versions, 0, "INCLUDED WITH THIS SETUP", bundled.displayVersion);
            installedValue = AddVersion(versions, 1, "INSTALLED IN THIS FOLDER", "Not installed");
            latestValue = AddVersion(versions, 2, "LATEST GITHUB RELEASE", preview ? "Preview" : "Not checked");
            layout.Controls.Add(versions, 0, 1);

            TableLayoutPanel location = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, ColumnCount = 2, RowCount = 3, Margin = new Padding(0, 0, 0, 12) };
            location.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            location.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            for (int i = 0; i < 3; i++) location.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            Label folderTitle = MakeLabel("Install folder", 10, FontStyle.Bold, Ink);
            folderTitle.Margin = new Padding(0, 0, 0, 5);
            location.Controls.Add(folderTitle, 0, 0);
            installFolder = new TextBox { Dock = DockStyle.Fill, Text = preferences.InstallRoot, Margin = new Padding(0, 2, 8, 0) };
            installFolder.Leave += delegate { if (ready && !busy && !preview) { RefreshInstalledState(); SavePreferences(); } };
            location.Controls.Add(installFolder, 0, 1);
            browseButton = MakeButton("Browse…", 90);
            browseButton.Dock = DockStyle.Top;
            browseButton.Margin = new Padding(0);
            browseButton.Click += BrowseClick;
            location.Controls.Add(browseButton, 1, 1);
            Label folderHint = MakeLabel("Releases are kept in separate folders. Existing versions are preserved.", 9, FontStyle.Regular, Muted);
            folderHint.Margin = new Padding(0, 5, 0, 0);
            location.Controls.Add(folderHint, 0, 2);
            location.SetColumnSpan(folderHint, 2);
            layout.Controls.Add(location, 0, 2);

            TableLayoutPanel actions = MakeStack();
            actions.Margin = new Padding(0, 0, 0, 16);
            FlowLayoutPanel mainButtons = MakeButtonRow();
            bundledButton = MakeButton("Install bundled " + bundled.displayVersion, 190);
            bundledButton.BackColor = Accent;
            bundledButton.ForeColor = Color.White;
            bundledButton.FlatStyle = FlatStyle.Flat;
            bundledButton.FlatAppearance.BorderSize = 0;
            bundledButton.Click += async delegate { await InstallAsync(false); };
            checkButton = MakeButton("Check for updates", 160);
            checkButton.Click += async delegate { await CheckUpdatesAsync(); };
            updateButton = MakeButton("Install update", 145);
            updateButton.Click += async delegate { await InstallAsync(true); };
            releaseButton = MakeButton("GitHub releases ↗", 160);
            releaseButton.Click += delegate { OpenReleasePage(); };
            mainButtons.Controls.AddRange(new Control[] { bundledButton, checkButton, updateButton, releaseButton });
            actions.Controls.Add(mainButtons, 0, 0);
            checkOnOpen = new CheckBox { AutoSize = true, UseMnemonic = false, Text = "Check for updates when I open this app", Checked = preferences.CheckOnOpen, Margin = new Padding(1, 7, 0, 0) };
            checkOnOpen.CheckedChanged += delegate { if (ready && !preview) SavePreferences(); };
            Label checkHint = MakeLabel("Checks only while this app is open. Updates are installed when you choose.", 9, FontStyle.Regular, Muted);
            checkHint.Margin = new Padding(0, 4, 0, 0);
            actions.Controls.Add(checkOnOpen, 0, 1);
            actions.Controls.Add(checkHint, 0, 2);
            layout.Controls.Add(actions, 0, 3);

            GroupBox setup = new GroupBox { Text = "After installing", Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Padding = new Padding(14, 24, 14, 12), Margin = new Padding(0, 0, 0, 16) };
            TableLayoutPanel instructions = MakeStack();
            instructions.Controls.Add(MakeLabel("1. Install the TeamSpeak plugin below, then restart TeamSpeak yourself when convenient.", 9.5F, FontStyle.Regular, Ink), 0, 0);
            instructions.Controls.Add(MakeLabel("2. In Arma 3 Launcher, select Mods → Local mod, then add the installed @TFAR_DTR folder.", 9.5F, FontStyle.Regular, Ink), 0, 1);
            instructions.Controls.Add(MakeLabel("3. Disable original TFAR Beta. Enable this mod, CBA_A3, and Long Live the Empire. ACE adds the toggle.", 9.5F, FontStyle.Regular, Ink), 0, 2);
            for (int i = 0; i < 3; i++) instructions.Controls[i].Margin = new Padding(0, 0, 0, 7);
            FlowLayoutPanel setupButtons = MakeButtonRow();
            pluginButton = MakeButton("Install TeamSpeak plugin", 205);
            pluginButton.Click += delegate { OpenPluginInstaller(); };
            modButton = MakeButton("Open mod folder", 145);
            modButton.Click += delegate { OpenInstalledFolder(false); };
            copyButton = MakeButton("Copy mod path", 140);
            copyButton.Click += delegate { CopyModPath(); };
            managerButton = MakeButton("Open updater folder", 165);
            managerButton.Click += delegate { OpenInstalledFolder(true); };
            setupButtons.Controls.AddRange(new Control[] { pluginButton, modButton, copyButton, managerButton });
            instructions.Controls.Add(setupButtons, 0, 3);
            Label setupHint = MakeLabel("Setup leaves Arma and TeamSpeak running. After an update, select the new version’s Local mod folder.", 9, FontStyle.Regular, Muted);
            setupHint.Margin = new Padding(0, 5, 0, 0);
            instructions.Controls.Add(setupHint, 0, 4);
            setup.Controls.Add(instructions);
            layout.Controls.Add(setup, 0, 4);

            Label notesHeading = MakeLabel("Release notes & activity", 10, FontStyle.Bold, Ink);
            notesHeading.Margin = new Padding(0, 0, 0, 7);
            layout.Controls.Add(notesHeading, 0, 5);
            notes = new TextBox { Dock = DockStyle.Fill, Height = 150, MinimumSize = new Size(0, 150), Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, BackColor = Color.White, BorderStyle = BorderStyle.FixedSingle, Font = new Font("Segoe UI", 9.5F), Margin = new Padding(0, 0, 0, 6), Text = "Bundled " + bundled.displayVersion + "  ·  Plugin " + bundled.pluginVersion + Environment.NewLine + Environment.NewLine + bundled.notes };
            layout.Controls.Add(notes, 0, 6);

            TableLayoutPanel footer = MakeStack();
            status = MakeLabel("Ready", 9.5F, FontStyle.Regular, Muted);
            status.Margin = new Padding(0, 3, 0, 6);
            progress = new ProgressBar { Dock = DockStyle.Fill, Height = 7, Style = ProgressBarStyle.Marquee, MarqueeAnimationSpeed = 25, Visible = false, Margin = new Padding(0) };
            footer.Controls.Add(status, 0, 0);
            footer.Controls.Add(progress, 0, 1);
            layout.Controls.Add(footer, 0, 7);
        }

        private static Label MakeLabel(string text, float size, FontStyle style, Color color)
        {
            return new Label { AutoSize = true, Dock = DockStyle.Fill, UseMnemonic = false, Text = text, Font = new Font("Segoe UI", size, style), ForeColor = color, Margin = new Padding(0) };
        }

        private static TableLayoutPanel MakeStack()
        {
            TableLayoutPanel stack = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, ColumnCount = 1, Margin = new Padding(0), GrowStyle = TableLayoutPanelGrowStyle.AddRows };
            stack.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            return stack;
        }

        private static Label AddVersion(TableLayoutPanel parent, int column, string caption, string value)
        {
            TableLayoutPanel panel = MakeStack();
            panel.Margin = new Padding(0, 0, 8, 0);
            Label captionLabel = MakeLabel(caption, 8, FontStyle.Bold, Muted);
            Label valueLabel = MakeLabel(value, 14, FontStyle.Bold, Ink);
            valueLabel.Margin = new Padding(0, 4, 0, 0);
            panel.Controls.Add(captionLabel, 0, 0);
            panel.Controls.Add(valueLabel, 0, 1);
            parent.Controls.Add(panel, column, 0);
            return valueLabel;
        }

        private static FlowLayoutPanel MakeButtonRow()
        {
            return new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, FlowDirection = FlowDirection.LeftToRight, WrapContents = true, Margin = new Padding(0), Padding = new Padding(0) };
        }

        private static Button MakeButton(string text, int width)
        {
            return new Button { Text = text, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, MinimumSize = new Size(width, 35), Padding = new Padding(8, 5, 8, 5), UseMnemonic = false, Margin = new Padding(0, 0, 10, 5), UseVisualStyleBackColor = true, Cursor = Cursors.Hand };
        }

        private SetupPreferences LoadPreferences()
        {
            SetupPreferences result = new SetupPreferences();
            try
            {
                if (File.Exists(preferencesPath))
                    result = new JavaScriptSerializer().Deserialize<SetupPreferences>(File.ReadAllText(preferencesPath)) ?? result;
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
            catch (ArgumentException) { }
            catch (InvalidOperationException) { }
            if (String.IsNullOrWhiteSpace(result.InstallRoot) || !Path.IsPathRooted(result.InstallRoot))
                result.InstallRoot = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "HelmetVoiceTFAR");
            return result;
        }

        private void SavePreferences()
        {
            if (preview || !ready) return;
            try
            {
                preferences.InstallRoot = GetRoot();
                preferences.CheckOnOpen = checkOnOpen.Checked;
                Directory.CreateDirectory(Path.GetDirectoryName(preferencesPath));
                string staged = preferencesPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
                File.WriteAllText(staged, new JavaScriptSerializer().Serialize(preferences));
                if (File.Exists(preferencesPath)) File.Replace(staged, preferencesPath, null);
                else File.Move(staged, preferencesPath);
            }
            catch (Exception ex)
            {
                AppendActivity("Could not save setup preferences: " + ex.Message);
            }
        }

        private string GetRoot()
        {
            string value = installFolder.Text.Trim();
            if (String.IsNullOrEmpty(value) || !Path.IsPathRooted(value))
                throw new ArgumentException("Choose a full, absolute install folder path.");
            return Path.GetFullPath(value);
        }

        private void BrowseClick(object sender, EventArgs e)
        {
            if (preview || busy) return;
            using (FolderBrowserDialog picker = new FolderBrowserDialog())
            {
                picker.Description = "Choose the folder for Task force Vanguard TFAR addon and its update helper.";
                picker.SelectedPath = installFolder.Text;
                picker.ShowNewFolderButton = true;
                if (picker.ShowDialog(this) != DialogResult.OK) return;
                installFolder.Text = picker.SelectedPath;
                RefreshInstalledState();
                SavePreferences();
            }
        }

        private void RefreshInstalledState()
        {
            installed = null;
            stateInvalid = false;
            try
            {
                installed = InstallerCore.ReadState(GetRoot());
                installedValue.Text = installed == null ? "Not installed" : installed.DisplayVersion;
            }
            catch (Exception ex)
            {
                stateInvalid = true;
                installedValue.Text = "Needs attention";
                SetStatus("Could not read this install folder. See activity below.", true);
                AppendActivity(ex.Message);
            }
            UpdateButtons();
        }

        private async Task CheckUpdatesAsync()
        {
            if (busy || preview) return;
            SetBusy(true);
            latest = null;
            latestValue.Text = "Checking…";
            SetStatus("Checking GitHub Releases…", false);
            try
            {
                latest = await Task.Run(() => InstallerCore.GetLatest());
                latestValue.Text = latest.displayVersion;
                notes.Text = "Latest " + latest.displayVersion + "  ·  Plugin " + latest.pluginVersion + Environment.NewLine + Environment.NewLine + latest.notes;
                int current = installed == null ? bundled.releaseNumber : installed.ReleaseNumber;
                if (latest.releaseNumber > current)
                    SetStatus("A newer release is available. Choose Install update when you are ready.", false);
                else if (installed == null)
                    SetStatus("The bundled release is current. Choose Install bundled " + bundled.displayVersion + " to set it up.", false);
                else if (latest.releaseNumber == current)
                    SetStatus("Your selected installation is up to date with the GitHub release feed.", false);
                else
                    SetStatus("Your selected installation is newer than the current GitHub release feed.", false);
                AppendActivity("Update check completed successfully.");
            }
            catch (Exception ex)
            {
                latest = null;
                latestValue.Text = "Check failed";
                SetStatus("Update check failed. Your update status is unknown; the bundled installer still works offline.", true);
                AppendActivity("Update check failed: " + ex.Message);
            }
            finally { SetBusy(false); }
        }

        private async Task InstallAsync(bool online)
        {
            if (preview || busy || stateInvalid) return;
            if (online && (latest == null || latest.releaseNumber <= (installed == null ? bundled.releaseNumber : installed.ReleaseNumber))) return;
            string root;
            try { root = GetRoot(); }
            catch (Exception ex) { ShowError(ex.Message); return; }
            SavePreferences();
            SetBusy(true);
            SetStatus(online ? "Downloading and verifying the selected release…" : "Verifying and installing the bundled release…", false);
            try
            {
                ReleaseManifest selected = latest;
                installed = await Task.Run(() => online ? InstallerCore.InstallOnline(root, selected, ReportProgress) : InstallerCore.InstallBundled(root, ReportProgress));
                string updaterMessage = await Task.Run(() => KeepUpdater(root));
                installedValue.Text = installed.DisplayVersion;
                SetStatus("Installed " + installed.DisplayVersion + ". Complete the TeamSpeak and Arma steps above.", false);
                AppendActivity("Installed mod: " + installed.ModDirectory);
                AppendActivity(updaterMessage);
                AppendActivity("When updating Arma's Local mod, disable the previous version and select the new version's folder.");
            }
            catch (Exception ex)
            {
                SetStatus("Installation did not finish. See the error below; existing releases are preserved.", true);
                AppendActivity("Installation error: " + ex.Message);
                MessageBox.Show(this, ex.Message, "Task force Vanguard TFAR addon — installation", MessageBoxButtons.OK, MessageBoxIcon.Error);
                RefreshInstalledState();
            }
            finally { SetBusy(false); }
        }

        // The install button authorizes this copy. Never replace an unrelated executable.
        // A previous copy is preserved so an updater refresh is reversible.
        private static string KeepUpdater(string root)
        {
            try
            {
                string source = Path.GetFullPath(Application.ExecutablePath);
                string target = Path.Combine(Path.GetFullPath(root), "Task-force-Vanguard-TFAR-addon-Setup.exe");
                if (String.Equals(source, target, StringComparison.OrdinalIgnoreCase)) return "Updater is available at: " + target;
                if (File.Exists(target))
                {
                    FileVersionInfo identity = FileVersionInfo.GetVersionInfo(target);
                    if (identity.ProductName != "Task force Vanguard TFAR addon" || identity.CompanyName != "Hacket2130")
                        return "The mod installed, but setup did not replace an unrecognized Task-force-Vanguard-TFAR-addon-Setup.exe. Keep this setup file to check for updates.";
                }
                string staged = target + "." + Guid.NewGuid().ToString("N") + ".tmp";
                File.Copy(source, staged, false);
                if (File.Exists(target))
                {
                    string backup = Path.Combine(root, "Task-force-Vanguard-TFAR-addon-Setup.previous-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N").Substring(0, 6) + ".exe");
                    File.Replace(staged, target, backup);
                }
                else File.Move(staged, target);
                return "Updater saved at: " + target;
            }
            catch (Exception ex)
            {
                return "The mod installed, but the updater copy could not be saved: " + ex.Message + " Keep this setup file to check for updates.";
            }
        }

        private void ReportProgress(string message)
        {
            if (IsDisposed || Disposing) return;
            if (InvokeRequired)
            {
                BeginInvoke(new Action<string>(ReportProgress), message);
                return;
            }
            SetStatus(message, false);
            AppendActivity(message);
        }

        private void SetBusy(bool value)
        {
            busy = value;
            progress.Visible = value;
            UseWaitCursor = value;
            UpdateButtons();
        }

        private void UpdateButtons()
        {
            if (bundledButton == null) return;
            bool active = !busy && !preview;
            bool hasInstall = installed != null && !stateInvalid;
            installFolder.ReadOnly = !active;
            browseButton.Enabled = active;
            checkOnOpen.Enabled = active;
            checkButton.Enabled = active;
            releaseButton.Enabled = active;
            // Selecting an older bundled release after a newer install would silently roll back.
            bundledButton.Enabled = active && !stateInvalid && (installed == null || installed.ReleaseNumber <= bundled.releaseNumber);
            bundledButton.Text = installed != null && installed.ReleaseNumber == bundled.releaseNumber ? "Verify bundled " + bundled.displayVersion : "Install bundled " + bundled.displayVersion;
            updateButton.Enabled = active && !stateInvalid && latest != null && latest.releaseNumber > (installed == null ? bundled.releaseNumber : installed.ReleaseNumber);
            if (latest != null) updateButton.Text = "Install " + latest.displayVersion;
            else updateButton.Text = "Install update";
            pluginButton.Enabled = active && hasInstall && File.Exists(installed.PluginInstaller);
            modButton.Enabled = active && hasInstall && Directory.Exists(installed.ModDirectory);
            copyButton.Enabled = active && hasInstall;
            managerButton.Enabled = active && hasInstall;
        }

        private void SetStatus(string text, bool error)
        {
            if (status == null) return;
            status.Text = text;
            status.ForeColor = error ? Color.FromArgb(160, 49, 35) : Muted;
        }

        private void AppendActivity(string message)
        {
            if (notes == null) return;
            notes.AppendText(Environment.NewLine + Environment.NewLine + DateTime.Now.ToString("HH:mm:ss") + "  " + message);
        }

        private void OpenPluginInstaller()
        {
            if (preview || busy || installed == null) return;
            try
            {
                if (!File.Exists(installed.PluginInstaller)) throw new FileNotFoundException("The installed TeamSpeak package is missing. Verify or reinstall this release.");
                // Use TeamSpeak's registered .ts3_plugin installer, exactly as a double-click does.
                Process.Start(new ProcessStartInfo(installed.PluginInstaller) { UseShellExecute = true });
                SetStatus("TeamSpeak's package installer was opened. Restart TeamSpeak yourself after installing the plugin.", false);
            }
            catch (Exception ex)
            {
                ShowError("Could not open the TeamSpeak package installer. Install TeamSpeak 3 and associate .ts3_plugin files with its package installer, then try again." + Environment.NewLine + Environment.NewLine + ex.Message);
            }
        }

        private void OpenInstalledFolder(bool updater)
        {
            if (preview || busy || installed == null) return;
            try
            {
                string path = updater ? GetRoot() : installed.ModDirectory;
                if (!Directory.Exists(path)) throw new DirectoryNotFoundException("The installed folder is missing.");
                Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
            }
            catch (Exception ex) { ShowError(ex.Message); }
        }

        private void CopyModPath()
        {
            if (preview || busy || installed == null) return;
            try
            {
                Clipboard.SetText(installed.ModDirectory);
                SetStatus("Mod path copied. Use it in Arma 3 Launcher's Local mod picker.", false);
            }
            catch (Exception ex) { ShowError("Could not copy the path: " + ex.Message); }
        }

        private void OpenReleasePage()
        {
            if (preview || busy) return;
            try
            {
                string link = latest == null ? RepositoryUrl + "/releases" : latest.releasePageUrl;
                Uri uri;
                if (!Uri.TryCreate(link, UriKind.Absolute, out uri) || uri.Scheme != "https" || !String.Equals(uri.Host, "github.com", StringComparison.OrdinalIgnoreCase) || uri.Port != 443 || !String.IsNullOrEmpty(uri.UserInfo) || !(String.Equals(uri.AbsolutePath, "/Hacket2130/helmet-voice-tfar/releases", StringComparison.OrdinalIgnoreCase) || uri.AbsolutePath.StartsWith("/Hacket2130/helmet-voice-tfar/releases/", StringComparison.OrdinalIgnoreCase)))
                    throw new InvalidDataException("This release page is outside the configured Task force Vanguard TFAR addon repository.");
                Process.Start(new ProcessStartInfo(uri.AbsoluteUri) { UseShellExecute = true });
            }
            catch (Exception ex) { ShowError(ex.Message); }
        }

        private void ShowError(string message)
        {
            SetStatus(message, true);
            AppendActivity(message);
            MessageBox.Show(this, message, "Task force Vanguard TFAR addon — setup", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }

        private void OnFormClosing(object sender, FormClosingEventArgs e)
        {
            if (busy)
            {
                e.Cancel = true;
                SetStatus("Please wait for the current operation to finish before closing setup.", false);
                return;
            }
            if (!preview) SavePreferences();
        }
    }
}
