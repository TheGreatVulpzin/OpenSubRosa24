// SubVulpes test launcher: starts the standalone server for a test, shows its console,
// edits the server keys of config.txt and packs the logs for the developers.
//
// Build: tools\launcher\build.bat. It uses the C# compiler of .NET Framework 4, which is
// part of Windows, so the testers do not install anything. C# 5 only (no $"" strings).
// Put the exe in the server folder, next to subvulpes-server.exe.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

namespace SubVulpes
{
    // A key=value file (config.txt, launcher.ini). Save keeps the order of the lines and the
    // lines without a key. The server reads its files in the ANSI code page.
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

    // One item of the game mode list: the gametype value and its name.
    class Mode
    {
        public readonly string Value, Name;
        public Mode(string value, string name) { Value = value; Name = name; }
        public override string ToString() { return Name; }
    }

    class LauncherForm : Form
    {
        static readonly Regex TicksLine = new Regex(@"\(([0-9.]+) ticks/s\)");
        static readonly Color Gray = Color.FromArgb(110, 110, 110);
        static readonly Color Green = Color.FromArgb(30, 140, 60);
        static readonly Color Orange = Color.FromArgb(215, 120, 0);
        static readonly Color Red = Color.FromArgb(200, 40, 40);

        readonly string folder;
        readonly KeyFile config, settings;
        readonly ToolTip tips = new ToolTip();
        TabControl tabs;
        TabPage serverTab, settingsTab;

        // Server tab
        Label statusDot, statusText, summary, speedText, uptimeText, footer;
        Button startStop;
        CheckBox profileBox, timersBox;
        TextBox console;
        Process server;
        DateTime started;
        bool stopping, crashed;
        readonly Timer clock = new Timer { Interval = 1000 };

        // Settings tab
        TextBox nameBox, passwordBox, welcomeBox;
        ComboBox modeBox;
        NumericUpDown playersBox, roundBox, portBox, bytesBox;
        Label dirtyText;
        Button saveButton, undoButton;
        bool loading, dirty;

        public LauncherForm(string folder)
        {
            this.folder = folder;
            config = new KeyFile(Path.Combine(folder, "config.txt"));
            settings = new KeyFile(Path.Combine(folder, "launcher.ini"));
            Text = "SubVulpes test launcher";
            Font = SystemFonts.MessageBoxFont;
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(820, 640);
            MinimumSize = new Size(640, 480);

            tabs = new TabControl { Dock = DockStyle.Fill, Padding = new Point(14, 5) };
            serverTab = new TabPage("Server") { Padding = new Padding(10) };
            settingsTab = new TabPage("Settings") { Padding = new Padding(10) };
            serverTab.Controls.Add(ServerPage());
            settingsTab.Controls.Add(SettingsPage());
            tabs.TabPages.Add(serverTab);
            tabs.TabPages.Add(settingsTab);
            Controls.Add(tabs);

            LoadSettings();
            SetState("Stopped", Gray);
            UpdateSummary();
            clock.Tick += delegate { ShowUptime(); };
            FormClosing += OnClosing;
            Shown += delegate { startStop.Focus(); };
        }

        static Label Hint(string text)
        {
            return new Label { Text = text, AutoSize = true, ForeColor = Gray, Margin = new Padding(3, 0, 3, 8) };
        }

        // ------------------------------------------------------------ server tab

