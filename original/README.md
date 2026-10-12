# original/

Original files. **Not versioned** (they are proprietary to Cryptic Sea).
Only this README and `MANIFEST.sha256` go into git.

| Folder | Content |
| --- | --- |
| `client/` | Sub Rosa ("the group's SubRosa" copy): Windows client, Linux x64, macOS i386 and dedicated server. |
| `blackhouser/` | "Black Houser" server: `subrosadedicated024d.exe` + config and map. |
| `archives/` | The original `.rar`/`.zip` files, untouched. |
| `vanilla/` | Official versions downloaded for comparison (0.22, 0.24b, 0.24c, 0.24d, 0.25b). The packages are in `vanilla/archives/`. |

Sources of `vanilla/`: `https://crypticsea.gart.sh/Sub Rosa/` and `https://archive.org/details/sub-rosa-0.22`.

The package with all versions (`SUB ROSA ALL.rar`, 2.1 GB, from `https://archive.org/download/sub-rosa-all`) is processed in the cloud. See `docs/versoes-todas.md`.

To check that the files match the manifest:

```bash
python -I tools/re/manifest.py check
```

Anyone who clones the repository must put these files in the folders by hand.
