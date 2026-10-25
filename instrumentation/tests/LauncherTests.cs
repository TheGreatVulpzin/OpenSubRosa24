// Exercise the panel on its UI thread. Keep all artifacts under build/.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SubVulpes
{
    class LauncherTests
    {
        [DllImport("user32.dll")]
        static extern bool PrintWindow(IntPtr window, IntPtr target, uint flags);

        [DllImport("user32.dll")]
        static extern bool SetProcessDPIAware();

        static int passed;
        static string root, screens;
        static readonly BindingFlags Private = BindingFlags.Instance | BindingFlags.NonPublic;

        static void Check(bool condition, string label)
        {
            if (!condition) throw new Exception(label);
            Console.WriteLine("PASS: " + label);
            passed++;
        }

        static T Field<T>(LauncherForm form, string name)
        {
            return (T)typeof(LauncherForm).GetField(name, Private).GetValue(form);
        }

        static object Call(LauncherForm form, string name, params object[] arguments)
        {
            return typeof(LauncherForm).GetMethod(name, Private).Invoke(form, arguments);
        }

        static IEnumerable<Control> Controls(Control rootControl)
        {
            yield return rootControl;
            foreach (Control child in rootControl.Controls)
                foreach (var descendant in Controls(child)) yield return descendant;
        }

        static async Task Wait(Func<bool> condition, int milliseconds, string label)
        {
            var elapsed = Stopwatch.StartNew();
            while (!condition() && elapsed.ElapsedMilliseconds < milliseconds)
                await Task.Delay(100);
            Check(condition(), label);
        }

        static void Click(LauncherForm form, string name)
        {
            var button = Field<Button>(form, name);
            Control parent = button.Parent;
            var tabs = Field<TabControl>(form, "mainTabs");
            while (parent != null) {
                var page = parent as TabPage;
                if (page != null && page.Parent == tabs) tabs.SelectedTab = page;
                parent = parent.Parent;
            }
            button.PerformClick();
        }

        static async Task Capture(LauncherForm form, string name, string tab)
        {
            form.Activate();
            form.Refresh();
            await Task.Delay(300);
            form.WindowState = FormWindowState.Normal;
            Field<TabControl>(form, "mainTabs").SelectedTab = Field<Dictionary<string, TabPage>>(form, "pages")[tab];
            form.Refresh();
            if (form.Width < form.MinimumSize.Width || form.Height < form.MinimumSize.Height)
                throw new Exception("The launcher window is too small for a screenshot.");
            using (var bitmap = new Bitmap(form.Width, form.Height)) {
                using (var graphics = Graphics.FromImage(bitmap)) {
                    IntPtr target = graphics.GetHdc();
                    try {
                        if (!PrintWindow(form.Handle, target, 0)) throw new Exception("Cannot capture the launcher window.");
                    } finally { graphics.ReleaseHdc(target); }
                }
                var colors = new HashSet<int>();
                for (int y = 0; y < bitmap.Height; y += 17)
                    for (int x = 0; x < bitmap.Width; x += 17)
                        colors.Add(bitmap.GetPixel(x, y).ToArgb());
                if (colors.Count < 8) throw new Exception("The launcher screenshot is blank.");
                bitmap.Save(Path.Combine(screens, name + ".png"), ImageFormat.Png);
            }

            Console.WriteLine("SCREEN: " + name + ".png");
        }

        static void CopyDirectory(string source, string target)
        {
            Directory.CreateDirectory(target);
            foreach (string file in Directory.GetFiles(source))
                File.Copy(file, Path.Combine(target, Path.GetFileName(file)));
            foreach (string directory in Directory.GetDirectories(source))
                CopyDirectory(directory, Path.Combine(target, Path.GetFileName(directory)));
        }

        static void UpdateTests()
        {
            Func<string, bool, ServerRelease> release = (tag, isPreview) => new ServerRelease {
                tag_name = tag, name = "Server " + tag, prerelease = isPreview
            };
            var stable = release("v1.2.0", false);
            var preview = release("v1.3.0-rc.2", true);
            var draft = release("v9.0.0", false);
            draft.draft = true;
            var list = new[] { preview, draft, stable, release("server-data-0.24", false) };
            Check(ReleaseUpdate.Select(list, "v1.1.0", false) == stable, "updates exclude previews, drafts and data releases");
            Check(ReleaseUpdate.Select(list, "v1.1.0", true) == preview, "pre-release option includes previews");
            Check(ReleaseUpdate.Select(list, "v1.3.0", true) == null, "updates do not downgrade");
            Check(ReleaseUpdate.CompareVersions("v1.3.0", "v1.3.0-rc.2") > 0 &&
                  ReleaseUpdate.CompareVersions("v1.3.0-rc.10", "v1.3.0-rc.2") > 0, "version comparison follows numeric pre-release order");
            string file = Path.Combine(root, "checksum-fixture.zip");
            File.WriteAllText(file, "checksum fixture");
            string hash;
            using (var algorithm = SHA256.Create())
                hash = BitConverter.ToString(algorithm.ComputeHash(File.ReadAllBytes(file))).Replace("-", "");
            ReleaseUpdate.Verify(file, hash.ToLowerInvariant() + "  checksum-fixture.zip\n");
            Check(true, "SHA-256 accepts the matching release checksum");
            bool rejected = false;
            try { ReleaseUpdate.Verify(file, new string('0', 64)); } catch (InvalidDataException) { rejected = true; }
            Check(rejected, "SHA-256 rejects changed content");
            rejected = false;
            try { ReleaseUpdate.Verify(file, "invalid"); } catch (InvalidDataException) { rejected = true; }
            Check(rejected, "SHA-256 rejects malformed checksums");
            var previous = WebRequest.DefaultWebProxy;
            try {
                WebRequest.DefaultWebProxy = new WebProxy("http://127.0.0.1:1");
                rejected = false;
                try { ReleaseUpdate.Find("v1.0.0", false); } catch (WebException) { rejected = true; }
                Check(rejected, "unreachable network fails without an install");
                Check(ServicePointManager.SecurityProtocol == (SecurityProtocolType)3072, "update requests use TLS 1.2");
            } finally { WebRequest.DefaultWebProxy = previous; }
        }

        static void LogTests(string log)
        {
            string logs = Path.GetDirectoryName(log);
            foreach (string name in new[] { "packets.bin", "config.txt", "crash.dmp", "notes.txt", "sv_snapshot.bin", "sv_args.txt", "recording.bin", "version.txt" })
                File.WriteAllText(Path.Combine(logs, name), "excluded fixture");
            Directory.CreateDirectory(Path.Combine(logs, "recordings"));
            File.WriteAllText(Path.Combine(logs, "recordings", "server.log"), "excluded recording");
            string exported = Path.Combine(root, "exported-server.log");
            LauncherForm.CopyLog(log, exported);
            Check(File.ReadAllBytes(log).SequenceEqual(File.ReadAllBytes(exported)), "Save Log copies only the exact server.log content");
            Check(!File.ReadAllText(exported).Contains("excluded fixture"), "Save Log excludes packets, config, dumps, notes, recordings and version.txt");
            bool rejected = false;
            try { LauncherForm.CopyLog(log, log); } catch (IOException) { rejected = true; }
            Check(rejected && File.ReadAllBytes(log).SequenceEqual(File.ReadAllBytes(exported)), "Save Log refuses to overwrite its live source");
        }

        static async Task Run(LauncherForm form, string repo)
        {
            var mainTabs = Field<TabControl>(form, "mainTabs");
            var pages = Field<Dictionary<string, TabPage>>(form, "pages");
            Check(mainTabs.TabPages.Cast<TabPage>().Select(t => t.Text).SequenceEqual(new[] { "Server", "Server settings", "Launcher settings" }), "top tabs match the requested layout with Debug hidden");
            Check(!Controls(form).OfType<StatusStrip>().Any(), "server state has no duplicate status bar");
            Check(form.Text == "OpenSubRosa24 Server - v99.0.0", "window title contains only product and version");
            Check(Field<Label>(form, "missingText").Text == "Missing: data/, config.txt (created)", "first run names both missing files on one line");
            Check(File.ReadAllBytes(Path.Combine(root, "config.txt")).SequenceEqual(
                File.ReadAllBytes(Path.Combine(repo, "tools", "release", "default-config.ini"))), "missing config uses the exact public package default");
            await Capture(form, "01-first-run", "Server");
            string otherFolder = Path.Combine(root, "other-server");
            Directory.CreateDirectory(otherFolder);
            Call(form, "OpenServerFolder", otherFolder);
            using (var restored = new LauncherForm(root))
                Check(Field<string>(restored, "folder") == otherFolder, "launcher remembers the selected server folder");
            Call(form, "OpenServerFolder", root);
            Click(form, "startStop");
            Check(Field<Process>(form, "server") == null, "Start refuses missing data");
            CopyDirectory(Path.Combine(repo, "build", "package", "OpenSubRosa24-server-v0.0.2-test", "data"), Path.Combine(root, "data"));
            File.Copy(Path.Combine(repo, "build", "hook", "subvulpes-server.exe"), Path.Combine(root, "subvulpes-server.exe"));
            File.Copy(Path.Combine(repo, "build", "hook", "subvulpes-server-record.exe"), Path.Combine(root, "subvulpes-server-record.exe"));
            Field<CheckBox>(form, "recordBox").Enabled = true;
            Call(form, "ShowMissing", false);
            mainTabs.SelectedTab = pages["Server settings"];
            Field<TextBox>(form, "nameBox").Text = "Launcher test";
            string secret = "test-only-" + Guid.NewGuid().ToString("N");
            Field<TextBox>(form, "passwordBox").Text = secret;
            Field<NumericUpDown>(form, "playersBox").Value = 12;
            Field<NumericUpDown>(form, "roundBox").Value = 9;
            int port;
            using (var socket = new UdpClient(0)) port = ((IPEndPoint)socket.Client.LocalEndPoint).Port;
            Field<NumericUpDown>(form, "portBox").Value = port;
            Click(form, "saveButton");
            var config = new KeyFile(Path.Combine(root, "config.txt"));
            Check(config.Get("servername", "") == "Launcher test" && config.Get("serverpassword", "") == secret &&
                  config.Get("maxnumofplayers", "") == "12" && config.Get("roundtime", "") == "9" &&
                  config.Get("port", "") == port.ToString(), "Save writes server option keys");
            Check(Field<TextBox>(form, "passwordBox").UseSystemPasswordChar, "admin password stays masked");
            var show = Controls(form).OfType<CheckBox>().Single(c => c.Text == "Show");
            show.Checked = true;
            Check(!Field<TextBox>(form, "passwordBox").UseSystemPasswordChar, "Show reveals the admin password");
            show.Checked = false;
            Check(Field<TextBox>(form, "passwordBox").UseSystemPasswordChar, "Show can mask the password again");
            await Capture(form, "02-server-settings", "Server settings");
            mainTabs.SelectedTab = pages["Launcher settings"];
            Check(pages["Launcher settings"].Contains(Field<CheckBox>(form, "previewsBox").Parent), "pre-release option belongs to Launcher settings");
            Check(Field<ToolTip>(form, "tips").GetToolTip(Field<CheckBox>(form, "previewsBox")).Contains("test versions"), "pre-release tooltip explains test versions");
            await Capture(form, "03-launcher-settings", "Launcher settings");
            mainTabs.SelectedTab = pages["Server"];
            Click(form, "startStop");
            await Wait(() => Field<TextBox>(form, "console").Text.Contains("Server started on port "), 90000, "Start reaches the live server loop through SV_LOG");
            var process = Field<Process>(form, "server");
            Check(process.StartInfo.EnvironmentVariables["SV_SAMPLE"] == null &&
                  process.StartInfo.EnvironmentVariables["SV_PACKETS"] == null &&
                  process.StartInfo.EnvironmentVariables["SV_RECORD_DIR"] == null, "normal sessions disable developer captures");
            await Wait(() => Field<TextBox>(form, "console").Text.Contains("ticks/s"), 75000, "live status receives real tick and player counts");
            await Capture(form, "04-server-running", "Server");
            Click(form, "stopButton");
            await Wait(() => Field<Process>(form, "server") == null, 20000, "Stop waits for cooperative server exit");
            Check(File.ReadAllText(Path.Combine(root, "logs", "server.log")).Contains("Server stopped."), "Stop flushes the final server log line");
            Check(!File.ReadAllText(Path.Combine(root, "logs", "server.log")).Contains(secret), "server.log excludes the admin password");
            await Capture(form, "05-server-stopped", "Server");
            LogTests(Path.Combine(root, "logs", "server.log"));
            Check(Controls(form).OfType<Button>().Any(b => b.Text == "Save Log") && !Controls(form).OfType<Button>().Any(b => b.Text == "Pack logs"), "Server offers Save Log without a support zip button");
            string savedLog = Path.Combine(root, "saved-from-panel.log");
            await (Task)Call(form, "SaveLogTo", savedLog);
            Check(File.ReadAllBytes(savedLog).SequenceEqual(File.ReadAllBytes(Path.Combine(root, "logs", "server.log"))), "Save Log handler writes the chosen destination");
            mainTabs.SelectedTab = pages["Launcher settings"];
            var previousProxy = WebRequest.DefaultWebProxy;
            WebRequest.DefaultWebProxy = new WebProxy("http://127.0.0.1:1");
            await (Task)Call(form, "CheckForUpdates");
            WebRequest.DefaultWebProxy = previousProxy;
            Check(Field<Label>(form, "message").Text.Contains("no network connection"), "Check for updates shows a clear network error");
            await Capture(form, "06-update-offline", "Launcher settings");

            // Exercise the same command handler as Ctrl+Shift+D.
            var message = new Message();
            typeof(LauncherForm).GetMethod("ProcessCmdKey", Private).Invoke(form, new object[] { message, Keys.Control | Keys.Shift | Keys.D });
            Check(mainTabs.TabPages.Contains(pages["Debug"]) && new KeyFile(Path.Combine(root, "launcher.ini")).Get("developer", "") == "1", "Debug shortcut enables and persists Debug Mode");
            Field<CheckBox>(form, "debugModeBox").Checked = false;
            Check(!mainTabs.TabPages.Contains(pages["Debug"]), "Debug Mode hides the Debug tab");
            Field<CheckBox>(form, "debugModeBox").Checked = true;
            mainTabs.SelectedTab = pages["Debug"];
            await Capture(form, "07-debug-test", "Debug");
            var tabs = pages["Debug"].Controls[0].Controls.OfType<TabControl>().Single();
            tabs.SelectedIndex = 1;
            await Capture(form, "08-debug-captures", "Debug");
            Check(Field<ComboBox>(form, "presetBox").Items.Count == 5, "Debug retains all five presets");
            foreach (Preset preset in Field<ComboBox>(form, "presetBox").Items) {
                Field<ComboBox>(form, "presetBox").SelectedItem = preset;
                Check(preset.Captures == null || Field<Captures>(form, "captures").Same(preset.Captures), "preset captures: " + preset.Name);
            }
            Field<ComboBox>(form, "presetBox").SelectedIndex = 0;
            tabs.SelectedIndex = 0;
            mainTabs.SelectedTab = pages["Server"];
            Click(form, "startStop");
            await Wait(() => Field<TextBox>(form, "console").Text.Contains("Server started on port "), 90000, "Debug Mode applies the preset when Start is on Server");
            Check(Field<Process>(form, "server").StartInfo.EnvironmentVariables["SV_SAMPLE"] != null, "Normal Playtest retains its speed capture");
            Click(form, "stopButton");
            await Wait(() => Field<Process>(form, "server") == null, 20000, "Debug Stop flushes captures");
            await Capture(form, "09-debug-finished", "Debug");
            Controls(form).OfType<Button>().First(b => b.Text == "Pack this test").PerformClick();
            await Wait(() => Directory.GetFiles(Path.Combine(root, "logs"), "test-*.zip").Length == 1, 15000, "Pack this test retains its developer archive");
            tabs.SelectedIndex = 2;
            await Capture(form, "10-debug-past-tests", "Debug");
            Check(Field<ListView>(form, "testsList").Items.Count == 1, "Past tests lists the debug session");
            Check(!File.ReadAllText(Path.Combine(root, "launcher.ini")).Contains(secret), "launcher.ini excludes the admin password");
            var standard = Controls(form).Where(c => c != Field<TextBox>(form, "console")).ToArray();
            Check(standard.All(c => c.ForeColor == SystemColors.ControlText), "all non-log controls use system text color");
            Check(standard.All(c => c.Font.FontFamily.Name == SystemFonts.MessageBoxFont.FontFamily.Name &&
                  c.Font.Size == SystemFonts.MessageBoxFont.Size), "all non-log controls use one system font and size");
            Field<CheckBox>(form, "debugModeBox").Checked = false;
            mainTabs.SelectedTab = pages["Server"];
            Click(form, "startStop");
            await Wait(() => Field<TextBox>(form, "console").Text.Contains("Server started on port "), 90000, "server restarts before window-close test");
            form.Close();
            await Wait(() => form.IsDisposed, 20000, "window close waits for safe server stop");
            Check(File.ReadAllText(Path.Combine(root, "logs", "server.log")).Contains("Server stopped."), "window close flushes the server log");
            Console.WriteLine("SUMMARY: " + passed + " checks passed");
        }

        static async Task ServerScreens(LauncherForm form)
        {
            Field<CheckBox>(form, "debugModeBox").Checked = false;
            Click(form, "startStop");
            await Wait(() => Field<TextBox>(form, "console").Text.Contains("Server started on port "), 90000, "server runs for the live screenshot");
            await Capture(form, "04-server-running", "Server");
            Click(form, "stopButton");
            await Wait(() => Field<Process>(form, "server") == null, 20000, "server stops after the live screenshot");
            await Capture(form, "05-server-stopped", "Server");
            Field<CheckBox>(form, "debugModeBox").Checked = true;
            var pages = Field<Dictionary<string, TabPage>>(form, "pages");
            var tabs = pages["Debug"].Controls[0].Controls.OfType<TabControl>().Single();
            tabs.SelectedIndex = 1;
            await Capture(form, "08-debug-captures", "Debug");
            Field<CheckBox>(form, "debugModeBox").Checked = false;
            form.Close();
        }

        [STAThread]
        static int Main(string[] args)
        {
            bool serverScreens = args.Length == 3 && args[1] == "--server-screens";
            if (args.Length != 1 && !serverScreens) throw new ArgumentException("Pass the repository directory.");
            string repo = Path.GetFullPath(args[0]);
            root = serverScreens ? Path.GetFullPath(args[2]) : Path.Combine(repo, "build", "launcher-tests", Guid.NewGuid().ToString("N"));
            if (!root.StartsWith(Path.Combine(repo, "build", "launcher-tests") + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                throw new ArgumentException("The fixture must stay under build/launcher-tests/.");
            screens = Path.Combine(repo, "build", "launcher-screens");
            Directory.CreateDirectory(root);
            Directory.CreateDirectory(screens);
            if (!serverScreens) {
                foreach (string screenshot in Directory.GetFiles(screens, "*.png")) File.Delete(screenshot);
                File.WriteAllText(Path.Combine(root, "version.txt"), "v99.0.0\ncommit fixture\nbuilt fixture\n");
            }
            Console.WriteLine("FIXTURE: " + root);
            if (!serverScreens) UpdateTests();
            SetProcessDPIAware();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            var form = new LauncherForm(root) { TopMost = true, StartPosition = FormStartPosition.Manual, Location = new Point(40, 40) };
            int result = 1;
            form.Shown += async delegate {
                try {
                    if (serverScreens) await ServerScreens(form);
                    else await Run(form, repo);
                    result = 0;
                }
                catch (Exception error) { Console.WriteLine("FAIL: " + error); }
                if (!form.IsDisposed) {
                    var task = (Task<bool>)Call(form, "StopServer");
                    await task;
                    form.Dispose();
                }
                Application.ExitThread();
            };
            form.Show();
            Application.Run();
            return result;
        }
    }
}
