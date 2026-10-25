# Server releases

Download the server package from [Releases](https://github.com/TheGreatVulpzin/OpenSubRosa24/releases).
Each release contains a zip and its `.zip.sha256` file.
Extract the zip, open `subvulpes-launcher.exe`, set the server name and admin password, then click **Start server**.
The package includes `data/` and a default `config.txt`. No separate server installation is needed.

## Build a local package

Use Windows, MSVC x86, CMake, Git and Windows PowerShell 5.1 or PowerShell 7.
The launcher uses the .NET Framework 4 C# compiler that comes with Windows.
The build needs no game executable or recordings. The package needs the approved data directory.

```powershell
cmd /c tools\build_hook.bat
cmd /c tools\launcher\build.bat
python -I tools/ci/check_repo.py
.\instrumentation\tests\test_package.ps1
.\instrumentation\tests\test_release_notes.ps1
.\tools\release\make_package.ps1 -Version v0.0.2-test -DataDir original\blackhouser\data
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

The launcher updates only the three executables, `LEIA-ME.txt`, `version.txt` and `record_focus.txt`.
It saves old files under `backup/`. It keeps `config.txt`, `data/`, `readme.txt` and `user/`.
For a first installation, extract the complete package into a new directory.

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
