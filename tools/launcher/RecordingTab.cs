// The Recording tab of the Debug page: a checklist of the functions that a recording must still reach,
// and the start of the game client with the hook recorder, next to the server.
//
// The list comes from recording_functions.csv (made by instrumentation/recording/coverage.py --export).
// The launcher counts the recorded calls of each function in the sv_args.txt files of the past tests
// (server: logs\<test>\sv_args.txt, client: logs\<test>\client\sv_args.txt) and of the running test.
// C# 5 only.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SubVulpes
{
    partial class LauncherForm
    {
        class RecFunction
        {
            public string Side, Name, Group, Text, Hint;
            public uint Rva;
        }

        const int RecordMinCalls = 2;
        const int FocusMax = 150;

        List<RecFunction> recFunctions;
        ComboBox recSide, recScenario;
        CheckBox recMissingOnly, recWithServer;
        ListView recList;
        Label recSummary;
        TextBox recClientBox;
        Button recStartClient;
        bool recBusy, recTabActive;
        DateTime recLastRefresh = DateTime.MinValue;
        Process client;
        string clientDir, clientSaveDir;
        readonly Dictionary<string, Dictionary<uint, int>> recCache = new Dictionary<string, Dictionary<uint, int>>();

        Control RecordingPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 3 };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            var clientBar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 0, 0, 6) };
            clientBar.Controls.Add(new Label { Text = "Client folder", AutoSize = true, Margin = new Padding(3, 7, 3, 3) });
            recClientBox = new TextBox { Width = 330, Text = settings.Get("client_folder", "") };
            var browse = new Button { Text = "Browse", AutoSize = true };
            browse.Click += delegate {
                using (var dialog = new FolderBrowserDialog { Description = "Folder of the client, with subrosa_fixed.exe" }) {
                    if (dialog.ShowDialog(this) == DialogResult.OK) {
                        recClientBox.Text = dialog.SelectedPath;
                        settings.Set("client_folder", dialog.SelectedPath);
                        settings.Save();
                    }
                }
            };
            recWithServer = new CheckBox { Text = "Start the client with the server", AutoSize = true, Checked = settings.Get("client_with_server", "0") == "1" };
            recWithServer.CheckedChanged += delegate { settings.Set("client_with_server", recWithServer.Checked ? "1" : "0"); settings.Save(); };
            recStartClient = new Button { Text = "Start the client now", AutoSize = true };
            recStartClient.Click += delegate { StartClient(); };
            clientBar.Controls.AddRange(new Control[] { recClientBox, browse, recWithServer, recStartClient });
            page.Controls.Add(clientBar, 0, 0);

            var bar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 0, 0, 6) };
            recSide = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 100 };
            recSide.Items.AddRange(new object[] { "server", "client" });
            recSide.SelectedIndex = 0;
            recSide.SelectedIndexChanged += delegate { RefreshChecklist(); };
            recMissingOnly = new CheckBox { Text = "Missing only", AutoSize = true, Checked = true, Margin = new Padding(8, 5, 3, 3) };
            recMissingOnly.CheckedChanged += delegate { RefreshChecklist(); };
            var refresh = new Button { Text = "Refresh", AutoSize = true };
            refresh.Click += delegate { RefreshChecklist(); };
            recSummary = new Label { AutoSize = true, Margin = new Padding(12, 7, 3, 3) };
            recScenario = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 140 };
            recScenario.Items.Add(NoScenario);
            foreach (string name in ReadScenarios().Keys) recScenario.Items.Add(name);
            int chosen = recScenario.Items.IndexOf(settings.Get("scenario", NoScenario));
            recScenario.SelectedIndex = chosen < 0 ? 0 : chosen;
            recScenario.SelectedIndexChanged += delegate { settings.Set("scenario", recScenario.Text); settings.Save(); };
            var snapshotNow = new Button { Text = "Snapshot now", AutoSize = true };
            snapshotNow.Click += delegate { RequestSnapshot(); };
            bar.Controls.AddRange(new Control[] { recSide, recMissingOnly, refresh,
                new Label { Text = "Server scenario", AutoSize = true, Margin = new Padding(12, 7, 3, 3) }, recScenario, snapshotNow, recSummary });
            page.Controls.Add(bar, 0, 1);

            recList = new ListView { Dock = DockStyle.Fill, View = View.Details, CheckBoxes = true, FullRowSelect = true, ShowGroups = true };
            recList.Columns.Add("Function", 220);
            recList.Columns.Add("Calls", 60);
            recList.Columns.Add("What to do", 560);
            // The ticks show what the counts say. A click does not change them.
            recList.ItemCheck += delegate(object s, ItemCheckEventArgs e) { if (!recBusy) e.NewValue = e.CurrentValue; };
            page.Controls.Add(recList, 0, 2);
            return page;
        }

        string RecordingFile()
        {
            foreach (string path in new[] {
                Path.Combine(launcherFolder, "recording_functions.csv"),
                Path.GetFullPath(Path.Combine(launcherFolder, @"..\..\instrumentation\recording\functions.csv")) }) {
                if (File.Exists(path)) return path;
            }
            return null;
        }

        static List<string> ParseCsvLine(string line)
        {
            var fields = new List<string>();
            var field = new StringBuilder();
            bool quoted = false;
            for (int i = 0; i < line.Length; i++) {
                char c = line[i];
                if (quoted) {
                    if (c == '"' && i + 1 < line.Length && line[i + 1] == '"') { field.Append('"'); i++; }
                    else if (c == '"') quoted = false;
                    else field.Append(c);
                } else if (c == '"') quoted = true;
                else if (c == ',') { fields.Add(field.ToString()); field.Clear(); }
                else field.Append(c);
            }
            fields.Add(field.ToString());
            return fields;
        }

        bool LoadRecFunctions()
        {
            if (recFunctions != null) return true;
            string path = RecordingFile();
            if (path == null) return false;
            var list = new List<RecFunction>();
            foreach (string line in File.ReadAllLines(path, Encoding.UTF8).Skip(1)) {
                var f = ParseCsvLine(line);
                if (f.Count < 6) continue;
                uint rva;
                if (!uint.TryParse(f[1], System.Globalization.NumberStyles.HexNumber, null, out rva)) continue;
                list.Add(new RecFunction { Side = f[0], Rva = rva, Name = f[2], Group = f[3], Text = f[4], Hint = f[5] });
            }
            recFunctions = list;
            return true;
        }

        // A call record of sv_args.txt starts with "R <rva in hex>".
        static Dictionary<uint, int> CountCalls(string path)
        {
            var counts = new Dictionary<uint, int>();
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (var reader = new StreamReader(stream, Encoding.ASCII, false, 1 << 16)) {
                string line;
                while ((line = reader.ReadLine()) != null) {
                    if (line.Length < 3 || line[0] != 'R' || line[1] != ' ') continue;
                    int end = line.IndexOf(' ', 2);
                    string hex = end < 0 ? line.Substring(2) : line.Substring(2, end - 2);
                    uint rva;
                    if (!uint.TryParse(hex, System.Globalization.NumberStyles.HexNumber, null, out rva)) continue;
                    int n;
                    counts.TryGetValue(rva, out n);
                    counts[rva] = n + 1;
                }
            }
            return counts;
        }

        List<string> RecordingFiles(string side)
        {
            var files = new List<string>();
            if (Directory.Exists(logsRoot)) {
                foreach (string dir in Directory.GetDirectories(logsRoot)) {
                    // Only recordings of the original game count. Our own server would test itself.
                    if (new KeyFile(Path.Combine(dir, "session.ini")).Get("origin", "") == "own server") continue;
                    string path = side == "server" ? Path.Combine(dir, "sv_args.txt") : Path.Combine(dir, "client", "sv_args.txt");
                    if (File.Exists(path)) files.Add(path);
                }
            }
            if (side == "client" && client != null && clientDir != null) {
                string live = Path.Combine(clientDir, "sv_args.txt");
                if (File.Exists(live)) files.Add(live);
            }
            return files;
        }

        // The calls of each function over all the files. A finished file is read once (the cache key is its size and time).
        Dictionary<uint, int> TotalCalls(string side)
        {
            var total = new Dictionary<uint, int>();
            foreach (string path in RecordingFiles(side)) {
                var info = new FileInfo(path);
                string key = path + "|" + info.Length + "|" + info.LastWriteTimeUtc.Ticks;
                Dictionary<uint, int> counts;
                lock (recCache) {
                    if (!recCache.TryGetValue(key, out counts)) counts = null;
                }
                if (counts == null) {
                    try { counts = CountCalls(path); } catch (IOException) { continue; }
                    lock (recCache) { recCache[key] = counts; }
                }
                foreach (var pair in counts) {
                    int n;
                    total.TryGetValue(pair.Key, out n);
                    total[pair.Key] = n + pair.Value;
                }
            }
            return total;
        }

        void RefreshChecklist()
        {
            if (recBusy || recSide == null) return;
            if (!LoadRecFunctions()) {
                recSummary.Text = "recording_functions.csv is not next to the launcher.";
                return;
            }
            recBusy = true;
            recLastRefresh = DateTime.Now;
            string side = (string)recSide.SelectedItem;
            bool missingOnly = recMissingOnly.Checked;
            recSummary.Text = "Counting the recordings...";
            Task.Run(() => {
                Dictionary<uint, int> total;
                try { total = TotalCalls(side); } catch (Exception) { total = new Dictionary<uint, int>(); }
                BeginInvoke(new Action(() => { ShowChecklist(side, missingOnly, total); recBusy = false; }));
            });
        }

        void ShowChecklist(string side, bool missingOnly, Dictionary<uint, int> total)
        {
            var functions = recFunctions.Where(f => f.Side == side).ToList();
            int done = 0;
            recList.BeginUpdate();
            recList.Items.Clear();
            recList.Groups.Clear();
            var groups = new Dictionary<string, ListViewGroup>();
            foreach (var f in functions) {
                int calls;
                total.TryGetValue(f.Rva, out calls);
                bool complete = calls >= RecordMinCalls;
                if (complete) done++;
                if (missingOnly && complete) continue;
                ListViewGroup group;
                string title = f.Group + (f.Hint.Length > 0 ? ": " + f.Hint : "");
                if (!groups.TryGetValue(f.Group, out group)) {
                    group = new ListViewGroup(title);
                    groups[f.Group] = group;
                    recList.Groups.Add(group);
                }
                recList.Items.Add(new ListViewItem(new[] { f.Name, calls.ToString(), f.Text }, group) { Checked = complete });
            }
            recList.EndUpdate();
            recSummary.Text = done + " of " + functions.Count + " functions have " + RecordMinCalls + " or more calls.";
        }

        // ------------------------------------------------------------ record_focus.txt

        // The server recorder hooks the functions of record_focus.txt all the time. They are the ones that are still missing.
        // The class "diff" keeps the hook until record_diff_max calls (32) and then drops it. The class "untested"
        // records every call with new arguments without a limit: a hot function would fill the file in seconds.
        const string FocusMarker = "# Made by the launcher: the functions that the checklist still misses.";

        void WriteFocus(string side, string target)
        {
            if (!LoadRecFunctions()) return;
            // The lines above the marker are the list that people keep by hand. The lines below are made again each time.
            var lines = new List<string>();
            // The list of the repository is the base when it exists (the target may hold an older made list). A package has
            // only the target.
            string repoList = Path.GetFullPath(Path.Combine(launcherFolder, @"..\..\instrumentation\recording\record_focus.txt"));
            string master = File.Exists(repoList) ? repoList : target;
            if (side == "server" && File.Exists(master)) {
                foreach (string line in File.ReadAllLines(master)) {
                    if (line == FocusMarker || line.StartsWith("# The functions that the checklist still misses")) break;
                    lines.Add(line);
                }
            }
            var kept = new HashSet<uint>();
            foreach (string line in lines) {
                var parts = line.Split(' ');
                uint rva;
                if (parts.Length >= 3 && parts[0].StartsWith("0x") &&
                    uint.TryParse(parts[0].Substring(2), System.Globalization.NumberStyles.HexNumber, null, out rva))
                    kept.Add(rva);
            }
            lines.Add(FocusMarker);
            lines.Add("# Format: rva class name");
            var total = TotalCalls(side);
            int added = 0;
            foreach (var f in recFunctions.Where(f => f.Side == side)) {
                int calls;
                total.TryGetValue(f.Rva, out calls);
                if (calls < RecordMinCalls && !kept.Contains(f.Rva) && added < FocusMax) {
                    lines.Add("0x" + f.Rva.ToString("x") + " diff " + f.Name);
                    added++;
                }
            }
            File.WriteAllText(target, string.Join("\n", lines.ToArray()) + "\n", Encoding.ASCII);
        }

        // ------------------------------------------------------------ the original server

        static readonly string[] RecordFiles = { "sv_args.txt", "sv_snapshot.bin", "sv_packets.bin", "subvulpes.log", "subvulpes.ini", "subvulpes_force.txt" };

        const string NoScenario = "None";

        string ScenarioFile()
        {
            foreach (string path in new[] {
                Path.Combine(launcherFolder, "recording_scenarios.txt"),
                Path.GetFullPath(Path.Combine(launcherFolder, @"..\..\instrumentation\recording\scenarios.txt")) }) {
                if (File.Exists(path)) return path;
            }
            return null;
        }

        // The scenarios of the forcer (instrumentation/hook/force.c): "[name]" starts one, its rules follow.
        // The name maps to the text of the rules.
        Dictionary<string, string> ReadScenarios()
        {
            var scenarios = new Dictionary<string, string>();
            string path = ScenarioFile();
            if (path == null) return scenarios;
            string name = null;
            var text = new StringBuilder();
            foreach (string line in File.ReadAllLines(path)) {
                if (line.StartsWith("[")) {
                    if (name != null) scenarios[name] = text.ToString();
                    name = line.Trim().Trim('[', ']');
                    text.Clear();
                } else if (name != null) {
                    text.Append(line).Append("\r\n");
                }
            }
            if (name != null) scenarios[name] = text.ToString();
            return scenarios;
        }

        // The image base of a 32-bit exe. The recording needs the copy with the fixed base address (tools/re/fixed_base.py).
        static uint ImageBase(string exe)
        {
            using (var stream = new FileStream(exe, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)) {
                var reader = new BinaryReader(stream);
                stream.Seek(0x3c, SeekOrigin.Begin);
                int pe = reader.ReadInt32();
                stream.Seek(pe + 0x34, SeekOrigin.Begin);
                return reader.ReadUInt32();
            }
        }

        const uint FixedBase = 0x20000000;

        string ServerProxyFile()
        {
            foreach (string path in new[] {
                Path.Combine(launcherFolder, "server_wsock32.dll"),
                Path.GetFullPath(Path.Combine(launcherFolder, @"..\hook\wsock32.dll")) }) {
                if (File.Exists(path)) return path;
            }
            return null;
        }

        // The files of an older recording move to <folder>\recordings_before\<time>. Nothing is deleted.
        static void MoveOldRecording(string dir)
        {
            string before = Path.Combine(dir, "recordings_before", DateTime.Now.ToString("yyyyMMdd-HHmmss"));
            foreach (string name in RecordFiles) {
                string path = Path.Combine(dir, name);
                if (!File.Exists(path)) continue;
                Directory.CreateDirectory(before);
                File.Move(path, Path.Combine(before, name));
            }
        }

        // The original server (subrosadedicated024d.exe) records its own calls through the wsock32.dll next to it.
        void PrepareOriginalServer()
        {
            string exe = Path.Combine(folder, "subrosadedicated024d.exe");
            if (ImageBase(exe) != FixedBase)
                throw new InvalidOperationException("subrosadedicated024d.exe is not the copy with the fixed base address (tools/re/fixed_base.py).");
            string proxy = ServerProxyFile();
            if (proxy == null)
                throw new InvalidOperationException("server_wsock32.dll is not next to the launcher (build the sv_wsock32 target).");
            MoveOldRecording(folder);
            // An old request would take the snapshot at the first second.
            File.Delete(Path.Combine(folder, "snapshot_now.txt"));
            File.Copy(proxy, Path.Combine(folder, "wsock32.dll"), true);
            File.WriteAllText(Path.Combine(folder, "subvulpes.ini"),
                "[hooks]\r\nenabled=1\r\nverify=0\r\noriginal_only=1\r\nown_main=0\r\nlog_calls=0\r\nlifted=1\r\n" +
                "record_args=1\r\nrecord_imports=1\r\nsnapshot_after=" + settings.Get("snapshot_after", "600") + "\r\n" +
                "capture_packets=0\r\nnet_guard=1\r\nrecord_total_mb=" + settings.Get("record_total_mb", "100") + "\r\n", Encoding.ASCII);
            // The forcer reads this file when the server starts. No file, no change of the game.
            string scenario = recScenario.Text;
            string rules;
            if (scenario != NoScenario && ReadScenarios().TryGetValue(scenario, out rules)) {
                File.WriteAllText(Path.Combine(folder, "subvulpes_force.txt"), rules, Encoding.ASCII);
                AddLine("[launcher] Scenario " + scenario + ": the server forces game values (see subvulpes_force.txt).");
            }
        }

        // A scenario can start bots: each line "#bot <arguments>" runs instrumentation/recording/bot.py against the
        // original server, 5 seconds after it starts. The bot needs Python on the PATH. The forcer ignores these lines.
        readonly List<Process> bots = new List<Process>();

        string BotScript()
        {
            foreach (string path in new[] {
                Path.Combine(launcherFolder, "bot.py"),
                Path.GetFullPath(Path.Combine(launcherFolder, @"..\..\instrumentation\recording\bot.py")) }) {
                if (File.Exists(path)) return path;
            }
            return null;
        }

        void StartScenarioBots()
        {
            string rules;
            if (recScenario.Text == NoScenario || !ReadScenarios().TryGetValue(recScenario.Text, out rules)) return;
            string script = BotScript();
            int pid = server.Id;
            foreach (string line in rules.Split('\n')) {
                string text = line.Trim();
                if (!text.StartsWith("#bot ")) continue;
                if (script == null) { AddLine("[launcher] The scenario has a bot, but bot.py was not found."); return; }
                string arguments = "-I \"" + script + "\" --pid " + pid + " " + text.Substring(5);
                var info = new ProcessStartInfo("python", arguments) {
                    UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardOutput = true, RedirectStandardError = true };
                try {
                    Task.Delay(5000).ContinueWith(t => {
                        var bot = Process.Start(info);
                        lock (bots) bots.Add(bot);
                        bot.OutputDataReceived += OnOutput;
                        bot.ErrorDataReceived += OnOutput;
                        bot.BeginOutputReadLine();
                        bot.BeginErrorReadLine();
                    });
                    AddLine("[launcher] Bot: " + text.Substring(5));
                } catch (Exception ex) {
                    AddLine("[launcher] Cannot start the bot: " + ex.Message);
                }
            }
        }

        void StopScenarioBots()
        {
            lock (bots) {
                foreach (Process bot in bots) {
                    try { if (!bot.HasExited) bot.Kill(); } catch { }
                }
                bots.Clear();
            }
        }

        // The recorder looks for snapshot_now.txt once each second, takes the snapshot and deletes the file.
        // The server and the client each have a recorder, so the file goes to both folders that run.
        void RequestSnapshot()
        {
            try {
                var folders = new List<string>();
                if (!string.IsNullOrEmpty(folder) && Directory.Exists(folder)) folders.Add(folder);
                if (client != null && clientDir != null) folders.Add(clientDir);
                foreach (string dir in folders) File.WriteAllText(Path.Combine(dir, "snapshot_now.txt"), "", Encoding.ASCII);
                AddLine("[launcher] The snapshot starts in about 1 second (the log says when it is written).");
            } catch (Exception ex) {
                AddLine("[launcher] Cannot ask for the snapshot: " + ex.Message);
            }
        }

        // When the original server closes, its files go to the folder of the test.
        void CollectOriginalServer()
        {
            StopScenarioBots();
            try {
                foreach (string name in RecordFiles) {
                    string path = Path.Combine(folder, name);
                    if (File.Exists(path)) File.Move(path, Path.Combine(testDir, name));
                }
                AddLine("[launcher] The server recording is in " + testDir + ".");
            } catch (Exception ex) {
                AddLine("[launcher] Cannot save the server recording: " + ex.Message);
            }
            RefreshChecklist();
        }

        // ------------------------------------------------------------ the client

        string ClientProxyFile()
        {
            foreach (string path in new[] {
                Path.Combine(launcherFolder, "client_wsock32.dll"),
                Path.GetFullPath(Path.Combine(launcherFolder, @"..\hook\client\wsock32.dll")) }) {
                if (File.Exists(path)) return path;
            }
            return null;
        }

        void StartClient()
        {
            if (client != null) return;
            string dir = recClientBox.Text.Trim();
            string exe = Path.Combine(dir, "subrosa_fixed.exe");
            if (dir.Length == 0 || !File.Exists(exe)) {
                message.Text = "The client folder needs subrosa_fixed.exe (the copy with the fixed base address).";
                return;
            }
            if (ImageBase(exe) != FixedBase) {
                message.Text = "subrosa_fixed.exe does not have the fixed base address (tools/re/fixed_base.py).";
                return;
            }
            string proxy = ClientProxyFile();
            if (proxy == null) {
                message.Text = "client_wsock32.dll is not next to the launcher (build the cl_wsock32 target).";
                return;
            }
            try {
                settings.Set("client_folder", dir);
                settings.Save();
                string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss");
                // The files of an older recording move away first, nothing is deleted.
                string before = Path.Combine(dir, "recordings_before", stamp);
                foreach (string name in new[] { "sv_args.txt", "sv_snapshot.bin", "sv_packets.bin", "subvulpes.log" }) {
                    string path = Path.Combine(dir, name);
                    if (!File.Exists(path)) continue;
                    Directory.CreateDirectory(before);
                    File.Move(path, Path.Combine(before, name));
                }
                File.Delete(Path.Combine(dir, "snapshot_now.txt"));
                File.Copy(proxy, Path.Combine(dir, "wsock32.dll"), true);
                File.WriteAllText(Path.Combine(dir, "subvulpes.ini"),
                    "[hooks]\r\nhooks=1\r\nlifted=1\r\noriginal_only=1\r\nverify=0\r\nrecord_args=1\r\nrecord_imports=1\r\n" +
                    "snapshot_after=" + settings.Get("client_snapshot_after", "240") + "\r\n" +
                    "record_total_mb=" + settings.Get("record_total_mb", "100") + "\r\n", Encoding.ASCII);
                WriteFocus("client", Path.Combine(dir, "record_focus.txt"));
                clientDir = dir;
                clientSaveDir = (server != null && testDir != null) ? Path.Combine(testDir, "client") : Path.Combine(logsRoot, stamp + "-client", "client");
                client = new Process { StartInfo = new ProcessStartInfo(exe) { WorkingDirectory = dir, UseShellExecute = false }, EnableRaisingEvents = true };
                client.Exited += delegate { if (!IsDisposed) BeginInvoke(new Action(OnClientExited)); };
                client.Start();
                recStartClient.Enabled = false;
                message.Text = "The client runs with the recorder. Join 127.0.0.1, port " + portBox.Value + ".";
            } catch (Exception ex) {
                if (client != null) { client.Dispose(); client = null; }
                message.Text = "Cannot start the client: " + ex.Message;
            }
        }

        void OnClientExited()
        {
            if (client == null) return;
            // 0 is a normal close. 0xC0000005 and the like are a crash.
            int exitCode = client.ExitCode;
            client.Dispose();
            client = null;
            recStartClient.Enabled = true;
            try {
                Directory.CreateDirectory(clientSaveDir);
                foreach (string name in new[] { "sv_args.txt", "sv_snapshot.bin", "sv_packets.bin", "subvulpes.log", "subvulpes.ini" }) {
                    string path = Path.Combine(clientDir, name);
                    if (File.Exists(path)) File.Move(path, Path.Combine(clientSaveDir, name));
                }
                message.Text = "The client closed with exit code " + (exitCode == 0 ? "0" : "0x" + exitCode.ToString("X8")) +
                               ". Its recording is in " + clientSaveDir + ".";
                AddLine("[launcher] The client closed with exit code 0x" + exitCode.ToString("X8") + ".");
            } catch (Exception ex) {
                message.Text = "Cannot save the client recording: " + ex.Message;
            }
            RefreshChecklist();
        }

        // The client starts a few seconds after the server, when the server recorded with the client option.
        void StartClientWithServer()
        {
            if (!recWithServer.Checked || !activeCaptures.Record) return;
            var timer = new Timer { Interval = 6000 };
            timer.Tick += delegate { timer.Stop(); timer.Dispose(); if (server != null) StartClient(); };
            timer.Start();
        }

        // Called from OnClock each second: the checklist follows the recording while someone plays.
        void RecordingClock()
        {
            if (!recTabActive || (server == null && client == null)) return;
            if ((DateTime.Now - recLastRefresh).TotalSeconds >= 15) RefreshChecklist();
        }
    }
}
