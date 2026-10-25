# Server releases

Download the server package from [Releases](https://github.com/TheGreatVulpzin/OpenSubRosa24/releases).
Each release contains a zip and its `.zip.sha256` file.
Extract the zip, open `subvulpes-launcher.exe`, select the server folder, then save the server settings.
Click **Start** on Server. **Stop** and window close request a safe stop between server ticks.
The package includes `data/` and a default `config.txt`. No separate server installation is needed.

## Build a local package

Use Windows, MSVC x86, CMake, Git and Windows PowerShell 5.1 or PowerShell 7.
The launcher uses the .NET Framework 4 C# compiler that comes with Windows.
It stays x86 and uses no external packages. The public default configuration is an embedded resource.
The build needs no game executable or recordings. The package needs the approved data directory.

```powershell
cmd /c tools\build_hook.bat
cmd /c tools\launcher\build.bat
python -I tools/ci/check_repo.py
.\instrumentation\tests\test_package.ps1
.\instrumentation\tests\test_release_notes.ps1
.\tools\release\make_package.ps1 -Version v0.0.2-test -DataDir original\blackhouser\data
.\instrumentation\tests\test_launcher.ps1
```

Check each command exit code before the next command.
`-DataDir` names the directory that contains `city.sbc`, `block/`, `building/`, `texture/` and `weapon/`.
Without `-Version`, the script uses the Actions tag or an exact tag at `HEAD`.
An explicit version does not create a tag. The script normalizes `0.0.2-test` to `v0.0.2-test`.

Output:

```text
build/package/OpenSubRosa24-server-v0.0.2-test/
build/package/OpenSubRosa24-server-v0.0.2-test.zip
build/package/OpenSubRosa24-server-v0.0.2-test.zip.sha256
```

The package contains three executables, `LEIA-ME.txt`, `version.txt`, `record_focus.txt`, `config.txt`, `readme.txt` and `data/`.
`version.txt` contains the version tag, short commit hash and UTC package date.
The script checks paths, executable formats, game binary hashes, public defaults and the final zip entries.
It rejects private configuration and unexpected files left by an earlier run.
Use a new version or an empty package directory when the data file list changes.
Do not commit anything under `build/`.

The launcher test opens the panel and runs the real server in a new directory under `build/launcher-tests/`.
It uses `build/package/OpenSubRosa24-server-v0.0.2-test/data/` as its local data fixture.
It checks default configuration, saved server keys, masked passwords, Start, Stop, window close and Debug presets.
It creates fake packet, configuration, dump, note and recording files, then checks that Save Log exports only the log.
Screenshots go to `build/launcher-screens/`. Test network errors and checksum failures use local fixtures.
The launcher test uses the real package version for its screenshots.
A full automatic update needs a newer published release with its server zip and `.zip.sha256` asset.

The launcher updates only the three executables, `LEIA-ME.txt`, `version.txt` and `record_focus.txt`.
It saves old files under `backup/`. It keeps `config.txt`, `data/`, `readme.txt` and `user/`.
For a first installation, extract the complete package into a new directory.

## Launcher operation

The launcher stores the last server folder as `server_folder` in `launcher.ini` beside the launcher.
Server options use `servername`, `maxnumofplayers`, `roundtime`, `port` and `serverpassword` from `src/game/options.c`.
The password starts masked. **Show** reveals or masks it. It never enters `launcher.ini` or launcher log messages.
The player limit is 2 to 128. Round time is 1 to 30 minutes in Round mode.
If `config.txt` is missing, the launcher creates the embedded package default and names the missing file on one line.
It also names missing `data/`. Start needs that directory and the selected server executable.

Normal sessions write `logs/server.log` through `SV_LOG`. The launcher reads that file for live output.
Players and tick speed update when the server writes its minute status line.
Stop signals the named event in `SV_STOP_EVENT`. The server checks it between ticks, then flushes its files.
The launcher waits up to 15 seconds. A timeout leaves the process running and reports the error.
It does not replace safe shutdown with forced termination. Older server executables need the matching launcher update.

**Check for updates** reads the repository releases API with TLS 1.2 and compares tags with the first line of `version.txt`.
It excludes drafts, data releases and pre-releases by default. `pre_releases=1` enables pre-releases.
The Include pre-releases checkbox in Launcher settings writes the same option. Version comparison prevents downgrades.
The launcher shows only the release name, downloads its server zip and checks the release `.sha256` file.
Only a matching SHA-256 reaches the existing `UpdateFiles` installer.
The installer saves a backup and preserves `config.txt`, `data/`, `readme.txt` and `user/`.
The launcher executable updates at its own path if the selected server folder differs.
For **Update from a zip**, check the downloaded `.sha256` yourself before selecting the zip.

**Save Log** opens a file chooser and exports only the current session `server.log` as text.
The saved log and live output mask the current admin password and all text after `/admin ` on each line.
The saved log uses UTF-8. The source log stays unchanged.
It creates no zip and never scans directories for other files.
Configuration, packets, dumps, notes, recordings and version metadata stay outside the saved log.
It refuses to overwrite the live source file.

The top tabs are **Server**, **Server settings**, **Launcher settings** and **Debug**.
The window title reads the first line of `version.txt` in the selected server folder.
Server has Start, Stop, one state line, live output and Save Log. There is no second state in a status bar.
Server settings has Server, Game and Network groups with Save and Undo.
Launcher settings has the server folder, updates, Include pre-releases and Debug Mode.
The pre-release tooltip explains that these are test versions.

Enable **Debug Mode** in Launcher settings to show Debug. It is off by default.
**Ctrl+Shift+D** toggles the same option. `developer=1` in `launcher.ini` also enables it.
The next Start uses the selected preset when Debug Mode is on, regardless of the selected tab.
With Debug Mode off, Start runs without test captures.
Debug retains Normal Playtest, Harness Recording, Playtest (For Speed Issues), Crashhunt and Custom.
It also retains per-test directories, capture controls, marks, dumps, past tests and **Pack this test**.
Test archives can contain private data. Check them before sharing.

## Write the release summary

Commit and review the changes first. Wait for CI success on the commit you will tag.
Use an annotated version tag. Its message becomes the summary:

```powershell
git push origin main
gh run list --repo TheGreatVulpzin/OpenSubRosa24 --workflow ci.yml --limit 10
git tag -a v0.0.2-test -m "Windows server package with game data and a ready-to-edit configuration."
git push origin v0.0.2-test
```

These commands are for the maintainer. Do not run them before review and CI success.
Use the tag message for the short release summary. GitHub supplies the change list.
A manual run under **Actions > Release server package > Run workflow** accepts an existing `tag` and an optional `summary`.
A nonempty manual summary takes precedence over the tag message. An empty summary uses the annotated tag message.
If both are empty, the workflow logs a warning and uses `Server package.`.

The workflow downloads `server-data-0.24.zip` and its checksum with `GITHUB_TOKEN`.
It fails with a clear message if the data release or either asset is missing.
It checks SHA-256 and archive paths before extraction, then passes the data directory to the package script.
Only the release workflow requests `contents: write`. No new secret is needed.

The release body uses `tools/release/release-notes.md`.
The version comes from the tag. The checksum comes from the package `.sha256` file.
GitHub's `generate-notes` API supplies changes since the nearest earlier version tag in the commit history.
The data tag and development tags do not set that range. The first version release requests all changes.
The formatter removes generated headings so all generated content stays inside **Changes**.
The title is `OpenSubRosa24 Server {version}`.
The release body starts with the summary.
Tags that contain `-` create pre-releases and do not become the latest release.
An existing release causes publication to fail. The workflow does not overwrite release assets.

## Preview the body locally

Save example GitHub notes in `build/package/changelog.md`, then run:

```powershell
.\tools\release\make_release_notes.ps1 -Version v0.0.2-test -Checksum build/package/OpenSubRosa24-server-v0.0.2-test.zip.sha256 -ChangelogFile build/package/changelog.md -Summary "Windows server package with game data and a ready-to-edit configuration."
Get-Content build/package/release-notes.md
```

The formatter creates no tag or release. Omit `-Summary` to read an existing annotated tag message.
Check publication with `gh release view v0.0.2-test --repo TheGreatVulpzin/OpenSubRosa24` after the maintainer pushes the tag.
