// SubVulpes test launcher: starts the standalone server for a test, shows its console,
// edits config.txt and packs the logs for the developers.
//
// Build: tools\launcher\build.bat. It uses the C# compiler of .NET Framework 4, which is
// part of Windows, so the testers do not install anything. C# 5 only (no $"" strings).
// Put the exe in the server folder, next to subvulpes-server.exe.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
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
            if (File.Exists(path))
                lines.AddRange(File.ReadAllText(path, Encoding.Default).Replace("\r\n", "\n").Split('\n'));
        }

        public List<string> Keys()
        {
            var keys = new List<string>();
            foreach (string line in lines) {
                int eq = line.IndexOf('=');
                if (eq > 0)
                    keys.Add(line.Substring(0, eq));
            }
            return keys;
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

    class LauncherForm : Form
    {
        static readonly Dictionary<string, string> Hints = new Dictionary<string, string> {
            { "servername", "The name in the server list." },
            { "serverpassword", "Empty: no password." },
            { "gametype", "3 = Round, 4 = World." },
            { "port", "UDP port. Open it in the router." },
            { "maxnumofplayers", "The maximum number of players." },
        };
        static readonly Regex TicksLine = new Regex(@"\(([0-9.]+) ticks/s\)");

        readonly string folder;
        readonly KeyFile config;
        readonly KeyFile settings;
        readonly Dictionary<string, TextBox> fields = new Dictionary<string, TextBox>();
        readonly ToolTip tips = new ToolTip();
        TextBox welcome;
        Button saveButton, startButton, stopButton;
        CheckBox profileBox, timersBox;
        TextBox console;
        Label ticksLabel, statusLabel;
        Process server;
        bool stopping;

        public LauncherForm(string folder)
        {
            this.folder = folder;
            config = new KeyFile(Path.Combine(folder, "config.txt"));
            settings = new KeyFile(Path.Combine(folder, "launcher.ini"));
            Text = "SubVulpes test launcher";
            Font = SystemFonts.MessageBoxFont;
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(980, 700);
            MinimumSize = new Size(720, 420);

            var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Padding = new Padding(6) };
            root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 380));
            root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            root.Controls.Add(LeftPanel(), 0, 0);
            root.Controls.Add(RightPanel(), 1, 0);
            Controls.Add(root);
            SetRunning(false);
            FormClosing += OnClosing;
            Shown += delegate { startButton.Focus(); };
        }

        // ------------------------------------------------------------ left: config and captures

        Control LeftPanel()
        {
            var panel = new FlowLayoutPanel {
                Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true
            };

            var configBox = new GroupBox { Text = "Server config (config.txt)", AutoSize = true, Width = 350 };
            var grid = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 2 };
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            foreach (string key in config.Keys())
                AddField(grid, key);
            if (fields.Count == 0)
                grid.Controls.Add(new Label { Text = "config.txt not found in this folder.", AutoSize = true });
            configBox.Controls.Add(grid);
            panel.Controls.Add(configBox);

            panel.Controls.Add(new Label { Text = "Welcome text (readme.txt)", AutoSize = true, Margin = new Padding(3, 8, 3, 0) });
            welcome = new TextBox { Multiline = true, ScrollBars = ScrollBars.Vertical, Width = 350, Height = 60 };
            string readme = Path.Combine(folder, "readme.txt");
            if (File.Exists(readme))
                welcome.Text = File.ReadAllText(readme, Encoding.Default).TrimEnd();
            welcome.TextChanged += delegate { saveButton.Enabled = true; };
            panel.Controls.Add(welcome);

            saveButton = new Button { Text = "Save config", AutoSize = true, Enabled = false };
            saveButton.Click += delegate { SaveConfig(); };
            panel.Controls.Add(saveButton);

            var captures = new GroupBox { Text = "Captures", AutoSize = true, Width = 350 };
            var list = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, AutoSize = true };
            profileBox = new CheckBox { Text = "Speed profile (about 30 MB for each hour)", AutoSize = true };
            profileBox.Checked = settings.Get("profile", "1") == "1";
            timersBox = new CheckBox { Text = "Game timers in the log (each minute)", AutoSize = true };
            timersBox.Checked = settings.Get("timers", "0") == "1";
            tips.SetToolTip(profileBox, "Records the code position every millisecond. It shows where the server is slow.");
            tips.SetToolTip(timersBox, "Writes the time of each part of the game to the log.");
            list.Controls.Add(profileBox);
            list.Controls.Add(timersBox);
            captures.Controls.Add(list);
            panel.Controls.Add(captures);
            return panel;
        }

        void AddField(TableLayoutPanel grid, string key)
        {
            var label = new Label { Text = key, AutoSize = true, Anchor = AnchorStyles.Left };
            var box = new TextBox { Text = config.Get(key, ""), Dock = DockStyle.Fill };
            box.TextChanged += delegate { saveButton.Enabled = true; };
            fields[key] = box;
            string hint;
            if (Hints.TryGetValue(key, out hint)) {
                tips.SetToolTip(label, hint);
                tips.SetToolTip(box, hint);
            }
            int row = grid.RowCount++;
            grid.Controls.Add(label, 0, row);
            if (key.IndexOf("password", StringComparison.OrdinalIgnoreCase) < 0) {
                grid.Controls.Add(box, 1, row);
                return;
            }
            // A password stays hidden until the user clicks Show.
            box.UseSystemPasswordChar = true;
            var show = new CheckBox { Text = "Show", AutoSize = true, Anchor = AnchorStyles.Left };
            show.CheckedChanged += delegate { box.UseSystemPasswordChar = !show.Checked; };
            var pair = new TableLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, ColumnCount = 2, Margin = Padding.Empty };
            pair.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            pair.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            pair.Controls.Add(box, 0, 0);
            pair.Controls.Add(show, 1, 0);
            grid.Controls.Add(pair, 1, row);
        }

        void SaveConfig()
        {
            if (fields.Count > 0) {
                foreach (var f in fields)
                    config.Set(f.Key, f.Value.Text);
                config.Save();
            }
            File.WriteAllText(Path.Combine(folder, "readme.txt"), welcome.Text, Encoding.Default);
            saveButton.Enabled = false;
            Status("Config saved. The server reads it when it starts.");
        }

        // ------------------------------------------------------------ right: server and console

        Control RightPanel()
        {
            var panel = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 4 };
            panel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            panel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            panel.RowStyles.Add(new RowStyle(SizeType.AutoSize));

            var buttons = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill };
            startButton = new Button { Text = "Start server", AutoSize = true };
            stopButton = new Button { Text = "Stop server", AutoSize = true };
            var openLogs = new Button { Text = "Open logs folder", AutoSize = true };
            var pack = new Button { Text = "Pack logs to send", AutoSize = true };
            startButton.Click += delegate { StartServer(); };
            stopButton.Click += delegate { StopServer(); };
            openLogs.Click += delegate { OpenLogs(); };
            pack.Click += delegate { PackLogs(); };
            buttons.Controls.AddRange(new Control[] { startButton, stopButton, openLogs, pack });
            panel.Controls.Add(buttons, 0, 0);

            ticksLabel = new Label { AutoSize = true, Font = new Font(Font.FontFamily, Font.Size * 1.3f, FontStyle.Bold), Margin = new Padding(3, 6, 3, 6) };
            panel.Controls.Add(ticksLabel, 0, 1);

            console = new TextBox {
                Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Both, WordWrap = false, Dock = DockStyle.Fill,
                Font = new Font("Consolas", 9.5f), BackColor = Color.Black, ForeColor = Color.Gainsboro
            };
            panel.Controls.Add(console, 0, 2);

            statusLabel = new Label { AutoSize = true, Margin = new Padding(3, 6, 3, 3) };
            panel.Controls.Add(statusLabel, 0, 3);
            return panel;
        }

        void SetRunning(bool running)
        {
            startButton.Enabled = !running;
            stopButton.Enabled = running;
            profileBox.Enabled = !running;
            timersBox.Enabled = !running;
            if (!running)
                ShowTicks("Server stopped.", SystemColors.ControlText);
        }

        void ShowTicks(string text, Color color)
        {
            ticksLabel.Text = text;
            ticksLabel.ForeColor = color;
        }

        void Status(string text)
        {
            statusLabel.Text = text;
        }

        void StartServer()
        {
            string exe = Path.Combine(folder, "subvulpes-server.exe");
            if (!File.Exists(exe)) {
                MessageBox.Show(this, "subvulpes-server.exe is not in this folder.", Text);
                return;
            }
            if (saveButton.Enabled)
                SaveConfig();
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
            stopping = false;
            console.Clear();
            server.Start();
            server.BeginOutputReadLine();
            server.BeginErrorReadLine();
            SetRunning(true);
            ShowTicks("Server starts. The first speed value comes after 1 minute.", SystemColors.ControlText);
            Status("Log: logs\\standalone-" + stamp + ".log");
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
                double ticks = double.Parse(m.Groups[1].Value, System.Globalization.CultureInfo.InvariantCulture);
                ShowTicks("Last minute: " + m.Groups[1].Value + " ticks/s (normal: about 62)",
                          ticks < 55 ? Color.DarkOrange : Color.ForestGreen);
            } else if (line.Contains("own main loop: running")) {
                ShowTicks("Server runs. It shows in the game list after about 40 s.", Color.ForestGreen);
            } else if (line.Contains("CRASH")) {
                ShowTicks("The server crashed. Click Pack logs to send.", Color.Red);
            }
        }

        void OnExited()
        {
            int code = server.ExitCode;
            server.Dispose();
            server = null;
            AddLine(stopping ? "[launcher] You stopped the server." : "[launcher] The server stopped, exit code " + code + ".");
            bool crashed = ticksLabel.ForeColor == Color.Red;
            SetRunning(false);
            if (crashed || !stopping)
                ShowTicks("The server stopped by itself. Click Pack logs to send.", Color.Red);
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
            Status("Send this file to Vulpzin: " + Path.GetFileName(zip));
            Process.Start("explorer.exe", "/select,\"" + zip + "\"");
        }

        void OnClosing(object sender, FormClosingEventArgs e)
        {
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