        Control ServerPage()
        {
            var page = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5 };
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));

            // The state, and the settings that the next start uses.
            var head = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, ColumnCount = 2 };
            head.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            head.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var big = new Font(Font.FontFamily, Font.Size * 1.6f, FontStyle.Bold);
            statusDot = new Label { Text = "●", Font = big, AutoSize = true, Margin = new Padding(0, 0, 4, 0) };
            statusText = new Label { Font = big, AutoSize = true, Anchor = AnchorStyles.Left };
            var line = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, Margin = Padding.Empty };
            summary = new Label { AutoSize = true, ForeColor = Gray, Margin = new Padding(3, 0, 6, 0) };
            var change = new LinkLabel { Text = "Change settings", AutoSize = true, Margin = Padding.Empty };
            change.LinkClicked += delegate { tabs.SelectedTab = settingsTab; };
            line.Controls.Add(summary);
            line.Controls.Add(change);
            head.Controls.Add(statusDot, 0, 0);
            head.Controls.Add(statusText, 1, 0);
            head.Controls.Add(line, 1, 1);
            page.Controls.Add(head, 0, 0);

            // The main button and the captures for the next start.
            var bar = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, Margin = new Padding(0, 10, 0, 6) };
            startStop = new Button { AutoSize = true, Padding = new Padding(16, 4, 16, 4), Font = new Font(Font, FontStyle.Bold) };
            startStop.Click += delegate { if (server == null) StartServer(); else StopServer(); };
            profileBox = new CheckBox { Text = "Speed profile", AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(16, 3, 3, 3) };
            timersBox = new CheckBox { Text = "Game timers", AutoSize = true, Anchor = AnchorStyles.Left };
            tips.SetToolTip(profileBox, "Records the code position every millisecond (about 30 MB for each hour). It shows where the server is slow.");
            tips.SetToolTip(timersBox, "Writes the time of each part of the game to the log each minute. Use it only when Vulpzin asks.");
            profileBox.Checked = settings.Get("profile", "1") == "1";
            timersBox.Checked = settings.Get("timers", "0") == "1";
            bar.Controls.AddRange(new Control[] { startStop, profileBox, timersBox });
            page.Controls.Add(bar, 0, 1);

            var speed = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, Margin = new Padding(0, 0, 0, 6) };
            speedText = new Label { AutoSize = true, Font = new Font(Font, FontStyle.Bold) };
            uptimeText = new Label { AutoSize = true, ForeColor = Gray, Margin = new Padding(12, 0, 3, 0) };
            speed.Controls.Add(speedText);
            speed.Controls.Add(uptimeText);
            page.Controls.Add(speed, 0, 2);

            console = new TextBox {
                Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Both, WordWrap = false, Dock = DockStyle.Fill,
                Font = new Font("Consolas", 9.5f), BackColor = Color.FromArgb(24, 24, 24), ForeColor = Color.Gainsboro
            };
            page.Controls.Add(console, 0, 3);

            var bottom = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, Margin = new Padding(0, 6, 0, 0) };
            var openLogs = new Button { Text = "Open logs folder", AutoSize = true };
            var pack = new Button { Text = "Pack logs to send", AutoSize = true };
            openLogs.Click += delegate { OpenLogs(); };
            pack.Click += delegate { PackLogs(); };
            footer = new Label { AutoSize = true, ForeColor = Gray, Anchor = AnchorStyles.Left };
            bottom.Controls.AddRange(new Control[] { openLogs, pack, footer });
            page.Controls.Add(bottom, 0, 4);
            return page;
        }

        void SetState(string text, Color color)
        {
            statusText.Text = text;
            statusDot.ForeColor = color;
            startStop.Text = server == null ? "Start server" : "Stop server";
            profileBox.Enabled = server == null;
            timersBox.Enabled = server == null;
        }

        void UpdateSummary()
        {
            if (!config.Exists) {
                summary.Text = "config.txt is not in this folder.";
                return;
            }
            var mode = modeBox.SelectedItem as Mode;
            summary.Text = nameBox.Text + "  |  " + (mode != null ? mode.Name : "?") + "  |  port " + portBox.Value +
                           "  |  " + playersBox.Value + " players";
        }

        void ShowUptime()
        {
            TimeSpan t = DateTime.Now - started;
            uptimeText.Text = "Running for " + ((int)t.TotalHours).ToString("00") + ":" + t.Minutes.ToString("00") + ":" + t.Seconds.ToString("00");
        }

        void StartServer()
        {
            string exe = Path.Combine(folder, "subvulpes-server.exe");
            if (!File.Exists(exe)) {
                MessageBox.Show(this, "subvulpes-server.exe is not in this folder.", Text);
                return;
            }
            if (dirty)
                SaveSettings();
            settings.Set("profile", profileBox.Checked ? "1" : "0");
            settings.Set("timers", timersBox.Checked ? "1" : "0");
            settings.Save();

            Directory.CreateDirectory(Path.Combine(folder, "logs"));
            string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss");
            var start = new ProcessStartInfo(exe) {
                WorkingDirectory = folder, UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true
            };
            var env = start.EnvironmentVariables;
            env["SV_LOG"] = Path.Combine("logs", "standalone-" + stamp + ".log");
            env.Remove("SV_SAMPLE");
            env.Remove("SV_TIMERS");
            if (profileBox.Checked)
                env["SV_SAMPLE"] = Path.Combine("logs", "profile-" + stamp + ".txt");
            if (timersBox.Checked)
                env["SV_TIMERS"] = "1";

            server = new Process { StartInfo = start, EnableRaisingEvents = true };
            server.OutputDataReceived += OnOutput;
            server.ErrorDataReceived += OnOutput;
            server.Exited += delegate { BeginInvoke(new Action(OnExited)); };
            stopping = crashed = false;
            console.Clear();
            server.Start();
            server.BeginOutputReadLine();
            server.BeginErrorReadLine();
            started = DateTime.Now;
            clock.Start();
            ShowUptime();
            SetState("Starting", Orange);
            speedText.Text = "The first speed value comes after 1 minute.";
            speedText.ForeColor = Gray;
            footer.Text = "Log: logs\\standalone-" + stamp + ".log";
        }

        void StopServer()
        {
            if (server == null)
                return;
            stopping = true;
            try {
                server.Kill();
            } catch (InvalidOperationException) {
                // It stopped before the click.
            }
        }

        void OnOutput(object sender, DataReceivedEventArgs e)
        {
            if (e.Data != null)
                BeginInvoke(new Action<string>(AddLine), e.Data);
        }

        void AddLine(string line)
        {
            // A long test makes many lines. Keep the last part only: the log file has all.
            if (console.TextLength > 200000)
                console.Text = console.Text.Substring(100000);
            console.AppendText(line + "\r\n");

            Match m = TicksLine.Match(line);
            if (m.Success) {
                double ticks = double.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture);
                speedText.Text = "Speed: " + m.Groups[1].Value + " ticks/s in the last minute (normal: about 62)";
                speedText.ForeColor = ticks < 55 ? Orange : Green;
            } else if (line.Contains("own main loop: running")) {
                SetState("Running", Green);
                speedText.Text = "The server shows in the game list after about 40 s.";
            } else if (line.Contains("CRASH")) {
                crashed = true;
            }
        }

        void OnExited()
        {
            int code = server.ExitCode;
            server.Dispose();
            server = null;
            clock.Stop();
            if (stopping) {
                AddLine("[launcher] You stopped the server.");
                SetState("Stopped", Gray);
                return;
            }
            AddLine("[launcher] The server stopped, exit code " + code + ".");
            SetState(crashed ? "Crashed" : "Stopped by itself", Red);
            speedText.Text = "Click Pack logs to send, and send the zip to Vulpzin.";
            speedText.ForeColor = Red;
        }

        // ------------------------------------------------------------ settings tab

        Control SettingsPage()
        {
            var page = new FlowLayoutPanel {
                Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true
            };

            nameBox = new TextBox { Width = 300 };
            passwordBox = new TextBox { Width = 220, UseSystemPasswordChar = true };
            var show = new CheckBox { Text = "Show", AutoSize = true, Anchor = AnchorStyles.Left };
            show.CheckedChanged += delegate { passwordBox.UseSystemPasswordChar = !show.Checked; };
            var password = new FlowLayoutPanel { AutoSize = true, Margin = Padding.Empty };
            password.Controls.Add(passwordBox);
            password.Controls.Add(show);
            welcomeBox = new TextBox { Width = 300, Height = 54, Multiline = true, ScrollBars = ScrollBars.Vertical };
            page.Controls.Add(Group("Server", new[] {
                Row("Name", nameBox, "The name in the server list."),
                Row("Password", password, "Empty: everybody can join."),
                Row("Welcome text", welcomeBox, "The players see it when they join (readme.txt)."),
            }));

            modeBox = new ComboBox { Width = 160, DropDownStyle = ComboBoxStyle.DropDownList };
            playersBox = new NumericUpDown { Width = 80, Minimum = 1, Maximum = 255 };
            roundBox = new NumericUpDown { Width = 80, Minimum = 1, Maximum = 999 };
            modeBox.SelectedIndexChanged += delegate { var m = modeBox.SelectedItem as Mode; roundBox.Enabled = m != null && m.Value == "3"; };
            page.Controls.Add(Group("Game", new[] {
                Row("Mode", modeBox, null),
                Row("Max players", playersBox, null),
                Row("Round time", roundBox, "Only for the Round mode."),
            }));

            portBox = new NumericUpDown { Width = 100, Minimum = 1, Maximum = 65535 };
            bytesBox = new NumericUpDown { Width = 120, Minimum = 0, Maximum = 100000000, Increment = 100000, ThousandsSeparator = true };
            page.Controls.Add(Group("Network", new[] {
                Row("Port", portBox, "Open this UDP port in the router."),
                Row("Upload limit", bytesBox, "Bytes per second for all the players."),
            }));

            var bar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 6, 0, 0) };
            saveButton = new Button { Text = "Save", AutoSize = true, Font = new Font(Font, FontStyle.Bold) };
            undoButton = new Button { Text = "Undo changes", AutoSize = true };
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

        GroupBox Group(string title, Control[] rows)
        {
            var box = new GroupBox { Text = title, AutoSize = true, MinimumSize = new Size(520, 0), Padding = new Padding(8, 6, 8, 4) };
            var grid = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 1 };
            foreach (Control r in rows)
                grid.Controls.Add(r);
            box.Controls.Add(grid);
            return box;
        }

        // A label column of fixed width, the field, and a gray hint under the field.
        Control Row(string label, Control field, string hint)
        {
            var row = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Margin = Padding.Empty };
            row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 110));
            row.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            row.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left | AnchorStyles.Top, Margin = new Padding(3, 6, 3, 3) }, 0, 0);
            row.Controls.Add(field, 1, 0);
            if (hint != null)
                row.Controls.Add(Hint(hint), 1, 1);
            return row;
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

            string type = config.Get("gametype", "4");
            modeBox.Items.Clear();
            modeBox.Items.Add(new Mode("3", "Round"));
            modeBox.Items.Add(new Mode("4", "World"));
            if (type != "3" && type != "4")
                modeBox.Items.Add(new Mode(type, "Mode " + type));
            foreach (Mode m in modeBox.Items)
                if (m.Value == type)
                    modeBox.SelectedItem = m;

            playersBox.Value = Number(config.Get("maxnumofplayers", "24"), 24, playersBox);
            roundBox.Value = Number(config.Get("roundtime", "30"), 30, roundBox);
            portBox.Value = Number(config.Get("port", "27584"), 27584, portBox);
            bytesBox.Value = Number(config.Get("servermaxbytespersecond", "1000000"), 1000000, bytesBox);
            loading = false;
            SetDirty(false);
            UpdateSummary();
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
            dirtyText.ForeColor = server == null ? Green : Orange;
            UpdateSummary();
        }

        void SetDirty(bool value)
        {
            if (loading)
                return;
            dirty = value;
            saveButton.Enabled = value;
            undoButton.Enabled = value;
            settingsTab.Text = value ? "Settings *" : "Settings";
            dirtyText.Text = value ? "Unsaved changes. Start server also saves them." : "";
            dirtyText.ForeColor = Orange;
        }

        // ------------------------------------------------------------ logs

        void OpenLogs()
        {
            string logs = Path.Combine(folder, "logs");
            Directory.CreateDirectory(logs);
            Process.Start("explorer.exe", "\"" + logs + "\"");
        }

        void PackLogs()
        {
            string logs = Path.Combine(folder, "logs");
            if (!Directory.Exists(logs) || Directory.GetFiles(logs).Length == 0) {
                MessageBox.Show(this, "The logs folder is empty. Start the server first.", Text);
                return;
            }
            string zip = Path.Combine(folder, "logs-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + ".zip");
            try {
                // The server keeps its files open while it runs. Copy each file with shared read.
                using (var archive = ZipFile.Open(zip, ZipArchiveMode.Create)) {
                    foreach (string file in Directory.GetFiles(logs)) {
                        var entry = archive.CreateEntry("logs/" + Path.GetFileName(file), CompressionLevel.Optimal);
                        using (var src = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                        using (var dst = entry.Open())
                            src.CopyTo(dst);
                    }
                }
            } catch (IOException ex) {
                MessageBox.Show(this, "Cannot make the zip: " + ex.Message, Text);
                return;
            }
            footer.Text = "Send this file to Vulpzin: " + Path.GetFileName(zip);
            Process.Start("explorer.exe", "/select,\"" + zip + "\"");
        }

        void OnClosing(object sender, FormClosingEventArgs e)
        {
            if (dirty && server == null) {
                var answer = MessageBox.Show(this, "Save the changed settings?", Text, MessageBoxButtons.YesNoCancel);
                if (answer == DialogResult.Cancel) {
                    e.Cancel = true;
                    return;
                }
                if (answer == DialogResult.Yes)
                    SaveSettings();
            }
            if (server == null)
                return;
            if (MessageBox.Show(this, "The server runs. Stop it and close?", Text, MessageBoxButtons.YesNo) != DialogResult.Yes) {
                e.Cancel = true;
                return;
            }
            StopServer();
        }

        [DllImport("user32.dll")]
        static extern bool SetProcessDPIAware();

        [STAThread]
        static void Main()
        {
            SetProcessDPIAware();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new LauncherForm(AppDomain.CurrentDomain.BaseDirectory));
        }
    }
}
