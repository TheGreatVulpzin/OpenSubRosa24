# Server releases

Download the server package from [Releases](https://github.com/TheGreatVulpzin/OpenSubRosa24/releases).
Each release contains a zip and its `.zip.sha256` checksum file.

## Build a local package

Use Windows, MSVC x86, CMake, Git and Windows PowerShell 5.1 or PowerShell 7.
The launcher uses the .NET Framework 4 C# compiler that comes with Windows.
No game files or recordings are needed to build the package.

From the repository root, run:

```powershell
cmd /c tools\build_hook.bat
cmd /c tools\launcher\build.bat
python -I tools/ci/check_repo.py
.\tools\release\make_package.ps1 -Version v0.0.1-test
```

Check each command exit code before the next command.
Without `-Version`, the script uses the Actions tag or an exact tag at `HEAD`.
An explicit version does not create a tag.
The script normalizes `0.0.1-test` to `v0.0.1-test`.

Output:

```text
build/package/OpenSubRosa24-server-v0.0.1-test/
build/package/OpenSubRosa24-server-v0.0.1-test.zip
build/package/OpenSubRosa24-server-v0.0.1-test.zip.sha256
```

The package folder contains only:

- `subvulpes-server.exe`
- `subvulpes-server-record.exe`
- `subvulpes-launcher.exe`
- `LEIA-ME.txt` (English and Brazilian Portuguese)
- `version.txt` (version tag, short commit hash and UTC package date)
- `record_focus.txt` (the recorder function selection)

The recorder can run without `record_focus.txt`. This package includes the repository selection.
The launcher accepts these six names from any folder inside the zip.
Its **Update from a zip...** action saves previous files under `backup/` and preserves user configuration and game data.

The script copies individual build outputs and repository text files. It never copies a server installation.
It rejects extra files, directories, links, known game hashes and password assignments in package text.
This includes `config.txt`, game data and recordings, even after a previous package run.
It also checks the final zip entry list.
The text check detects common secret formats. It cannot prove that arbitrary executable code contains no embedded secret.
Build only reviewed source. Never add credentials to source or package inputs.
Do not commit anything under `build/`.

## Publish a test release

Commit and review the changes first. Publish only when the maintainer requests it.
Run these commands from the repository root:

```powershell
git push origin main
gh run list --repo TheGreatVulpzin/OpenSubRosa24 --workflow ci.yml --limit 10
```

Wait for CI success on the commit you will tag. If CI fails, inspect its failed steps:

```powershell
gh run view RUN_ID --repo TheGreatVulpzin/OpenSubRosa24 --log-failed
```

Then create and push the test tag:

```powershell
git tag -a v0.0.1-test -m "Test server package v0.0.1-test"
git push origin v0.0.1-test
gh run list --repo TheGreatVulpzin/OpenSubRosa24 --workflow release.yml --limit 5
```

The workflow runs on tags that start with `v`.
It validates a version such as `v0.1.0` or `v0.1.0-rc1` and checks that the tag matches the checkout.
It uses Windows, MSVC x86 and the .NET Framework 4 compiler.
It builds both servers and the launcher, runs repository, C and package tests, and creates the package.
It publishes a GitHub Release with the zip and checksum through `GITHUB_TOKEN`.
Only the release workflow requests `contents: write`. No new secret is needed.
Actions use fixed commit hashes. Tags that contain `-` create pre-releases and do not become the latest release.
An existing release causes publication to fail. The workflow does not overwrite release assets.

For a manual run, open **Actions > Release server package > Run workflow**.
Select the trusted workflow branch and enter an existing version tag.
The workflow builds that tag. It does not create a tag.
The release workflow must exist on the default branch before GitHub offers this manual action.

Check the published assets:

```powershell
gh release view v0.0.1-test --repo TheGreatVulpzin/OpenSubRosa24
```

## Delete the test release

These commands delete the test release, its assets, the remote tag and the local tag.
Run them only when you intend to remove that test version:

```powershell
gh release delete v0.0.1-test --repo TheGreatVulpzin/OpenSubRosa24 --yes
git push origin :refs/tags/v0.0.1-test
git tag -d v0.0.1-test
```
