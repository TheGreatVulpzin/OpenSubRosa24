// Server panel with separate developer captures and verified release updates.
//
// Build: tools\launcher\build.bat. It uses the C# compiler of .NET Framework 4, which is
// part of Windows, so the testers do not install anything. C# 5 only (no $"" strings).
// The exe is 32-bit, so that its dumps of the 32-bit server have the x86 view.
// Put the exe in the server folder, next to subvulpes-server.exe.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Media;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace SubVulpes
{
    class ReleaseAsset
    {
        public string name { get; set; }
        public string browser_download_url { get; set; }
    }

    class ServerRelease
    {
        public string name { get; set; }
        public string tag_name { get; set; }
        public bool draft { get; set; }
        public bool prerelease { get; set; }
        public ReleaseAsset[] assets { get; set; }
        public string DisplayName { get { return string.IsNullOrWhiteSpace(name) ? tag_name : name; } }
    }

    static class ReleaseUpdate
    {
        const string Endpoint = "https://api.github.com/repos/TheGreatVulpzin/OpenSubRosa24/releases";
        static readonly Regex Tag = new Regex(@"^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?$");

        public static int CompareVersions(string left, string right)
        {
            Match a = Tag.Match(left), b = Tag.Match(right);
            if (!a.Success || !b.Success)
                throw new InvalidDataException("Cannot read the version tag.");
            for (int i = 1; i <= 3; i++) {
                int c = long.Parse(a.Groups[i].Value).CompareTo(long.Parse(b.Groups[i].Value));
                if (c != 0) return c;
            }
            string x = a.Groups[4].Value, y = b.Groups[4].Value;
            if (x == y) return 0;
            if (x == "") return 1;
            if (y == "") return -1;
            string[] xs = x.Split('.'), ys = y.Split('.');
            for (int i = 0; i < Math.Min(xs.Length, ys.Length); i++) {
                long xn, yn;
                bool nx = long.TryParse(xs[i], out xn), ny = long.TryParse(ys[i], out yn);
                int c = nx && ny ? xn.CompareTo(yn) : nx != ny ? (nx ? -1 : 1) : string.CompareOrdinal(xs[i], ys[i]);
                if (c != 0) return c;
            }
            return xs.Length.CompareTo(ys.Length);
        }

        public static ServerRelease Select(IEnumerable<ServerRelease> releases, string current, bool previews)
        {
            ServerRelease best = null;
            foreach (var release in releases) {
                if (release.draft || (!previews && release.prerelease) || !Tag.IsMatch(release.tag_name ?? ""))
                    continue;
                if (!previews && Tag.Match(release.tag_name).Groups[4].Success)
                    continue;
                if (CompareVersions(release.tag_name, current) > 0 &&
                    (best == null || CompareVersions(release.tag_name, best.tag_name) > 0))
                    best = release;
            }
            return best;
        }

        static HttpWebRequest Request(string url)
        {
            if (new Uri(url).Scheme != Uri.UriSchemeHttps)
                throw new InvalidDataException("The download URL must use HTTPS.");
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2.
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.UserAgent = "OpenSubRosa24-launcher";
            request.Accept = "application/vnd.github+json";
            request.Timeout = 15000;
            request.ReadWriteTimeout = 15000;
            return request;
        }

        static string Read(string url)
        {
            using (var response = Request(url).GetResponse())
            using (var reader = new StreamReader(response.GetResponseStream()))
                return reader.ReadToEnd();
        }

        public static ServerRelease Find(string current, bool previews)
        {
            var releases = new List<ServerRelease>();
            var json = new JavaScriptSerializer { MaxJsonLength = 8 * 1024 * 1024 };
            for (int page = 1; ; page++) {
                var batch = json.Deserialize<ServerRelease[]>(Read(Endpoint + "?per_page=100&page=" + page));
                releases.AddRange(batch);
                if (batch.Length < 100) break;
            }
            return Select(releases, current, previews);
        }

        public static void Verify(string file, string checksum)
        {
            Match hash = Regex.Match(checksum.Trim(), @"\A([0-9a-fA-F]{64})(?:[ \t]+[^\r\n]+)?\s*\z");
            if (!hash.Success)
                throw new InvalidDataException("The release SHA-256 file is invalid.");
            using (var algorithm = SHA256.Create())
            using (var stream = File.OpenRead(file)) {
                string actual = BitConverter.ToString(algorithm.ComputeHash(stream)).Replace("-", "");
                if (!actual.Equals(hash.Groups[1].Value, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("SHA-256 mismatch. The update was not installed.");
            }
        }

        public static string Download(ServerRelease release, string directory)
        {
            string name = "OpenSubRosa24-server-" + release.tag_name + ".zip";
            var assets = release.assets ?? new ReleaseAsset[0];
            var zip = assets.FirstOrDefault(a => a.name == name);
            var hash = assets.FirstOrDefault(a => a.name == name + ".sha256");
            if (zip == null || hash == null)
                throw new InvalidDataException("The release has no server zip or SHA-256 file.");
            string checksum = Read(hash.browser_download_url);
            Directory.CreateDirectory(directory);
            string path = Path.Combine(directory, name);
            try {
                using (var response = Request(zip.browser_download_url).GetResponse())
                using (var source = response.GetResponseStream())
                using (var destination = new FileStream(path, FileMode.CreateNew))
                    source.CopyTo(destination);
                Verify(path, checksum);
                return path;
            } catch {
                if (File.Exists(path)) File.Delete(path);
                throw;
            }
        }
    }

    // A key=value file (config.txt, launcher.ini, session.ini). Save keeps the order of the
    // lines and the lines without a key. The server reads its files in the ANSI code page.
    class KeyFile
    {
        public readonly string FilePath;
        readonly List<string> lines = new List<string>();

        public KeyFile(string path)
        {
            FilePath = path;
            Load();
        }

        public bool Exists { get { return File.Exists(FilePath); } }

        public void Load()
        {
            lines.Clear();
            if (File.Exists(FilePath))
                lines.AddRange(File.ReadAllText(FilePath, Encoding.Default).Replace("\r\n", "\n").Split('\n'));
        }

        int Find(string key)
        {
            for (int i = 0; i < lines.Count; i++)
                if (lines[i].StartsWith(key + "="))
                    return i;
            return -1;
        }

        public string Get(string key, string fallback)
        {
            int i = Find(key);
            return i < 0 ? fallback : lines[i].Substring(key.Length + 1);
        }

        public void Set(string key, string value)
        {
            int i = Find(key);
            if (i < 0)
                lines.Add(key + "=" + value);
            else
                lines[i] = key + "=" + value;
        }

        public void Save()
        {
            File.WriteAllText(FilePath, string.Join("\r\n", lines.ToArray()), Encoding.Default);
        }
    }

    // The captures of one test. A preset is a named set of captures.
    class Captures
    {
        public bool Profile, Timers, Packets, Record, FreezeDump;
        public string CrashDump = "small";  // none, small or full

        public Captures Copy() { return (Captures)MemberwiseClone(); }

        public bool Same(Captures o)
        {
            return Profile == o.Profile && Timers == o.Timers && Packets == o.Packets && Record == o.Record &&
                   FreezeDump == o.FreezeDump && CrashDump == o.CrashDump;
        }

        public string Summary()
        {
            var parts = new List<string>();
            if (Profile) parts.Add("speed profile");
            if (Timers) parts.Add("game timers");
            if (Packets) parts.Add("packets");
            if (Record) parts.Add("harness recording");
            if (CrashDump != "none") parts.Add(CrashDump + " crash dump");
            if (FreezeDump) parts.Add("freeze dump");
            return parts.Count == 0 ? "no captures" : string.Join(", ", parts.ToArray());
        }
    }

    class Preset
    {
        public readonly string Name, Text;
        public readonly Captures Captures;
        public Preset(string name, string text, Captures c) { Name = name; Text = text; Captures = c; }
        public override string ToString() { return Name; }
    }

    // One item of the game mode list: the gametype value and its name.
    class Mode
    {
        public readonly string Value, Name;
        public Mode(string value, string name) { Value = value; Name = name; }
        public override string ToString() { return Name; }
    }

    // The dashed line marks normal speed (62 ticks/s).
    class SpeedGraph : Panel
    {
        public readonly List<double> Points = new List<double>();

        public SpeedGraph()
        {
            DoubleBuffered = true;
            BackColor = SystemColors.Window;
            BorderStyle = BorderStyle.FixedSingle;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            int w = ClientSize.Width - 40, h = ClientSize.Height - 16;
            Func<double, float> y = v => 8 + (float)(h * (1 - Math.Min(v, 70) / 70));
            using (var pen = new Pen(SystemColors.ControlDark) { DashStyle = DashStyle.Dash })
                g.DrawLine(pen, 32, y(62), 32 + w, y(62));
            using (var brush = new SolidBrush(SystemColors.ControlText)) {
                g.DrawString("62", Font, brush, 4, y(62) - 7);
                g.DrawString("0", Font, brush, 4, y(0) - 7);
            }
            if (Points.Count == 0) {
                return;
            }
            int n = Math.Max(Points.Count, 10);
            var pts = new PointF[Points.Count];
            for (int i = 0; i < Points.Count; i++)
                pts[i] = new PointF(32 + w * (i + 1) / (float)n, y(Points[i]));
            for (int i = 1; i < pts.Length; i++)
                using (var pen = new Pen(Points[i] < 55 || Points[i - 1] < 55 ? Color.Red : SystemColors.Highlight, 2))
                    g.DrawLine(pen, pts[i - 1], pts[i]);
            for (int i = 0; i < pts.Length; i++)
                using (var b = new SolidBrush(Points[i] < 55 ? Color.Red : SystemColors.Highlight))
                    g.FillEllipse(b, pts[i].X - 3, pts[i].Y - 3, 6, 6);
        }
    }

    class LauncherForm : Form
    {
        static readonly Regex TicksLine = new Regex(@"^Server status: ([0-9.]+) ticks/s, (\d+) players, (\d+) bots$");
        const int FreezeSeconds = 150;
        const int HotkeyId = 1;
        static readonly string[] UpdateFiles = {
            "subvulpes-server.exe", "subvulpes-server-record.exe", "subvulpes-launcher.exe", "LEIA-ME.txt", "version.txt",
            "record_focus.txt"
        };

        static readonly Preset[] Presets = {
            new Preset("Normal Playtest",
                "Captures a speed profile and small crash or freeze dumps.",
                new Captures { Profile = true, FreezeDump = true }),
            new Preset("Harness Recording",
                "Records packets and function calls with a memory snapshot after 10 minutes.",
                new Captures { Packets = true, Record = true }),
            new Preset("Playtest (For Speed Issues)",
                "Captures a speed profile, game timers and small crash or freeze dumps.",
                new Captures { Profile = true, Timers = true, FreezeDump = true }),
            new Preset("Crashhunt",
                "Captures packets and full crash or freeze dumps.",
                new Captures { Packets = true, FreezeDump = true, CrashDump = "full" }),
            new Preset("Custom", "Uses the selected capture options.", null),
        };

        string folder, logsRoot, logFile;
        KeyFile config;
        readonly KeyFile settings;
        readonly string launcherFolder;
        System.Threading.EventWaitHandle stopEvent;
        bool developerRun, closeAfterStop, updating, configCreated;
        TaskCompletionSource<bool> serverStopped;
        Captures activeCaptures;
        long logOffset;
        string pendingLog = "";
        readonly ToolTip tips = new ToolTip();
        Captures captures = Presets[0].Captures.Copy();
        bool settingCaptures;

        // Frame
        TabControl mainTabs;
        readonly Dictionary<string, TabPage> pages = new Dictionary<string, TabPage>();
        string serverState = "Stopped", speedStatus = "- ticks/s", playersStatus = "- players";
        Label message, serverStatus, missingText;
        ComboBox presetBox;
        Button startStop, stopButton, browseButton, checkUpdate;
        TextBox folderBox;
        CheckBox previewsBox, debugModeBox;


        // Overview
        Label lastTicks, lowTicks, playersText, uptimeText, marksText, presetText, capturesText;
        SpeedGraph graph;
        Panel runPanel, endPanel;
        Label endTitle, endText;
        TextBox endNotes;

        // Console
        TextBox console;

        // Captures page
        CheckBox profileBox, timersBox, packetsBox, recordBox, freezeBox;
        RadioButton dumpNone, dumpSmall, dumpFull;

        // Past tests
        ListView testsList;
        TextBox testNotes;

        // Settings
        TextBox nameBox, passwordBox, welcomeBox;
        ComboBox modeBox;
        NumericUpDown playersBox, roundBox, portBox, bytesBox;
        Label dirtyText;
        Button saveButton, undoButton;
        bool loading, dirty;

        // The running test
        Process server;
        string testDir;
        DateTime started, lastSpeedLine;
        bool stopping, crashed, frozen;
        int marks, maxPlayers;
        readonly List<double> speeds = new List<double>();
        readonly Timer clock = new Timer { Interval = 1000 };

        public LauncherForm(string folder)
        {
            launcherFolder = folder;
            settings = new KeyFile(Path.Combine(folder, "launcher.ini"));
            this.folder = settings.Get("server_folder", folder);
            if (!Directory.Exists(this.folder)) this.folder = folder;
            SetFolder(this.folder);
            Text = "OpenSubRosa24 Server" + Version();
            Font = SystemFonts.MessageBoxFont;
            ForeColor = SystemColors.ControlText;
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(1000, 680);
            MinimumSize = new Size(820, 560);
            AllowDrop = true;
            DragEnter += (s, e) => { if (ZipOf(e) != null) e.Effect = DragDropEffects.Copy; };
            DragDrop += async (s, e) => { string zip = ZipOf(e); if (zip != null) await InstallUpdate(zip); };

            mainTabs = new TabControl { Dock = DockStyle.Fill };
            message = new Label { AutoSize = true, Dock = DockStyle.Fill, Margin = new Padding(8, 4, 8, 4) };
            var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2, Padding = new Padding(4) };
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            layout.Controls.Add(mainTabs, 0, 0);
            layout.Controls.Add(message, 0, 1);
            AddPage("Server", ServerPage());
            AddPage("Server settings", SettingsPage());
            AddPage("Launcher settings", LauncherSettingsPage());
            AddPage("Debug", DeveloperPage());
            Controls.Add(layout);
            SetTextColors(this);
            SetDebugMode(debugModeBox.Checked);


            LoadSettings();
            string saved = settings.Get("preset", Presets[0].Name);
            presetBox.SelectedItem = Presets.FirstOrDefault(p => p.Name == saved) ?? Presets[0];
            if (presetBox.SelectedItem == Presets[Presets.Length - 1])
                captures = ReadCustomCaptures();
            ShowCaptures();
            SetState("Stopped");
            ShowPage("Server");
            clock.Tick += delegate { OnClock(); };
            clock.Start();
            FormClosing += OnClosing;
            Load += delegate {
                RegisterHotKey(Handle, HotkeyId, 0x2 /* MOD_CONTROL */, (int)Keys.F12);
                CleanOldLauncher();
            };
            FormClosed += delegate { UnregisterHotKey(Handle, HotkeyId); };
        }

        string Version()
        {
            string v = Path.Combine(folder, "version.txt");
            return File.Exists(v) ? " - " + File.ReadLines(v).FirstOrDefault() : "";
        }

        void SetTextColors(Control control)
        {
            if (control == console) return;
            control.ForeColor = SystemColors.ControlText;
            foreach (Control child in control.Controls) SetTextColors(child);
        }



        void SetFolder(string path)
        {
            string selected = Path.GetFullPath(path);
            var nextConfig = new KeyFile(Path.Combine(selected, "config.txt"));
            bool absent = !nextConfig.Exists;
            if (absent) {
                using (var source = typeof(LauncherForm).Assembly.GetManifestResourceStream("SubVulpes.DefaultConfig"))
                using (var destination = File.Create(nextConfig.FilePath))
                    source.CopyTo(destination);
                nextConfig.Load();
            }
            folder = selected;
            logsRoot = Path.Combine(folder, "logs");
            logFile = Path.Combine(logsRoot, "server.log");
            config = nextConfig;
            configCreated = absent;
            if (missingText != null) ShowMissing(absent);
        }

        void OpenServerFolder(string path)
        {
            if (server != null) return;
            SetFolder(path);
            folderBox.Text = folder;
            settings.Set("server_folder", folder);
            settings.Save();
            LoadSettings();
            recordBox.Enabled = File.Exists(Path.Combine(folder, "subvulpes-server-record.exe"));
            logOffset = 0;
            pendingLog = "";
            console.Clear();
            Text = "OpenSubRosa24 Server" + Version();
        }

        void ShowMissing(bool configCreated)
        {
            var missing = new List<string>();
            if (!Directory.Exists(Path.Combine(folder, "data"))) missing.Add("data/");
            if (configCreated) missing.Add("config.txt (created)");
            missingText.Text = missing.Count == 0 ? "" : "Missing: " + string.Join(", ", missing.ToArray());
        }

        void ChooseFolder()
        {
            using (var dialog = new FolderBrowserDialog { SelectedPath = folder }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                if (dirty && MessageBox.Show(this, "Save the changed settings?", Text, MessageBoxButtons.YesNo) == DialogResult.Yes)
                    SaveSettings();
                try {
                    OpenServerFolder(dialog.SelectedPath);
                } catch (Exception ex) {
                    MessageBox.Show(this, "Cannot open the server folder: " + ex.Message, Text);
                }
            }
        }

        Control ServerPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 4 };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            var bar = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, WrapContents = false, Margin = new Padding(0, 0, 0, 8) };
            startStop = new Button { Text = "Start", AutoSize = true, Font = new Font(Font, FontStyle.Bold) };
            stopButton = new Button { Text = "Stop", AutoSize = true, Font = new Font(Font, FontStyle.Bold), Enabled = false };
            startStop.Click += delegate { StartServer(); };
            stopButton.Click += async delegate { await StopServer(); };
            serverStatus = new Label { AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(12, 7, 3, 3) };
            bar.Controls.AddRange(new Control[] { startStop, stopButton, serverStatus });
            page.Controls.Add(bar, 0, 0);
            missingText = new Label { AutoSize = true, Margin = new Padding(3, 0, 3, 8) };
            ShowMissing(configCreated);
            page.Controls.Add(missingText, 0, 1);
            Control log = ConsolePage();
            log.Dock = DockStyle.Fill;
            page.Controls.Add(log, 0, 2);
            var save = new Button { Text = "Save Log", AutoSize = true, Margin = new Padding(3, 8, 3, 3) };
            save.Click += async delegate { await SaveLog(); };
            page.Controls.Add(save, 0, 3);
            return page;
        }

        Control LauncherSettingsPage()
        {
            var page = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true };
            var folderRow = new TableLayoutPanel { ColumnCount = 3, AutoSize = true, Margin = new Padding(0, 0, 0, 8) };
            folderRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 120));
            folderRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 460));
            folderRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            folderRow.Controls.Add(new Label { Text = "Server folder", AutoSize = true, Anchor = AnchorStyles.Left }, 0, 0);
            folderBox = new TextBox { Text = folder, ReadOnly = true, Dock = DockStyle.Fill };
            browseButton = new Button { Text = "Browse", AutoSize = true };
            browseButton.Click += delegate { ChooseFolder(); };
            folderRow.Controls.Add(folderBox, 1, 0);
            folderRow.Controls.Add(browseButton, 2, 0);
            page.Controls.Add(folderRow);
            var updates = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 0, 0, 8) };
            checkUpdate = new Button { Text = "Check for updates", AutoSize = true };
            checkUpdate.Click += async delegate { await CheckForUpdates(); };
            tips.SetToolTip(checkUpdate, "Finds and verifies a newer release on GitHub.");
            var update = new Button { Text = "Update from a zip", AutoSize = true };
            update.Click += async delegate { await ChooseUpdate(); };
            tips.SetToolTip(update, "Installs package files and keeps a backup.");
            updates.Controls.AddRange(new Control[] { checkUpdate, update });
            page.Controls.Add(updates);
            previewsBox = new CheckBox { Text = "Include pre-releases", AutoSize = true, Checked = settings.Get("pre_releases", "0") == "1" };
            tips.SetToolTip(previewsBox, "Includes test versions that may still contain bugs.");
            previewsBox.CheckedChanged += delegate { settings.Set("pre_releases", previewsBox.Checked ? "1" : "0"); settings.Save(); };
            page.Controls.Add(previewsBox);
            debugModeBox = new CheckBox { Text = "Debug Mode", AutoSize = true, Checked = settings.Get("developer", "0") == "1" };
            tips.SetToolTip(debugModeBox, "Shows Debug and uses its captures on the next server start.");
            debugModeBox.CheckedChanged += delegate { SetDebugMode(debugModeBox.Checked); settings.Set("developer", debugModeBox.Checked ? "1" : "0"); settings.Save(); };
            page.Controls.Add(debugModeBox);
            return page;
        }

        Control DeveloperPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 2 };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            presetBox = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 260, Margin = new Padding(3, 3, 3, 8) };
            presetBox.Items.AddRange(Presets);
            presetBox.SelectedIndexChanged += delegate { OnPreset(); };
            page.Controls.Add(presetBox, 0, 0);
            var tabs = new TabControl { Dock = DockStyle.Fill };
            Control[] views = { OverviewPage(), CapturesPage(), TestsPage() };
            string[] names = { "Test", "Captures", "Past tests" };
            for (int i = 0; i < views.Length; i++) {
                var tab = new TabPage(names[i]) { Padding = new Padding(8), UseVisualStyleBackColor = true };
                views[i].Dock = DockStyle.Fill;
                tab.Controls.Add(views[i]);
                tabs.TabPages.Add(tab);
            }
            tabs.SelectedIndexChanged += delegate { if (tabs.SelectedIndex == 2) LoadTests(); };
            page.Controls.Add(tabs, 0, 1);
            return page;
        }

        void AddPage(string name, Control view)
        {
            var page = new TabPage(name) { Padding = new Padding(8), UseVisualStyleBackColor = true };
            view.Dock = DockStyle.Fill;
            page.Controls.Add(view);
            pages[name] = page;
            mainTabs.TabPages.Add(page);
        }

        void ShowPage(string name)
        {
            if (mainTabs.TabPages.Contains(pages[name])) mainTabs.SelectedTab = pages[name];
        }

        void SetDebugMode(bool enabled)
        {
            if (enabled) {
                if (!mainTabs.TabPages.Contains(pages["Debug"])) mainTabs.TabPages.Add(pages["Debug"]);
            } else {
                if (mainTabs.SelectedTab == pages["Debug"]) ShowPage("Server");
                mainTabs.TabPages.Remove(pages["Debug"]);
            }
        }

        void SetState(string text)
        {
            serverState = text;
            startStop.Enabled = server == null && !updating;
            stopButton.Enabled = server != null && !stopping;
            browseButton.Enabled = server == null && !updating;
            presetBox.Enabled = server == null;
            RefreshStatus();
        }

        void RefreshStatus()
        {
            serverStatus.Text = serverState + " | " + playersStatus + " | " + speedStatus;
        }

        // ------------------------------------------------------------ overview

        Control OverviewPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 5 };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));

            var cards = new TableLayoutPanel { Dock = DockStyle.Fill, Height = 48, ColumnCount = 5, Margin = new Padding(0, 0, 0, 8) };
            for (int i = 0; i < 5; i++)
                cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 20));
            lastTicks = Card(cards, 0, "Last minute");
            lowTicks = Card(cards, 1, "Lowest minute");
            playersText = Card(cards, 2, "Players");
            uptimeText = Card(cards, 3, "Running for");
            marksText = Card(cards, 4, "Marks");
            page.Controls.Add(cards, 0, 0);

            page.Controls.Add(new Label { Text = "Speed of each minute (ticks/s)", AutoSize = true, ForeColor = SystemColors.ControlText, Margin = new Padding(3, 4, 3, 2) }, 0, 1);
            graph = new SpeedGraph { Dock = DockStyle.Fill, MinimumSize = new Size(0, 120) };
            page.Controls.Add(graph, 0, 2);

            // While a test runs: the marks and the dumps. After the stop: the result and the notes.
            runPanel = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, Margin = new Padding(0, 8, 0, 0) };
            var mark = new Button { Text = "Mark this moment (Ctrl+F12)", AutoSize = true };
            var dump = new Button { Text = "Dump now", AutoSize = true };
            mark.Click += delegate { AddMark(); };
            dump.Click += delegate { DumpNow("dump"); };
            tips.SetToolTip(mark, "Writes a timestamp to the test log.");
            tips.SetToolTip(dump, "Writes server memory to a file.");
            presetText = new Label { AutoSize = true, ForeColor = SystemColors.ControlText, Anchor = AnchorStyles.Left, Margin = new Padding(12, 8, 3, 3) };
            runPanel.Controls.AddRange(new Control[] { mark, dump, presetText });
            page.Controls.Add(runPanel, 0, 3);

            endPanel = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, ColumnCount = 1, Visible = false, Margin = new Padding(0, 8, 0, 0) };
            endTitle = new Label { AutoSize = true, Font = new Font(Font, FontStyle.Regular) };
            endText = new Label { AutoSize = true, ForeColor = SystemColors.ControlText, Margin = new Padding(3, 2, 3, 6) };
            endNotes = new TextBox { Multiline = true, Height = 54, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical };
            var endBar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 4, 0, 0) };
            var pack = new Button { Text = "Pack this test", AutoSize = true };
            pack.Click += delegate { SaveNotes(testDir, endNotes.Text); PackTest(testDir); };
            capturesText = new Label { AutoSize = true, ForeColor = SystemColors.ControlText, Anchor = AnchorStyles.Left, Margin = new Padding(8, 8, 3, 3) };
            endBar.Controls.AddRange(new Control[] { pack, capturesText });
            endPanel.Controls.Add(endTitle);
            endPanel.Controls.Add(endText);
            endPanel.Controls.Add(new Label { Text = "Notes", AutoSize = true });
            endPanel.Controls.Add(endNotes);
            endPanel.Controls.Add(endBar);
            page.Controls.Add(endPanel, 0, 4);

            ResetCards();
            return page;
        }

        Label Card(TableLayoutPanel cards, int column, string title)
        {
            var value = new Label { AutoSize = true, Tag = title, Anchor = AnchorStyles.Left, Margin = new Padding(4) };
            cards.Controls.Add(value, column, 0);
            return value;
        }

        static void SetCard(Label label, string value)
        {
            label.Text = label.Tag + ": " + value;
        }

        void ResetCards()
        {
            foreach (var l in new[] { lastTicks, lowTicks, playersText, uptimeText })
                SetCard(l, "-");
            lastTicks.ForeColor = SystemColors.ControlText;
            SetCard(marksText, "0");
            speedStatus = "- ticks/s";
            playersStatus = "- players";
        }

        // ------------------------------------------------------------ console

        Control ConsolePage()
        {
            console = new TextBox {
                Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Both, WordWrap = false,
                Font = new Font("Consolas", Font.Size), BackColor = Color.FromArgb(24, 24, 24), ForeColor = Color.Gainsboro
            };
            return console;
        }

        // ------------------------------------------------------------ captures

        Control CapturesPage()
        {
            var page = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true };

            profileBox = Option("Speed profile", "Samples code each millisecond to locate slow functions.");
            timersBox = Option("Game timers", "The time of each part of the game, in the log each minute.");
            packetsBox = Option("Packet capture", "Records network traffic, including player names and chat.");
            recordBox = Option("Harness recording", "Records function calls with a memory snapshot after 10 minutes.");
            recordBox.Enabled = File.Exists(Path.Combine(folder, "subvulpes-server-record.exe"));
            freezeBox = Option("Freeze dump", "A dump when the server writes no speed line for " + FreezeSeconds + " s.");
            page.Controls.Add(Group("Speed", profileBox, timersBox));
            page.Controls.Add(Group("Session", packetsBox, recordBox));

            dumpNone = new RadioButton { Text = "None", AutoSize = true };
            dumpSmall = new RadioButton { Text = "Small: the stacks of the threads (about 1 MB)", AutoSize = true };
            dumpFull = new RadioButton { Text = "Full: all the memory (about 600 MB, less in the zip)", AutoSize = true };
            foreach (var r in new[] { dumpNone, dumpSmall, dumpFull })
                r.CheckedChanged += delegate { OnCaptureChanged(); };
            var crash = Group("Crash and freeze", new Label { Text = "Crash dump:", AutoSize = true }, dumpNone, dumpSmall, dumpFull, freezeBox);
            page.Controls.Add(crash);
            return page;
        }

        CheckBox Option(string text, string hint)
        {
            var box = new CheckBox { Text = text, AutoSize = true };
            tips.SetToolTip(box, hint);
            box.CheckedChanged += delegate { OnCaptureChanged(); };
            return box;
        }

        GroupBox Group(string title, params Control[] rows)
        {
            var box = new GroupBox { Text = title, AutoSize = true, MinimumSize = new Size(560, 0), Padding = new Padding(8, 6, 8, 4) };
            var list = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false };
            foreach (Control r in rows) {
                list.Controls.Add(r);
            }
            box.Controls.Add(list);
            return box;
        }

        void ShowCaptures()
        {
            settingCaptures = true;
            profileBox.Checked = captures.Profile;
            timersBox.Checked = captures.Timers;
            packetsBox.Checked = captures.Packets;
            recordBox.Checked = captures.Record;
            freezeBox.Checked = captures.FreezeDump;
            dumpNone.Checked = captures.CrashDump == "none";
            dumpSmall.Checked = captures.CrashDump == "small";
            dumpFull.Checked = captures.CrashDump == "full";
            settingCaptures = false;
            var preset = (Preset)presetBox.SelectedItem;
            presetText.Text = captures.Summary();
            tips.SetToolTip(presetBox, preset.Text);
        }

        void OnPreset()
        {
            var preset = (Preset)presetBox.SelectedItem;
            if (settingCaptures) return;
            captures = preset.Captures != null ? preset.Captures.Copy() : ReadCustomCaptures();
            ShowCaptures();
        }

        // A change on the Captures page makes the preset Custom, unless it gives a preset again.
        void OnCaptureChanged()
        {
            if (settingCaptures)
                return;
            captures = new Captures {
                Profile = profileBox.Checked, Timers = timersBox.Checked, Packets = packetsBox.Checked,
                Record = recordBox.Checked, FreezeDump = freezeBox.Checked,
                CrashDump = dumpFull.Checked ? "full" : dumpSmall.Checked ? "small" : "none"
            };
            var match = Presets.FirstOrDefault(p => p.Captures != null && p.Captures.Same(captures)) ?? Presets[Presets.Length - 1];
            settingCaptures = true;
            presetBox.SelectedItem = match;
            settingCaptures = false;
            var preset = match;
            presetText.Text = captures.Summary();
            tips.SetToolTip(presetBox, preset.Text);
        }

        Captures ReadCustomCaptures()
        {
            return new Captures {
                Record = settings.Get("custom.record", "0") == "1",
                Profile = settings.Get("custom.profile", "1") == "1", Timers = settings.Get("custom.timers", "0") == "1",
                Packets = settings.Get("custom.packets", "0") == "1", FreezeDump = settings.Get("custom.freeze", "1") == "1",
                CrashDump = settings.Get("custom.crashdump", "small")
            };
        }

        void SaveCaptureChoice()
        {
            var preset = (Preset)presetBox.SelectedItem;
            settings.Set("preset", preset.Name);
            if (preset.Captures == null) {
                settings.Set("custom.record", captures.Record ? "1" : "0");
                settings.Set("custom.profile", captures.Profile ? "1" : "0");
                settings.Set("custom.timers", captures.Timers ? "1" : "0");
                settings.Set("custom.packets", captures.Packets ? "1" : "0");
                settings.Set("custom.freeze", captures.FreezeDump ? "1" : "0");
                settings.Set("custom.crashdump", captures.CrashDump);
            }
            settings.Save();
        }

        // ------------------------------------------------------------ the server

        void StartServer()
        {
            if (server != null) return;
            developerRun = debugModeBox.Checked;
            activeCaptures = developerRun ? captures.Copy() : new Captures { CrashDump = "none" };
            string exe = Path.Combine(folder, activeCaptures.Record ? "subvulpes-server-record.exe" : "subvulpes-server.exe");
            if (!Directory.Exists(Path.Combine(folder, "data"))) {
                ShowMissing(false);
                return;
            }
            if (!File.Exists(exe)) {
                message.Text = Path.GetFileName(exe) + " is not in this folder.";
                return;
            }
            try {
                if (dirty) SaveSettings();
                if (developerRun) SaveCaptureChoice();
                string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss-fff");
                testDir = developerRun ? Path.Combine(logsRoot, stamp) : logsRoot;
                Directory.CreateDirectory(testDir);
                logFile = Path.Combine(testDir, "server.log");
                string eventName = @"Local\OpenSubRosa24-stop-" + Guid.NewGuid().ToString("N");
                stopEvent = new System.Threading.EventWaitHandle(false, System.Threading.EventResetMode.ManualReset, eventName);
                var start = new ProcessStartInfo(exe) {
                    WorkingDirectory = folder, UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardOutput = true, RedirectStandardError = true
                };
                var env = start.EnvironmentVariables;
                foreach (string key in new[] { "SV_SAMPLE", "SV_TIMERS", "SV_PACKETS", "SV_DUMP", "SV_DUMP_TYPE", "SV_TEST_CRASH", "SV_RECORD_DIR", "SV_SNAPSHOT_AFTER" })
                    env.Remove(key);
                env["SV_LOG"] = logFile;
                env["SV_STOP_EVENT"] = eventName;
                if (activeCaptures.Profile) env["SV_SAMPLE"] = Path.Combine(testDir, "profile.txt");
                if (activeCaptures.Timers) env["SV_TIMERS"] = "1";
                if (activeCaptures.Packets) env["SV_PACKETS"] = Path.Combine(testDir, "packets.bin");
                if (activeCaptures.Record) {
                    env["SV_RECORD_DIR"] = testDir;
                    env["SV_SNAPSHOT_AFTER"] = settings.Get("snapshot_after", "600");
                }
                if (activeCaptures.CrashDump != "none") {
                    env["SV_DUMP"] = Path.Combine(testDir, "crash.dmp");
                    env["SV_DUMP_TYPE"] = activeCaptures.CrashDump;
                }
                string testCrash = developerRun ? settings.Get("test_crash", "") : "";
                if (testCrash != "") env["SV_TEST_CRASH"] = testCrash;
                if (developerRun) {
                    var session = new KeyFile(Path.Combine(testDir, "session.ini"));
                    session.Set("preset", ((Preset)presetBox.SelectedItem).Name);
                    session.Set("captures", activeCaptures.Summary());
                    session.Set("start", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
                    session.Set("server", ServerVersion());
                    session.Save();
                }
                server = new Process { StartInfo = start, EnableRaisingEvents = true };
                serverStopped = new TaskCompletionSource<bool>();
                server.ErrorDataReceived += OnOutput;
                server.Exited += delegate { if (!IsDisposed) BeginInvoke(new Action(OnExited)); };
                stopping = crashed = frozen = false;
                marks = maxPlayers = 0;
                speeds.Clear();
                graph.Points.Clear();
                graph.Invalidate();
                console.Clear();
                ResetCards();
                logOffset = 0;
                pendingLog = "";
                // Remove the previous session before the server opens SV_LOG.
                if (File.Exists(logFile)) File.Delete(logFile);
                server.Start();
                server.BeginOutputReadLine();
                server.BeginErrorReadLine();
                started = lastSpeedLine = DateTime.Now;
                runPanel.Visible = true;
                endPanel.Visible = false;
                SetState("Starting");
                message.Text = developerRun ? "This test: logs\\" + stamp : "";
            } catch (Exception ex) {
                if (server != null) { server.Dispose(); server = null; }
                if (stopEvent != null) { stopEvent.Dispose(); stopEvent = null; }
                SetState("Stopped");
                message.Text = "Cannot start the server: " + ex.Message;
            }
        }

        string ServerVersion()
        {
            var info = new FileInfo(Path.Combine(folder, "subvulpes-server.exe"));
            return info.Exists ? info.LastWriteTime.ToString("yyyy-MM-dd HH:mm") + ", " + info.Length + " bytes" : "?";
        }

        async Task<bool> StopServer()
        {
            if (server == null) return true;
            if (!stopping) {
                stopping = true;
                stopEvent.Set();
                SetState("Stopping");
            }
            await Task.WhenAny(serverStopped.Task, Task.Delay(15000));
            if (server == null) return true;
            stopping = false;
            SetState("Stop timed out");
            message.Text = "The server did not stop within 15 seconds.";
            return false;
        }

        void OnOutput(object sender, DataReceivedEventArgs e)
        {
            if (e.Data != null && !IsDisposed)
                BeginInvoke(new Action<string>(AddLine), e.Data);
        }

        void ReadLog()
        {
            if (server == null || !File.Exists(logFile)) return;
            try {
                using (var stream = new FileStream(logFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete)) {
                    if (stream.Length < logOffset) { logOffset = 0; pendingLog = ""; }
                    stream.Position = logOffset;
                    using (var reader = new StreamReader(stream, Encoding.Default)) {
                        pendingLog += reader.ReadToEnd();
                        logOffset = stream.Position;
                    }
                }
                int end;
                while ((end = pendingLog.IndexOf('\n')) >= 0) {
                    AddLine(pendingLog.Substring(0, end).TrimEnd('\r'));
                    pendingLog = pendingLog.Substring(end + 1);
                }
            } catch (IOException ex) {
                message.Text = "Cannot read server.log: " + ex.Message;
            }
        }


        void AddLine(string line)
        {
            // A long test makes many lines. Keep the last part only: the log file has all.
            if (console.TextLength > 200000)
                console.Text = console.Text.Substring(100000);
            console.AppendText(RedactLog(line, config.Get("serverpassword", "")) + "\r\n");

            Match m = TicksLine.Match(line);
            if (m.Success) {
                double ticks = double.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture);
                speeds.Add(ticks);
                graph.Points.Add(ticks);
                graph.Invalidate();
                lastSpeedLine = DateTime.Now;
                frozen = false;
                SetCard(lastTicks, m.Groups[1].Value);
                lastTicks.ForeColor = SystemColors.ControlText;
                speedStatus = m.Groups[1].Value + " ticks/s";
                SetCard(lowTicks, speeds.Min().ToString("0.0", CultureInfo.InvariantCulture));
                if (m.Groups[2].Success) {
                    int players = int.Parse(m.Groups[2].Value);
                    maxPlayers = Math.Max(maxPlayers, players);
                    SetCard(playersText, players + (m.Groups[3].Value != "0" ? " (+" + m.Groups[3].Value + " bots)" : ""));
                    playersStatus = players + (players == 1 ? " player" : " players") +
                                         (m.Groups[3].Value != "0" ? ", " + m.Groups[3].Value + " bots" : "");
                }
                if (serverState == "Running" || serverState == "Frozen?")
                    SetState("Running");
            } else if (Regex.IsMatch(line, @"^Server started on port \d+\.$")) {
                lastSpeedLine = DateTime.Now;
                SetState("Running");
            } else if (line.Contains("snapshot saved")) {
                message.Text = "Recording complete. Memory snapshot saved.";
            } else if (line.Contains("CRASH")) {
                crashed = true;
            }
            RefreshStatus();
        }

        void OnClock()
        {
            if (server == null) return;
            ReadLog();
            TimeSpan t = DateTime.Now - started;
            SetCard(uptimeText, ((int)t.TotalHours).ToString("00") + ":" + t.Minutes.ToString("00") + ":" + t.Seconds.ToString("00"));
            if (server == null || frozen || (DateTime.Now - lastSpeedLine).TotalSeconds < FreezeSeconds)
                return;
            // The status line comes each minute. No line for a long time: the main loop stops.
            frozen = true;
            SetState("Frozen?");
            AddLine("[launcher] No speed line for " + FreezeSeconds + " s. The server can be frozen.");
            if (activeCaptures.FreezeDump)
                DumpNow("freeze");
        }

        void OnExited()
        {
            if (server == null) return;
            ReadLog();
            int code = server.ExitCode;
            server.Dispose();
            server = null;
            if (stopEvent != null) { stopEvent.Dispose(); stopEvent = null; }
            serverStopped.TrySetResult(true);
            string result;
            if (stopping) {
                AddLine("[launcher] You stopped the server.");
                SetState("Stopped");
                result = frozen ? "frozen" : "ok";
            } else {
                AddLine("[launcher] The server stopped by itself, exit code " + code + ".");
                SetState(crashed ? "Crashed" : "Stopped by itself");
                result = crashed ? "crash" : "stopped by itself";
            }
            if (!developerRun) return;
            TimeSpan t = DateTime.Now - started;
            var session = new KeyFile(Path.Combine(testDir, "session.ini"));
            session.Set("end", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            session.Set("minutes", ((int)t.TotalMinutes).ToString());
            session.Set("result", result);
            session.Set("marks", marks.ToString());
            session.Set("max_players", maxPlayers.ToString());
            if (speeds.Count > 0) {
                session.Set("speed_min", speeds.Min().ToString("0.0", CultureInfo.InvariantCulture));
                session.Set("speed_avg", speeds.Average().ToString("0.0", CultureInfo.InvariantCulture));
            }
            session.Save();

            runPanel.Visible = false;
            endPanel.Visible = true;
            endTitle.Text = "Test finished: " + (int)t.TotalMinutes + " min, " + result;
            endTitle.ForeColor = SystemColors.ControlText;
            endText.Text = (speeds.Count > 0
                                ? "Speed " + speeds.Min().ToString("0", CultureInfo.InvariantCulture) + " to " +
                                      speeds.Max().ToString("0", CultureInfo.InvariantCulture) + " ticks/s (normal 62). "
                                : "No speed line (the test was shorter than 1 minute). ") +
                           marks + (marks == 1 ? " mark" : " marks") + ". Most players: " + maxPlayers + ".";
            endNotes.Text = "";
            capturesText.Text = Files(testDir).Count + " files, " + FolderSize(testDir);
            if (debugModeBox.Checked) ShowPage("Debug");
        }

        // ------------------------------------------------------------ marks and dumps

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == 0x0312 /* WM_HOTKEY */ && (int)m.WParam == HotkeyId)
                AddMark();
            base.WndProc(ref m);
        }

        void AddMark()
        {
            if (server == null || !developerRun)
                return;
            marks++;
            SetCard(marksText, marks.ToString());
            TimeSpan t = DateTime.Now - started;
            string up = ((int)t.TotalHours).ToString("00") + ":" + t.Minutes.ToString("00") + ":" + t.Seconds.ToString("00");
            string line = DateTime.Now.ToString("HH:mm:ss") + " (running for " + up + ")";
            File.AppendAllText(Path.Combine(testDir, "marks.txt"), line + "\r\n");
            AddLine("[mark " + up + "] " + marks);
            SystemSounds.Asterisk.Play();
        }

        [DllImport("dbghelp.dll", SetLastError = true)]
        static extern bool MiniDumpWriteDump(IntPtr process, int processId, Microsoft.Win32.SafeHandles.SafeFileHandle file,
                                             int type, IntPtr exception, IntPtr userStream, IntPtr callback);

        // MiniDumpWithDataSegs | IndirectlyReferencedMemory | UnloadedModules | ThreadInfo, or
        // WithFullMemory | HandleData | UnloadedModules | ThreadInfo (dbghelp.h).
        const int SmallDump = 0x1 | 0x40 | 0x20 | 0x1000, FullDump = 0x2 | 0x4 | 0x20 | 0x1000;

        void DumpNow(string kind)
        {
            if (server == null)
                return;
            bool full = activeCaptures.CrashDump == "full";
            string file = Path.Combine(testDir, kind + "-" + DateTime.Now.ToString("HHmmss") + ".dmp");
            IntPtr handle = server.Handle;
            int pid = server.Id;
            AddLine("[launcher] Writing a " + (full ? "full" : "small") + " dump: " + Path.GetFileName(file));
            Task.Run(() => {
                bool ok;
                using (var f = new FileStream(file, FileMode.Create, FileAccess.Write))
                    ok = MiniDumpWriteDump(handle, pid, f.SafeFileHandle, full ? FullDump : SmallDump, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
                string size = (new FileInfo(file).Length / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture) + " MB";
                BeginInvoke(new Action<string>(AddLine), ok ? "[launcher] Dump ready (" + size + ")." : "[launcher] The dump failed.");
            });
        }

        // ------------------------------------------------------------ past tests

        Control TestsPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 5 };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));

            testsList = new ListView { Dock = DockStyle.Fill, View = View.Details, FullRowSelect = true, MultiSelect = false, HideSelection = false };
            foreach (var c in new[] { "Start", "Preset", "Minutes", "Speed (min / avg)", "Players", "Result", "Size" })
                testsList.Columns.Add(c, c == "Preset" ? 190 : c == "Start" ? 140 : c.StartsWith("Speed") ? 130 : 90);
            testsList.SelectedIndexChanged += delegate { ShowTestNotes(); };
            page.Controls.Add(testsList, 0, 1);
            page.Controls.Add(new Label { Text = "Notes of the selected test", AutoSize = true, Margin = new Padding(3, 8, 3, 2) }, 0, 2);
            testNotes = new TextBox { Multiline = true, Height = 60, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical };
            page.Controls.Add(testNotes, 0, 3);
            var bar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 6, 0, 0) };
            var pack = new Button { Text = "Pack the selected test", AutoSize = true };
            var open = new Button { Text = "Open its folder", AutoSize = true };
            var save = new Button { Text = "Save the notes", AutoSize = true };
            pack.Click += delegate { string d = SelectedTest(); if (d != null) { SaveNotes(d, testNotes.Text); PackTest(d); } };
            open.Click += delegate { string d = SelectedTest(); if (d != null) Process.Start("explorer.exe", "\"" + d + "\""); };
            save.Click += delegate { string d = SelectedTest(); if (d != null) SaveNotes(d, testNotes.Text); };
            bar.Controls.AddRange(new Control[] { pack, open, save });
            page.Controls.Add(bar, 0, 4);
            return page;
        }

        void LoadTests()
        {
            testsList.Items.Clear();
            if (!Directory.Exists(logsRoot))
                return;
            foreach (string dir in Directory.GetDirectories(logsRoot).OrderByDescending(d => d)) {
                var s = new KeyFile(Path.Combine(dir, "session.ini"));
                if (!s.Exists)
                    continue;
                var item = new ListViewItem(new[] {
                    s.Get("start", Path.GetFileName(dir)), s.Get("preset", "?"), s.Get("minutes", "running"),
                    s.Get("speed_min", "-") + " / " + s.Get("speed_avg", "-"), s.Get("max_players", "-"),
                    s.Get("result", server != null && dir == testDir ? "running" : "?"), FolderSize(dir)
                }) { Tag = dir };
                testsList.Items.Add(item);
            }
            if (testsList.Items.Count > 0)
                testsList.Items[0].Selected = true;
        }

        string SelectedTest()
        {
            return testsList.SelectedItems.Count == 0 ? null : (string)testsList.SelectedItems[0].Tag;
        }

        void ShowTestNotes()
        {
            string d = SelectedTest();
            string notes = d == null ? null : Path.Combine(d, "notes.txt");
            testNotes.Text = notes != null && File.Exists(notes) ? File.ReadAllText(notes) : "";
        }

        static void SaveNotes(string dir, string text)
        {
            if (dir != null && text.Trim() != "")
                File.WriteAllText(Path.Combine(dir, "notes.txt"), text);
        }

        static List<string> Files(string dir)
        {
            return Directory.Exists(dir) ? Directory.GetFiles(dir).ToList() : new List<string>();
        }

        static string FolderSize(string dir)
        {
            long bytes = Files(dir).Sum(f => new FileInfo(f).Length);
            return (bytes / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture) + " MB";
        }

        // One zip for each test, next to the test folder. The server can still write its files:
        // read each one with shared access.
        void PackTest(string dir)
        {
            if (dir == null || Files(dir).Count == 0) {
                MessageBox.Show(this, "This test has no files.", Text);
                return;
            }
            string zip = Path.Combine(logsRoot, "test-" + Path.GetFileName(dir) + ".zip");
            message.Text = "Packing " + Path.GetFileName(zip) + "...";
            Task.Run(() => {
                string error = null;
                try {
                    if (File.Exists(zip))
                        File.Delete(zip);
                    using (var archive = ZipFile.Open(zip, ZipArchiveMode.Create))
                        foreach (string file in Files(dir)) {
                            var entry = archive.CreateEntry(Path.GetFileName(dir) + "/" + Path.GetFileName(file), CompressionLevel.Optimal);
                            using (var src = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                            using (var dst = entry.Open())
                                src.CopyTo(dst);
                        }
                } catch (IOException ex) {
                    error = ex.Message;
                }
                BeginInvoke(new Action(() => {
                    if (error != null) {
                        message.Text = "";
                        MessageBox.Show(this, "Cannot make the zip: " + error, Text);
                        return;
                    }
                    message.Text = "Saved: logs\\" + Path.GetFileName(zip) + " (" +
                                  (new FileInfo(zip).Length / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture) + " MB)";
                    Process.Start("explorer.exe", "/select,\"" + zip + "\"");
                }));
            });
        }

        // ------------------------------------------------------------ update

        public static string RedactLog(string text, string password)
        {
            text = Regex.Replace(text, @"/admin [^\r\n]*", "/admin [redacted]");
            return password.Length > 0 ? text.Replace(password, "[redacted]") : text;
        }

        public static void CopyLog(string source, string destination, string password)
        {
            if (Path.GetFullPath(source).Equals(Path.GetFullPath(destination), StringComparison.OrdinalIgnoreCase))
                throw new IOException("Choose another file for the saved log.");
            using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (var reader = new StreamReader(input, Encoding.Default))
            using (var output = new FileStream(destination, FileMode.Create, FileAccess.Write))
            using (var writer = new StreamWriter(output, new UTF8Encoding(false)))
                writer.Write(RedactLog(reader.ReadToEnd(), password));
        }

        async Task SaveLogTo(string destination)
        {
            string source = logFile;
            try {
                string password = new KeyFile(Path.Combine(folder, "config.txt")).Get("serverpassword", "");
                await Task.Run(() => CopyLog(source, destination, password));
                message.Text = "Saved: " + destination;
            } catch (Exception ex) {
                message.Text = "Cannot save the log: " + ex.Message;
            }
        }

        async Task SaveLog()
        {
            if (!File.Exists(logFile)) { message.Text = "server.log is missing."; return; }
            using (var dialog = new SaveFileDialog { Filter = "Log files (*.log)|*.log|All files (*.*)|*.*", FileName = "server.log", DefaultExt = "log" })
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    await SaveLogTo(dialog.FileName);
        }

        async Task CheckForUpdates()
        {
            if (updating) return;
            updating = true;
            checkUpdate.Enabled = false;
            SetState(serverState);
            string zip = null, releaseName = null;
            try {
                string file = Path.Combine(folder, "version.txt");
                if (!File.Exists(file)) throw new InvalidDataException("version.txt is missing.");
                string version = File.ReadLines(file).FirstOrDefault();
                bool previews = previewsBox.Checked;
                var release = await Task.Run(() => ReleaseUpdate.Find((version ?? "").Trim(), previews));
                if (release == null) {
                    message.Text = "No newer release.";
                    return;
                }
                releaseName = release.DisplayName;
                message.Text = releaseName;
                string directory = Path.Combine(logsRoot, "updates", Guid.NewGuid().ToString("N"));
                zip = await Task.Run(() => ReleaseUpdate.Download(release, directory));
            } catch (WebException) {
                message.Text = "Cannot check for updates: no network connection or GitHub is unavailable.";
            } catch (Exception ex) {
                message.Text = "Cannot update: " + ex.Message;
            } finally {
                updating = false;
                checkUpdate.Enabled = true;
                SetState(serverState);
            }
            if (zip != null) await InstallUpdate(zip, releaseName);
        }

        static string ZipOf(DragEventArgs e)
        {
            var files = e.Data.GetData(DataFormats.FileDrop) as string[];
            return files != null && files.Length == 1 && files[0].EndsWith(".zip", StringComparison.OrdinalIgnoreCase) ? files[0] : null;
        }

        async Task ChooseUpdate()
        {
            using (var dialog = new OpenFileDialog { Filter = "Update zip (*.zip)|*.zip", Title = "Choose the update zip" })
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    await InstallUpdate(dialog.FileName);
        }

        // Only the files of UpdateFiles come from the zip, from any folder inside it. The old
        // files go to backup\<time>. config.txt, data and user never change. The running
        // launcher cannot be overwritten, but Windows lets it be renamed: it becomes
        // subvulpes-launcher.old, and the next start deletes it.
        public async Task InstallUpdate(string zip, string releaseName = null)
        {
            if (updating) return;
            var found = new Dictionary<string, ZipArchiveEntry>(StringComparer.OrdinalIgnoreCase);
            ZipArchive archive;
            try {
                archive = ZipFile.OpenRead(zip);
            } catch (Exception ex) {
                MessageBox.Show(this, "Cannot read the zip: " + ex.Message, Text);
                return;
            }
            using (archive) {
                foreach (var entry in archive.Entries) {
                    if (!UpdateFiles.Contains(entry.Name, StringComparer.OrdinalIgnoreCase)) continue;
                    if (found.ContainsKey(entry.Name)) {
                        message.Text = "The zip contains duplicate update files.";
                        return;
                    }
                    found[entry.Name] = entry;
                }
                if (found.Count == 0) {
                    MessageBox.Show(this, "The zip has no update file (" + string.Join(", ", UpdateFiles) + ").", Text);
                    return;
                }
                string version = "";
                if (found.ContainsKey("version.txt"))
                    using (var r = new StreamReader(found["version.txt"].Open()))
                        version = " " + r.ReadLine();
                string question = "Install " + (releaseName ?? version.Trim()) + "?";
                if (MessageBox.Show(this, question, Text, MessageBoxButtons.YesNo) != DialogResult.Yes)
                    return;
                updating = true;
                if (!await StopServer()) { updating = false; return; }

                string backup = Path.Combine(folder, "backup", DateTime.Now.ToString("yyyyMMdd-HHmmss"));
                Directory.CreateDirectory(backup);
                bool newLauncher = false;
                try {
                    foreach (var f in found) {
                        string target = f.Key.Equals("subvulpes-launcher.exe", StringComparison.OrdinalIgnoreCase)
                            ? Application.ExecutablePath : Path.Combine(folder, f.Key);
                        if (File.Exists(target))
                            File.Copy(target, Path.Combine(backup, f.Key), true);
                        if (f.Key.Equals("subvulpes-launcher.exe", StringComparison.OrdinalIgnoreCase)) {
                            string old = Path.Combine(launcherFolder, "subvulpes-launcher.old");
                            if (File.Exists(old))
                                File.Delete(old);
                            File.Move(Application.ExecutablePath, old);
                            newLauncher = true;
                        }
                        f.Value.ExtractToFile(target, true);
                    }
                } catch (Exception ex) {
                    updating = false;
                    MessageBox.Show(this, "The update stopped: " + ex.Message + "\r\n\r\nThe old files are in " + backup + ".", Text);
                    return;
                }
                if (newLauncher) {
                    MessageBox.Show(this, "Update installed" + version + ". The launcher starts again now.", Text);
                    updating = false;
                    Process.Start(Application.ExecutablePath);
                    Close();
                    return;
                }
                Text = "OpenSubRosa24 Server" + Version();
                updating = false;
                message.Text = "Update installed" + version + ". Old files: backup\\" + Path.GetFileName(backup);
            }
        }

        void CleanOldLauncher()
        {
            string old = Path.Combine(launcherFolder, "subvulpes-launcher.old");
            try {
                if (File.Exists(old))
                    File.Delete(old);
            } catch (IOException) {
                // The old launcher still closes. The next start deletes it.
            } catch (UnauthorizedAccessException) {
            }
        }

        // ------------------------------------------------------------ settings

        Control SettingsPage()
        {
            var page = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true };

            nameBox = new TextBox { Width = 300, MaxLength = 49 };
            passwordBox = new TextBox { Width = 220, MaxLength = 49, UseSystemPasswordChar = true };
            var password = new FlowLayoutPanel { AutoSize = true, Margin = Padding.Empty };
            password.Controls.Add(passwordBox);
            var show = new CheckBox { Text = "Show", AutoSize = true, Anchor = AnchorStyles.Left };
            show.CheckedChanged += delegate { passwordBox.UseSystemPasswordChar = !show.Checked; };
            password.Controls.Add(show);
            welcomeBox = new TextBox { Width = 300, Height = 54, Multiline = true, ScrollBars = ScrollBars.Vertical };
            page.Controls.Add(SettingsGroup("Server", new[] {
                Row("Name", nameBox, null),
                Row("Admin password", password, "In the game, type /admin and this password to become admin."),
                Row("Welcome text", welcomeBox, "The players see it when they join (readme.txt)."),
            }));

            modeBox = new ComboBox { Width = 160, DropDownStyle = ComboBoxStyle.DropDownList };
            playersBox = new NumericUpDown { Width = 80, Minimum = 2, Maximum = 128 };
            roundBox = new NumericUpDown { Width = 80, Minimum = 1, Maximum = 30 };
            modeBox.SelectedIndexChanged += delegate { var m = modeBox.SelectedItem as Mode; roundBox.Enabled = m != null && m.Value == "3"; };
            page.Controls.Add(SettingsGroup("Game", new[] {
                Row("Mode", modeBox, null),
                Row("Max players", playersBox, null),
                Row("Round time", roundBox, "Minutes in Round mode."),
            }));

            portBox = new NumericUpDown { Width = 100, Minimum = 1, Maximum = 65535 };
            bytesBox = new NumericUpDown { Width = 120, Minimum = 0, Maximum = 100000000, Increment = 100000, ThousandsSeparator = true };
            page.Controls.Add(SettingsGroup("Network", new[] {
                Row("Port", portBox, "Open this UDP port in the router."),
                Row("Upload limit", bytesBox, "Bytes per second for all the players."),
            }));

            var bar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 6, 0, 0) };
            saveButton = new Button { Text = "Save", AutoSize = true };
            undoButton = new Button { Text = "Undo", AutoSize = true };
            dirtyText = new Label { AutoSize = true, Anchor = AnchorStyles.Left };
            saveButton.Click += delegate { SaveSettings(); };
            undoButton.Click += delegate { config.Load(); LoadSettings(); };
            bar.Controls.AddRange(new Control[] { saveButton, undoButton, dirtyText });
            page.Controls.Add(bar);


            foreach (Control c in new Control[] { nameBox, passwordBox, welcomeBox, modeBox, playersBox, roundBox, portBox, bytesBox }) {
                c.TextChanged += delegate { SetDirty(true); };
                var n = c as NumericUpDown;
                if (n != null)
                    n.ValueChanged += delegate { SetDirty(true); };
            }
            modeBox.SelectedIndexChanged += delegate { SetDirty(true); };
            return page;
        }

        GroupBox SettingsGroup(string title, Control[] rows)
        {
            var box = new GroupBox { Text = title, AutoSize = true, MinimumSize = new Size(560, 0), Padding = new Padding(8, 6, 8, 4) };
            var grid = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 1 };
            foreach (Control r in rows)
                grid.Controls.Add(r);
            box.Controls.Add(grid);
            return box;
        }

        // A label column of fixed width and the field. The hint is a tooltip on both.
        Control Row(string label, Control field, string hint)
        {
            var row = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Margin = new Padding(0, 0, 0, 4) };
            row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 120));
            row.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var name = new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left | AnchorStyles.Top, Margin = new Padding(3, 6, 3, 3) };
            row.Controls.Add(name, 0, 0);
            row.Controls.Add(field, 1, 0);
            if (hint != null) {
                Tip(name, hint);
                Tip(field, hint);
            }
            return row;
        }

        // A tooltip on a panel does not show over its children, so each child gets it too.
        void Tip(Control c, string text)
        {
            tips.SetToolTip(c, text);
            foreach (Control child in c.Controls)
                Tip(child, text);
        }

        static decimal Number(string text, decimal fallback, NumericUpDown box)
        {
            decimal v;
            if (!decimal.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out v))
                v = fallback;
            return Math.Max(box.Minimum, Math.Min(box.Maximum, Math.Round(v)));
        }

        void LoadSettings()
        {
            loading = true;
            nameBox.Text = config.Get("servername", "");
            passwordBox.Text = config.Get("serverpassword", "");
            string readme = Path.Combine(folder, "readme.txt");
            welcomeBox.Text = File.Exists(readme) ? File.ReadAllText(readme, Encoding.Default).TrimEnd() : "";

            string type = config.Get("gametype", "3");
            modeBox.Items.Clear();
            modeBox.Items.Add(new Mode("3", "Round"));
            modeBox.Items.Add(new Mode("4", "World"));
            if (type != "3" && type != "4")
                modeBox.Items.Add(new Mode(type, "Mode " + type));
            foreach (Mode m in modeBox.Items)
                if (m.Value == type)
                    modeBox.SelectedItem = m;

            playersBox.Value = Number(config.Get("maxnumofplayers", "16"), 16, playersBox);
            roundBox.Value = Number(config.Get("roundtime", "7"), 7, roundBox);
            portBox.Value = Number(config.Get("port", "27584"), 27584, portBox);
            bytesBox.Value = Number(config.Get("servermaxbytespersecond", "100000"), 100000, bytesBox);
            loading = false;
            SetDirty(false);
        }

        void SaveSettings()
        {
            // Only the server keys change. The client keys (voice and others) stay as they are.
            config.Set("servername", nameBox.Text);
            config.Set("serverpassword", passwordBox.Text);
            config.Set("gametype", ((Mode)modeBox.SelectedItem).Value);
            config.Set("maxnumofplayers", playersBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Set("roundtime", roundBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Set("port", portBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Set("servermaxbytespersecond", bytesBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Save();
            File.WriteAllText(Path.Combine(folder, "readme.txt"), welcomeBox.Text, Encoding.Default);
            SetDirty(false);
            dirtyText.Text = server == null ? "Saved." : "Saved. Stop and start the server to use the changes.";
            dirtyText.ForeColor = SystemColors.ControlText;
        }

        void SetDirty(bool value)
        {
            if (loading)
                return;
            dirty = value;
            saveButton.Enabled = value;
            undoButton.Enabled = value;
            pages["Server settings"].Text = value ? "Server settings *" : "Server settings";
            dirtyText.Text = value ? "Unsaved changes." : "";
            dirtyText.ForeColor = SystemColors.ControlText;
        }



        // ------------------------------------------------------------ close

        async void OnClosing(object sender, FormClosingEventArgs e)
        {
            if (updating) { e.Cancel = true; return; }
            if (closeAfterStop) return;
            if (dirty) {
                var answer = MessageBox.Show(this, "Save the changed settings?", Text, MessageBoxButtons.YesNoCancel);
                if (answer == DialogResult.Cancel) { e.Cancel = true; return; }
                if (answer == DialogResult.Yes) SaveSettings();
            }
            if (developerRun && testDir != null && endPanel.Visible)
                SaveNotes(testDir, endNotes.Text);
            if (server == null) return;
            e.Cancel = true;
            if (await StopServer()) {
                closeAfterStop = true;
                Close();
            }
        }

        protected override bool ProcessCmdKey(ref Message message, Keys keyData)
        {
            if (keyData == (Keys.Control | Keys.Shift | Keys.D)) {
                debugModeBox.Checked = !debugModeBox.Checked;
                ShowPage(debugModeBox.Checked ? "Debug" : "Server");

                return true;
            }
            return base.ProcessCmdKey(ref message, keyData);
        }

        [DllImport("user32.dll")]
        static extern bool RegisterHotKey(IntPtr hWnd, int id, int modifiers, int key);

        [DllImport("user32.dll")]
        static extern bool UnregisterHotKey(IntPtr hWnd, int id);

        [DllImport("user32.dll")]
        static extern bool SetProcessDPIAware();

        [STAThread]
        static void Main(string[] args)
        {
            SetProcessDPIAware();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            var form = new LauncherForm(AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\'));
            // A zip dropped on the exe icon: install it when the window shows.
            if (args.Length == 1 && args[0].EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
                form.Shown += async delegate { await form.InstallUpdate(args[0]); };
            Application.Run(form);
        }
    }
}
