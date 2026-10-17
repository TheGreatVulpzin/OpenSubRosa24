// SubVulpes test launcher: a server panel for the testers. It starts the standalone server
// with the captures of a preset, shows the speed and the console, keeps each test in its own
// folder, packs a test for the developers, edits the server keys of config.txt and installs
// an update zip.
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
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SubVulpes
{
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

    // The speed of each minute as a line. The gray line is the normal speed (62 ticks/s).
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
            using (var pen = new Pen(Color.Silver) { DashStyle = DashStyle.Dash })
                g.DrawLine(pen, 32, y(62), 32 + w, y(62));
            using (var brush = new SolidBrush(Color.Gray)) {
                g.DrawString("62", Font, brush, 4, y(62) - 7);
                g.DrawString("0", Font, brush, 4, y(0) - 7);
            }
            if (Points.Count == 0) {
                TextRenderer.DrawText(g, "The speed of each minute shows here.", Font, ClientRectangle, Color.Gray,
                                      TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
                return;
            }
            int n = Math.Max(Points.Count, 10);
            var pts = new PointF[Points.Count];
            for (int i = 0; i < Points.Count; i++)
                pts[i] = new PointF(32 + w * (i + 1) / (float)n, y(Points[i]));
            using (var pen = new Pen(Color.FromArgb(40, 110, 200), 2))
                if (pts.Length > 1)
                    g.DrawLines(pen, pts);
            for (int i = 0; i < pts.Length; i++)
                using (var b = new SolidBrush(Points[i] < 55 ? Color.DarkOrange : Color.FromArgb(40, 110, 200)))
                    g.FillEllipse(b, pts[i].X - 3, pts[i].Y - 3, 6, 6);
        }
    }

    class LauncherForm : Form
    {
        static readonly Regex TicksLine = new Regex(@"\(([0-9.]+) ticks/s\)(?:, (\d+) players, (\d+) bots)?");
        static readonly Color Gray = Color.FromArgb(110, 110, 110);
        static readonly Color Green = Color.FromArgb(30, 140, 60);
        static readonly Color Orange = Color.FromArgb(215, 120, 0);
        static readonly Color Red = Color.FromArgb(200, 40, 40);
        static readonly Color Side = Color.FromArgb(235, 237, 240);
        static readonly Color SideSelected = Color.FromArgb(214, 226, 245);
        const int FreezeSeconds = 150;
        const int HotkeyId = 1;
        static readonly string[] UpdateFiles = {
            "subvulpes-server.exe", "subvulpes-server-record.exe", "subvulpes-launcher.exe", "LEIA-ME.txt", "version.txt",
            "record_focus.txt"
        };

        static readonly Preset[] Presets = {
            new Preset("Normal Playtest",
                "Play as usual. The speed profile shows slow parts. A crash or a freeze writes a small dump.",
                new Captures { Profile = true, FreezeDump = true }),
            new Preset("Harness Recording",
                "Records the calls of the game functions and the packets, for the tests of the C code. Play at least 10 minutes: the memory snapshot comes at 10:00. Do many different things: buy, sell, use the phone, drive, finish a round.",
                new Captures { Packets = true, Record = true }),
            new Preset("Playtest (For Speed Issues)",
                "Use it when the game is slow. It adds the game timers to the speed profile. Press Ctrl+F12 when the game lags.",
                new Captures { Profile = true, Timers = true, FreezeDump = true }),
            new Preset("Crashhunt",
                "Use it when the server crashes or freezes. A crash or a freeze writes a full dump (about 600 MB, less in the zip).",
                new Captures { Packets = true, FreezeDump = true, CrashDump = "full" }),
            new Preset("Custom", "Choose the captures yourself in the Captures page.", null),
        };

        readonly string folder, logsRoot;
        readonly KeyFile config, settings;
        readonly ToolTip tips = new ToolTip();
        Captures captures = Presets[0].Captures.Copy();
        bool settingCaptures;

        // Frame
        Panel content;
        readonly Dictionary<string, Control> pages = new Dictionary<string, Control>();
        readonly Dictionary<string, Button> navButtons = new Dictionary<string, Button>();
        Label statusDot, statusText, footer;
        ComboBox presetBox;
        Button startStop;

        // Overview
        Label lastTicks, lowTicks, playersText, uptimeText, marksText, presetText, capturesText, summaryText;
        SpeedGraph graph;
        Panel runPanel, endPanel;
        Label endTitle, endText;
        TextBox endNotes;

        // Console
        TextBox console;

        // Captures page
        CheckBox profileBox, timersBox, packetsBox, recordBox, freezeBox;
        RadioButton dumpNone, dumpSmall, dumpFull;
        Label presetHelp;

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
            this.folder = folder;
            logsRoot = Path.Combine(folder, "logs");
            config = new KeyFile(Path.Combine(folder, "config.txt"));
            settings = new KeyFile(Path.Combine(folder, "launcher.ini"));
            Text = "SubVulpes test launcher" + Version();
            Font = SystemFonts.MessageBoxFont;
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(1000, 680);
            MinimumSize = new Size(820, 560);
            AllowDrop = true;
            DragEnter += (s, e) => { if (ZipOf(e) != null) e.Effect = DragDropEffects.Copy; };
            DragDrop += (s, e) => { string zip = ZipOf(e); if (zip != null) InstallUpdate(zip); };

            content = new Panel { Dock = DockStyle.Fill, Padding = new Padding(14, 8, 14, 8) };
            AddPage("Overview", OverviewPage());
            AddPage("Console", ConsolePage());
            AddPage("Captures", CapturesPage());
            AddPage("Past tests", TestsPage());
            AddPage("Settings", SettingsPage());
            Controls.Add(content);
            Controls.Add(TopBar());
            Controls.Add(Sidebar());

            LoadSettings();
            string saved = settings.Get("preset", Presets[0].Name);
            presetBox.SelectedItem = Presets.FirstOrDefault(p => p.Name == saved) ?? Presets[0];
            if (presetBox.SelectedItem == Presets[Presets.Length - 1])
                captures = ReadCustomCaptures();
            ShowCaptures();
            SetState("Stopped", Gray);
            ShowPage("Overview");
            clock.Tick += delegate { OnClock(); };
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
            return File.Exists(v) ? ", build " + File.ReadAllText(v).Trim() : "";
        }

        static Label Hint(string text)
        {
            return new Label { Text = text, AutoSize = true, ForeColor = Gray, Margin = new Padding(3, 0, 3, 8) };
        }

        static Label Title(string text, Font baseFont)
        {
            return new Label { Text = text, AutoSize = true, Font = new Font(baseFont.FontFamily, baseFont.Size * 1.35f, FontStyle.Bold), Margin = new Padding(0, 0, 0, 8) };
        }

        // ------------------------------------------------------------ frame

        Control Sidebar()
        {
            var bar = new Panel { Dock = DockStyle.Left, Width = 170, BackColor = Side, Padding = new Padding(8, 12, 8, 8) };
            var list = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false };
            list.Controls.Add(new Label { Text = "SubVulpes", AutoSize = true, Font = new Font(Font.FontFamily, Font.Size * 1.3f, FontStyle.Bold), Margin = new Padding(6, 0, 0, 14) });
            foreach (string name in new[] { "Overview", "Console", "Captures", "Past tests", "Settings" }) {
                var b = new Button {
                    Text = name, Width = 150, Height = 34, FlatStyle = FlatStyle.Flat, TextAlign = ContentAlignment.MiddleLeft,
                    Padding = new Padding(8, 0, 0, 0), Margin = new Padding(0, 0, 0, 2), Cursor = Cursors.Hand
                };
                b.FlatAppearance.BorderSize = 0;
                string page = name;
                b.Click += delegate { ShowPage(page); };
                navButtons[name] = b;
                list.Controls.Add(b);
            }
            var update = new Button { Text = "Update from a zip...", Dock = DockStyle.Bottom, Height = 30, FlatStyle = FlatStyle.Flat };
            update.FlatAppearance.BorderColor = Color.Silver;
            update.Click += delegate { ChooseUpdate(); };
            tips.SetToolTip(update, "Installs the exe files of an update zip. You can also drop the zip on this window.");
            bar.Controls.Add(list);
            bar.Controls.Add(update);
            return bar;
        }

        Control TopBar()
        {
            var bar = new TableLayoutPanel { Dock = DockStyle.Top, Height = 58, ColumnCount = 5, Padding = new Padding(14, 10, 14, 4) };
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var big = new Font(Font.FontFamily, Font.Size * 1.5f, FontStyle.Bold);
            statusDot = new Label { Text = "●", Font = big, AutoSize = true, Anchor = AnchorStyles.Left };
            statusText = new Label { Font = big, AutoSize = true, Anchor = AnchorStyles.Left };
            presetBox = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 230, Anchor = AnchorStyles.Right, Margin = new Padding(3, 6, 8, 3) };
            presetBox.Items.AddRange(Presets);
            presetBox.SelectedIndexChanged += delegate { OnPreset(); };
            tips.SetToolTip(presetBox, "What do you test? The preset chooses the captures.");
            startStop = new Button { AutoSize = true, Padding = new Padding(18, 4, 18, 4), Font = new Font(Font, FontStyle.Bold), Anchor = AnchorStyles.Right };
            startStop.Click += delegate { if (server == null) StartServer(); else StopServer(); };
            bar.Controls.Add(statusDot, 0, 0);
            bar.Controls.Add(statusText, 1, 0);
            bar.Controls.Add(presetBox, 3, 0);
            bar.Controls.Add(startStop, 4, 0);
            footer = new Label { Dock = DockStyle.Bottom, Height = 22, ForeColor = Gray, TextAlign = ContentAlignment.MiddleLeft, Padding = new Padding(14, 0, 0, 0) };
            Controls.Add(footer);
            return bar;
        }

        void AddPage(string name, Control page)
        {
            page.Dock = DockStyle.Fill;
            page.Visible = false;
            pages[name] = page;
            content.Controls.Add(page);
        }

        void ShowPage(string name)
        {
            foreach (var p in pages)
                p.Value.Visible = p.Key == name;
            foreach (var b in navButtons) {
                b.Value.BackColor = b.Key == name ? SideSelected : Side;
                b.Value.Font = new Font(Font, b.Key == name ? FontStyle.Bold : FontStyle.Regular);
            }
            if (name == "Past tests")
                LoadTests();
        }

        void SetState(string text, Color color)
        {
            statusText.Text = text;
            statusDot.ForeColor = color;
            startStop.Text = server == null ? "Start server" : "Stop server";
            presetBox.Enabled = server == null;
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

            var cards = new TableLayoutPanel { Dock = DockStyle.Fill, Height = 72, ColumnCount = 5, Margin = new Padding(0, 0, 0, 8) };
            for (int i = 0; i < 5; i++)
                cards.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 20));
            lastTicks = Card(cards, 0, "Last minute");
            lowTicks = Card(cards, 1, "Lowest minute");
            playersText = Card(cards, 2, "Players");
            uptimeText = Card(cards, 3, "Running for");
            marksText = Card(cards, 4, "Marks");
            page.Controls.Add(cards, 0, 0);

            page.Controls.Add(new Label { Text = "Speed of each minute (ticks/s)", AutoSize = true, ForeColor = Gray, Margin = new Padding(3, 4, 3, 2) }, 0, 1);
            graph = new SpeedGraph { Dock = DockStyle.Fill, MinimumSize = new Size(0, 120) };
            page.Controls.Add(graph, 0, 2);

            // While a test runs: the marks and the dumps. After the stop: the result and the notes.
            runPanel = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, Margin = new Padding(0, 8, 0, 0) };
            var mark = new Button { Text = "Mark this moment (Ctrl+F12)", AutoSize = true };
            var dump = new Button { Text = "Dump now", AutoSize = true };
            mark.Click += delegate { AddMark(); };
            dump.Click += delegate { DumpNow("dump"); };
            tips.SetToolTip(mark, "Writes the time to the log. Use it when something strange happens. Ctrl+F12 also works in the game.");
            tips.SetToolTip(dump, "Writes the memory of the server to a file. Use it when the game lags or freezes.");
            presetText = new Label { AutoSize = true, ForeColor = Gray, Anchor = AnchorStyles.Left, Margin = new Padding(12, 8, 3, 3) };
            runPanel.Controls.AddRange(new Control[] { mark, dump, presetText });
            page.Controls.Add(runPanel, 0, 3);

            endPanel = new TableLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, ColumnCount = 1, Visible = false, Margin = new Padding(0, 8, 0, 0) };
            endTitle = new Label { AutoSize = true, Font = new Font(Font.FontFamily, Font.Size * 1.2f, FontStyle.Bold) };
            endText = new Label { AutoSize = true, ForeColor = Gray, Margin = new Padding(3, 2, 3, 6) };
            endNotes = new TextBox { Multiline = true, Height = 54, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical };
            var endBar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 4, 0, 0) };
            var pack = new Button { Text = "Pack this test", AutoSize = true, Font = new Font(Font, FontStyle.Bold) };
            pack.Click += delegate { SaveNotes(testDir, endNotes.Text); PackTest(testDir); };
            capturesText = new Label { AutoSize = true, ForeColor = Gray, Anchor = AnchorStyles.Left, Margin = new Padding(8, 8, 3, 3) };
            endBar.Controls.AddRange(new Control[] { pack, capturesText });
            endPanel.Controls.Add(endTitle);
            endPanel.Controls.Add(endText);
            endPanel.Controls.Add(new Label { Text = "Notes for Vulpzin: what happened, what was strange?", AutoSize = true });
            endPanel.Controls.Add(endNotes);
            endPanel.Controls.Add(endBar);
            page.Controls.Add(endPanel, 0, 4);

            summaryText = new Label { AutoSize = true, ForeColor = Gray };
            ResetCards();
            return page;
        }

        Label Card(TableLayoutPanel cards, int column, string title)
        {
            var box = new Panel { Dock = DockStyle.Fill, Height = 64, BackColor = SystemColors.Window, Margin = new Padding(column == 0 ? 0 : 4, 0, column == 4 ? 0 : 4, 0), BorderStyle = BorderStyle.FixedSingle };
            box.Controls.Add(new Label { Text = title, ForeColor = Gray, Location = new Point(8, 6), AutoSize = true });
            var value = new Label { Location = new Point(8, 26), AutoSize = true, Font = new Font(Font.FontFamily, Font.Size * 1.6f, FontStyle.Bold) };
            box.Controls.Add(value);
            cards.Controls.Add(box, column, 0);
            return value;
        }

        void ResetCards()
        {
            foreach (var l in new[] { lastTicks, lowTicks, playersText, uptimeText })
                l.Text = "-";
            lastTicks.ForeColor = SystemColors.ControlText;
            marksText.Text = "0";
        }

        // ------------------------------------------------------------ console

        Control ConsolePage()
        {
            console = new TextBox {
                Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Both, WordWrap = false,
                Font = new Font("Consolas", 9.5f), BackColor = Color.FromArgb(24, 24, 24), ForeColor = Color.Gainsboro
            };
            return console;
        }

        // ------------------------------------------------------------ captures

        Control CapturesPage()
        {
            var page = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true };
            page.Controls.Add(Title("Captures", Font));
            presetHelp = new Label { AutoSize = true, MaximumSize = new Size(620, 0), Margin = new Padding(3, 0, 3, 10) };
            page.Controls.Add(presetHelp);

            profileBox = Option("Speed profile", "The code position every millisecond. It shows where the server is slow. About 30 MB for each hour.");
            timersBox = Option("Game timers", "The time of each part of the game, in the log each minute.");
            packetsBox = Option("Packet capture", "Every network packet. It lets Vulpzin replay the session. It has the player names and the chat.");
            recordBox = Option("Harness recording", "The calls of the game functions, for the tests of the C code (subvulpes-server-record.exe, about 10% slower). The memory snapshot comes after 10 minutes.");
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
            box.Tag = hint;
            return box;
        }

        GroupBox Group(string title, params Control[] rows)
        {
            var box = new GroupBox { Text = title, AutoSize = true, MinimumSize = new Size(560, 0), Padding = new Padding(8, 6, 8, 4) };
            var list = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false };
            foreach (Control r in rows) {
                list.Controls.Add(r);
                if (r.Tag is string)
                    list.Controls.Add(new Label { Text = (string)r.Tag, AutoSize = true, ForeColor = Gray, MaximumSize = new Size(520, 0), Margin = new Padding(20, 0, 3, 6) });
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
            presetHelp.Text = preset.Name + ": " + preset.Text;
            presetText.Text = preset.Name + ": " + captures.Summary() + ".";
        }

        void OnPreset()
        {
            var preset = (Preset)presetBox.SelectedItem;
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
            presetHelp.Text = preset.Name + ": " + preset.Text;
            presetText.Text = preset.Name + ": " + captures.Summary() + ".";
        }

        Captures ReadCustomCaptures()
        {
            return new Captures {
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
            // The harness recording runs the recording build of the server (record_shim.c).
            string exe = Path.Combine(folder, captures.Record ? "subvulpes-server-record.exe" : "subvulpes-server.exe");
            if (!File.Exists(exe)) {
                MessageBox.Show(this, Path.GetFileName(exe) + " is not in this folder.", Text);
                return;
            }
            if (dirty)
                SaveSettings();
            SaveCaptureChoice();

            string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss");
            testDir = Path.Combine(logsRoot, stamp);
            Directory.CreateDirectory(testDir);
            string rel = Path.Combine("logs", stamp);
            var start = new ProcessStartInfo(exe) {
                WorkingDirectory = folder, UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true
            };
            var env = start.EnvironmentVariables;
            foreach (string k in new[] { "SV_SAMPLE", "SV_TIMERS", "SV_PACKETS", "SV_DUMP", "SV_DUMP_TYPE", "SV_TEST_CRASH" })
                env.Remove(k);
            env["SV_LOG"] = Path.Combine(rel, "server.log");
            if (captures.Profile) env["SV_SAMPLE"] = Path.Combine(rel, "profile.txt");
            if (captures.Timers) env["SV_TIMERS"] = "1";
            if (captures.Packets) env["SV_PACKETS"] = Path.Combine(rel, "packets.bin");
            env.Remove("SV_RECORD_DIR");
            env.Remove("SV_SNAPSHOT_AFTER");
            if (captures.Record) {
                env["SV_RECORD_DIR"] = rel;
                env["SV_SNAPSHOT_AFTER"] = settings.Get("snapshot_after", "600");
            }
            if (captures.CrashDump != "none") {
                env["SV_DUMP"] = Path.Combine(rel, "crash.dmp");
                env["SV_DUMP_TYPE"] = captures.CrashDump;
            }
            // A developer test of the crash path: launcher.ini test_crash=<seconds>.
            string testCrash = settings.Get("test_crash", "");
            if (testCrash != "")
                env["SV_TEST_CRASH"] = testCrash;

            var session = new KeyFile(Path.Combine(testDir, "session.ini"));
            session.Set("preset", ((Preset)presetBox.SelectedItem).Name);
            session.Set("captures", captures.Summary());
            session.Set("start", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            session.Set("server", ServerVersion());
            session.Save();

            server = new Process { StartInfo = start, EnableRaisingEvents = true };
            server.OutputDataReceived += OnOutput;
            server.ErrorDataReceived += OnOutput;
            server.Exited += delegate { BeginInvoke(new Action(OnExited)); };
            stopping = crashed = frozen = false;
            marks = maxPlayers = 0;
            speeds.Clear();
            graph.Points.Clear();
            graph.Invalidate();
            console.Clear();
            ResetCards();
            server.Start();
            server.BeginOutputReadLine();
            server.BeginErrorReadLine();
            started = lastSpeedLine = DateTime.Now;
            clock.Start();
            runPanel.Visible = true;
            endPanel.Visible = false;
            SetState("Starting", Orange);
            footer.Text = "This test: logs\\" + stamp +
                          (captures.Record ? ". Play at least " + int.Parse(settings.Get("snapshot_after", "600")) / 60 + " min: the memory snapshot comes then." : "");
        }

        string ServerVersion()
        {
            var info = new FileInfo(Path.Combine(folder, "subvulpes-server.exe"));
            return info.Exists ? info.LastWriteTime.ToString("yyyy-MM-dd HH:mm") + ", " + info.Length + " bytes" : "?";
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
                speeds.Add(ticks);
                graph.Points.Add(ticks);
                graph.Invalidate();
                lastSpeedLine = DateTime.Now;
                frozen = false;
                lastTicks.Text = m.Groups[1].Value;
                lastTicks.ForeColor = ticks < 55 ? Orange : Green;
                lowTicks.Text = speeds.Min().ToString("0.0", CultureInfo.InvariantCulture);
                if (m.Groups[2].Success) {
                    int players = int.Parse(m.Groups[2].Value);
                    maxPlayers = Math.Max(maxPlayers, players);
                    playersText.Text = players + (m.Groups[3].Value != "0" ? " (+" + m.Groups[3].Value + " bots)" : "");
                }
                if (statusText.Text == "Running" || statusText.Text == "Frozen?")
                    SetState("Running", Green);
            } else if (line.Contains("own main loop: running")) {
                lastSpeedLine = DateTime.Now;
                SetState("Running", Green);
            } else if (line.Contains("snapshot saved")) {
                footer.Text = "The memory snapshot is saved. The harness recording is complete: you can stop at any time.";
            } else if (line.Contains("CRASH")) {
                crashed = true;
            }
        }

        void OnClock()
        {
            TimeSpan t = DateTime.Now - started;
            uptimeText.Text = ((int)t.TotalHours).ToString("00") + ":" + t.Minutes.ToString("00") + ":" + t.Seconds.ToString("00");
            if (server == null || frozen || (DateTime.Now - lastSpeedLine).TotalSeconds < FreezeSeconds)
                return;
            // The status line comes each minute. No line for a long time: the main loop stops.
            frozen = true;
            SetState("Frozen?", Red);
            AddLine("[launcher] No speed line for " + FreezeSeconds + " s. The server can be frozen.");
            if (captures.FreezeDump)
                DumpNow("freeze");
        }

        void OnExited()
        {
            int code = server.ExitCode;
            server.Dispose();
            server = null;
            clock.Stop();
            string result;
            if (stopping) {
                AddLine("[launcher] You stopped the server.");
                SetState("Stopped", Gray);
                result = frozen ? "frozen" : "ok";
            } else {
                AddLine("[launcher] The server stopped by itself, exit code " + code + ".");
                SetState(crashed ? "Crashed" : "Stopped by itself", Red);
                result = crashed ? "crash" : "stopped by itself";
            }
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
            endTitle.ForeColor = result == "ok" ? SystemColors.ControlText : Red;
            endText.Text = (speeds.Count > 0
                                ? "Speed " + speeds.Min().ToString("0", CultureInfo.InvariantCulture) + " to " +
                                      speeds.Max().ToString("0", CultureInfo.InvariantCulture) + " ticks/s (normal 62). "
                                : "No speed line (the test was shorter than 1 minute). ") +
                           marks + (marks == 1 ? " mark" : " marks") + ". Most players: " + maxPlayers + ".";
            endNotes.Text = "";
            capturesText.Text = Files(testDir).Count + " files, " + FolderSize(testDir);
            ShowPage("Overview");
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
            if (server == null)
                return;
            marks++;
            marksText.Text = marks.ToString();
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
            bool full = captures.CrashDump == "full";
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
            page.Controls.Add(Title("Past tests", Font), 0, 0);
            testsList = new ListView { Dock = DockStyle.Fill, View = View.Details, FullRowSelect = true, MultiSelect = false, HideSelection = false };
            foreach (var c in new[] { "Start", "Preset", "Minutes", "Speed (min / avg)", "Players", "Result", "Size" })
                testsList.Columns.Add(c, c == "Preset" ? 190 : c == "Start" ? 140 : c.StartsWith("Speed") ? 130 : 90);
            testsList.SelectedIndexChanged += delegate { ShowTestNotes(); };
            page.Controls.Add(testsList, 0, 1);
            page.Controls.Add(new Label { Text = "Notes of the selected test", AutoSize = true, Margin = new Padding(3, 8, 3, 2) }, 0, 2);
            testNotes = new TextBox { Multiline = true, Height = 60, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical };
            page.Controls.Add(testNotes, 0, 3);
            var bar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 6, 0, 0) };
            var pack = new Button { Text = "Pack the selected test", AutoSize = true, Font = new Font(Font, FontStyle.Bold) };
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
            footer.Text = "Packing " + Path.GetFileName(zip) + "...";
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
                        footer.Text = "";
                        MessageBox.Show(this, "Cannot make the zip: " + error, Text);
                        return;
                    }
                    footer.Text = "Send this file to Vulpzin: logs\\" + Path.GetFileName(zip) + " (" +
                                  (new FileInfo(zip).Length / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture) + " MB)";
                    Process.Start("explorer.exe", "/select,\"" + zip + "\"");
                }));
            });
        }

        // ------------------------------------------------------------ update

        static string ZipOf(DragEventArgs e)
        {
            var files = e.Data.GetData(DataFormats.FileDrop) as string[];
            return files != null && files.Length == 1 && files[0].EndsWith(".zip", StringComparison.OrdinalIgnoreCase) ? files[0] : null;
        }

        void ChooseUpdate()
        {
            using (var dialog = new OpenFileDialog { Filter = "Update zip (*.zip)|*.zip", Title = "Choose the update zip" })
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    InstallUpdate(dialog.FileName);
        }

        // Only the files of UpdateFiles come from the zip, from any folder inside it. The old
        // files go to backup\<time>. config.txt, data and user never change. The running
        // launcher cannot be overwritten, but Windows lets it be renamed: it becomes
        // subvulpes-launcher.old, and the next start deletes it.
        public void InstallUpdate(string zip)
        {
            var found = new Dictionary<string, ZipArchiveEntry>(StringComparer.OrdinalIgnoreCase);
            ZipArchive archive;
            try {
                archive = ZipFile.OpenRead(zip);
            } catch (Exception ex) {
                MessageBox.Show(this, "Cannot read the zip: " + ex.Message, Text);
                return;
            }
            using (archive) {
                foreach (var entry in archive.Entries)
                    if (UpdateFiles.Contains(entry.Name, StringComparer.OrdinalIgnoreCase))
                        found[entry.Name] = entry;
                if (found.Count == 0) {
                    MessageBox.Show(this, "The zip has no update file (" + string.Join(", ", UpdateFiles) + ").", Text);
                    return;
                }
                string version = "";
                if (found.ContainsKey("version.txt"))
                    using (var r = new StreamReader(found["version.txt"].Open()))
                        version = " (build " + r.ReadToEnd().Trim() + ")";
                string question = "Install this update" + version + "?\r\n\r\n" + string.Join("\r\n", found.Keys.ToArray()) +
                                  "\r\n\r\nThe old files go to the backup folder.";
                if (server != null)
                    question += "\r\n\r\nThe server runs. The update stops it.";
                if (MessageBox.Show(this, question, Text, MessageBoxButtons.YesNo) != DialogResult.Yes)
                    return;
                if (server != null) {
                    StopServer();
                    server.WaitForExit(5000);
                }

                string backup = Path.Combine(folder, "backup", DateTime.Now.ToString("yyyyMMdd-HHmmss"));
                Directory.CreateDirectory(backup);
                bool newLauncher = false;
                try {
                    foreach (var f in found) {
                        string target = Path.Combine(folder, f.Key);
                        if (File.Exists(target))
                            File.Copy(target, Path.Combine(backup, f.Key), true);
                        if (f.Key.Equals("subvulpes-launcher.exe", StringComparison.OrdinalIgnoreCase)) {
                            string old = Path.Combine(folder, "subvulpes-launcher.old");
                            if (File.Exists(old))
                                File.Delete(old);
                            File.Move(Application.ExecutablePath, old);
                            newLauncher = true;
                        }
                        f.Value.ExtractToFile(target, true);
                    }
                } catch (Exception ex) {
                    MessageBox.Show(this, "The update stopped: " + ex.Message + "\r\n\r\nThe old files are in " + backup + ".", Text);
                    return;
                }
                if (newLauncher) {
                    MessageBox.Show(this, "Update installed" + version + ". The launcher starts again now.", Text);
                    Process.Start(Path.Combine(folder, "subvulpes-launcher.exe"));
                    Close();
                    return;
                }
                Text = "SubVulpes test launcher" + Version();
                footer.Text = "Update installed" + version + ". Old files: backup\\" + Path.GetFileName(backup);
            }
        }

        void CleanOldLauncher()
        {
            string old = Path.Combine(folder, "subvulpes-launcher.old");
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
            page.Controls.Add(Title("Settings", Font));

            nameBox = new TextBox { Width = 300 };
            passwordBox = new TextBox { Width = 220, UseSystemPasswordChar = true };
            var show = new CheckBox { Text = "Show", AutoSize = true, Anchor = AnchorStyles.Left };
            show.CheckedChanged += delegate { passwordBox.UseSystemPasswordChar = !show.Checked; };
            var password = new FlowLayoutPanel { AutoSize = true, Margin = Padding.Empty };
            password.Controls.Add(passwordBox);
            password.Controls.Add(show);
            welcomeBox = new TextBox { Width = 300, Height = 54, Multiline = true, ScrollBars = ScrollBars.Vertical };
            page.Controls.Add(SettingsGroup("Server", new[] {
                Row("Name", nameBox, "The name in the server list."),
                Row("Password", password, "Empty: everybody can join."),
                Row("Welcome text", welcomeBox, "The players see it when they join (readme.txt)."),
            }));

            modeBox = new ComboBox { Width = 160, DropDownStyle = ComboBoxStyle.DropDownList };
            playersBox = new NumericUpDown { Width = 80, Minimum = 1, Maximum = 255 };
            roundBox = new NumericUpDown { Width = 80, Minimum = 1, Maximum = 999 };
            modeBox.SelectedIndexChanged += delegate { var m = modeBox.SelectedItem as Mode; roundBox.Enabled = m != null && m.Value == "3"; };
            page.Controls.Add(SettingsGroup("Game", new[] {
                Row("Mode", modeBox, null),
                Row("Max players", playersBox, null),
                Row("Round time", roundBox, "Only for the Round mode."),
            }));

            portBox = new NumericUpDown { Width = 100, Minimum = 1, Maximum = 65535 };
            bytesBox = new NumericUpDown { Width = 120, Minimum = 0, Maximum = 100000000, Increment = 100000, ThousandsSeparator = true };
            page.Controls.Add(SettingsGroup("Network", new[] {
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

        GroupBox SettingsGroup(string title, Control[] rows)
        {
            var box = new GroupBox { Text = title, AutoSize = true, MinimumSize = new Size(560, 0), Padding = new Padding(8, 6, 8, 4) };
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
            navButtons["Settings"].Text = value ? "Settings *" : "Settings";
            dirtyText.Text = value ? "Unsaved changes. Start server also saves them." : "";
            dirtyText.ForeColor = Orange;
        }

        void UpdateSummary()
        {
            var mode = modeBox.SelectedItem as Mode;
            footer.Text = config.Exists
                ? nameBox.Text + "  |  " + (mode != null ? mode.Name : "?") + "  |  port " + portBox.Value + "  |  " + playersBox.Value + " players max"
                : "config.txt is not in this folder.";
        }

        // ------------------------------------------------------------ close

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
            if (testDir != null && endPanel.Visible)
                SaveNotes(testDir, endNotes.Text);
            if (server == null)
                return;
            if (MessageBox.Show(this, "The server runs. Stop it and close?", Text, MessageBoxButtons.YesNo) != DialogResult.Yes) {
                e.Cancel = true;
                return;
            }
            StopServer();
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
                form.Shown += delegate { form.InstallUpdate(args[0]); };
            Application.Run(form);
        }
    }
}
