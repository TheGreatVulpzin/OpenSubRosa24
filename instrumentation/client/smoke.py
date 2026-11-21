"""Run window and editor smoke scenarios in separate client processes."""
import argparse
from collections import Counter
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import subprocess
import struct
import time

ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = {"menus": 360, "explore": 280, "play": 600, "crash": 360, "buildings": 1800, "level": 360, "model": 300}
READY_SECONDS = 15


def last_error(text):
    errors = [line.strip() for line in text.splitlines() if line.startswith(
        ("SMOKE FAIL ", "SMOKE TIMEOUT ", "exception ", "no C function ", "import "))]
    if not errors:
        return "no error line"
    modules = [line.removeprefix("exception module: ") for line in errors if line.startswith("exception module: ")]
    specific = [line for line in errors if not line.endswith(": incomplete run")]
    last = (specific or errors)[-1]
    return f"{modules[-1]} | {last}" if modules and last.startswith("exception 0x") else last


def run_process(command, scenario, cwd, env, log, timeout, ready_seconds=READY_SECONDS):
    """Require a completion marker and a successful process exit."""
    start = time.monotonic()
    reason = ""
    stage = "startup"
    with log.open("w", encoding="utf-8") as output:
        try:
            process = subprocess.Popen(command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT)
        except OSError as error:
            return {"scenario": scenario, "ok": False, "error": str(error), "stage": stage,
                    "timeout": False, "seconds": 0, "exit_code": None, "log": str(log)}
        try:
            while True:
                try:
                    process.wait(timeout=0.2)
                    break
                except subprocess.TimeoutExpired:
                    text = log.read_text(encoding="utf-8", errors="replace")
                    elapsed = time.monotonic() - start
                    if elapsed >= ready_seconds and f"SMOKE READY {scenario}\n" not in text:
                        reason = "SR_SMOKE unavailable: no READY marker"
                    elif elapsed >= timeout:
                        reason = f"process timeout after {timeout:g} seconds"
                    if reason:
                        process.kill()
                        process.wait(timeout=10)
                        break
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
    text = log.read_text(encoding="utf-8", errors="replace")
    stages = [line.removeprefix("SMOKE STAGE ") for line in text.splitlines() if line.startswith("SMOKE STAGE ")]
    if stages:
        stage = stages[-1]
    timeouts = [line.removeprefix("SMOKE TIMEOUT ") for line in text.splitlines() if line.startswith("SMOKE TIMEOUT ")]
    if timeouts:
        stage = timeouts[-1]
    failed = "SMOKE FAIL " in text or bool(timeouts)
    complete = f"SMOKE READY {scenario}\n" in text and f"SMOKE PASS {scenario}\n" in text
    ok = process.returncode == 0 and complete and not failed and not reason
    if not ok and not reason:
        reason = last_error(text)
        if reason == "no error line":
            reason = f"exit {process.returncode}; missing completion marker" if not complete else f"exit {process.returncode}"
    return {"scenario": scenario, "ok": ok, "error": reason, "stage": stage,
            "timeout": bool(timeouts) or reason.startswith("process timeout"),
            "seconds": round(time.monotonic() - start, 2), "exit_code": process.returncode, "log": str(log)}


@contextmanager
def preserve_inputs(folder):
    """Restore files that the client can save when an editor closes."""
    paths = {folder / "config.txt", folder / "client.srk", folder / "Sub Rosa/config.txt", folder / "Sub Rosa/client.srk", folder / "user/server.srk", folder / "data/city.sbc"}
    paths.update((folder / "data/building").glob("*.sbb"))
    paths.update(folder.glob("*.srk"))
    paths.update((folder / "user").glob("*.srk"))
    paths.update((folder / "Sub Rosa").rglob("*.srk"))
    before_buildings = set((folder / "data/building").glob("*.sbb"))
    saved = {path: path.read_bytes() if path.exists() else None for path in paths}
    try:
        yield
    finally:
        for path in set((folder / "data/building").glob("*.sbb")) - before_buildings:
            path.unlink()
        for path, data in saved.items():
            if data is None:
                path.unlink(missing_ok=True)
            elif not path.exists() or path.read_bytes() != data:
                path.write_bytes(data)


def read_bitmap(path):
    """Read the uncompressed 24 bit BMP files from the driver."""
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("invalid BMP header")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from("<iiHHI", data, 18)
    if not 0 < width <= 8192 or not 0 < abs(height) <= 8192 or planes != 1 or bits != 24 or compression:
        raise ValueError("unsupported BMP format")
    stride = (width * 3 + 3) & ~3
    if offset < 54 or len(data) != offset + stride * abs(height):
        raise ValueError("incomplete BMP pixels")
    rows = [data[offset + y * stride:offset + y * stride + width * 3] for y in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), b"".join(rows)


def image_metrics(path):
    width, height, pixels = read_bitmap(path)
    colors = Counter(pixels[i:i + 3] for i in range(0, len(pixels), 3))
    count = width * height
    mean = [sum(color[channel] * number for color, number in colors.items()) / count for channel in (2, 1, 0)]
    black = colors[b"\0\0\0"] / count
    return {"width": width, "height": height, "black_fraction": black, "distinct_colors": len(colors),
            "mean_rgb": mean, "ok": black < 1 and len(colors) > 1}


def pixel_difference(first, second):
    aw, ah, a = read_bitmap(first)
    bw, bh, b = read_bitmap(second)
    if (aw, ah) != (bw, bh):
        return 100.0
    different = sum(a[i:i + 3] != b[i:i + 3] for i in range(0, len(a), 3))
    return 100.0 * different / (aw * ah)


def collect_artifacts(directory, result):
    """Validate saved pixels even when the client crashes before completion."""
    shots, errors = [], []
    manifest = directory / "shots.jsonl"
    if manifest.exists():
        for line in manifest.read_text(encoding="utf-8").splitlines():
            try:
                entry = json.loads(line)
                relative = Path(entry["path"])
                path = (directory / relative).resolve()
                if relative.is_absolute() or not path.is_relative_to((directory / "shots").resolve()):
                    raise ValueError("capture path outside shots")
                metrics = image_metrics(path)
                entry.update(metrics)
                if not entry["ok"]:
                    errors.append(f"{entry['name']}: image is all black or one color")
                shots.append(entry)
            except (OSError, ValueError, KeyError, TypeError) as error:
                errors.append(f"invalid capture: {error}")
    if not shots:
        errors.append("no framebuffer captures; executable needs SR_SMOKE_OUTPUT support")
    text = Path(result["log"]).read_text(encoding="utf-8", errors="replace")
    checks = [line.removeprefix("SMOKE ASSERT ") for line in text.splitlines() if line.startswith("SMOKE ASSERT ")]
    result.update(shots=shots, checks=checks, artifact_errors=errors, index=str(directory / "index.md"))
    if errors:
        result["ok"] = False
        result["error"] = "; ".join(filter(None, [result["error"], *errors]))
    write_index(directory, result)


def write_index(directory, result):
    lines = [f"# {result['scenario']}", "", f"Result: {'PASS' if result['ok'] else 'FAIL'}.", "",
             result.get("error", ""), "", "[Process log](run.log). [Menu controls](screens.json).", ""]
    for shot in result.get("shots", []):
        mean = ", ".join(f"{channel:.2f}" for channel in shot["mean_rgb"])
        lines.extend([f"- [{shot['name']}]({shot['path']})", f"  Black: {shot['black_fraction']:.6%}. "
                      f"Colors: {shot['distinct_colors']}. Mean RGB: {mean}."])
    if result.get("checks"):
        lines.extend(["", "Checks:", "", *[f"- {check}" for check in result["checks"]]])
    if result.get("comparison"):
        lines.extend(["", "Comparison warnings:", "", *[f"- {warning}" for warning in result["comparison"]]])
    (directory / "index.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def compare_results(first, second):
    comparisons = []

    def by_name(shots):
        occurrences, indexed = Counter(), {}
        for shot in shots:
            occurrences[shot["name"]] += 1
            indexed[shot["name"], occurrences[shot["name"]]] = shot
        return indexed

    for a, b in zip(first, second):
        a_dir, b_dir = Path(a["index"]).parent, Path(b["index"]).parent
        a_shots, b_shots = by_name(a.get("shots", [])), by_name(b.get("shots", []))
        warnings = []
        for key in sorted(a_shots.keys() | b_shots.keys()):
            name, occurrence = key
            entry = {"scenario": a["scenario"], "name": name, "occurrence": occurrence}
            if key not in a_shots or key not in b_shots:
                entry.update(different_percent=None, warning="missing counterpart")
                warnings.append(f"{name}[{occurrence}]: missing counterpart")
            else:
                first_path, second_path = a_shots[key]["path"], b_shots[key]["path"]
                difference = pixel_difference(a_dir / first_path, b_dir / second_path)
                entry.update(path=first_path, compare_path=second_path, different_percent=difference)
                if difference:
                    warnings.append(f"{name}[{occurrence}]: {difference:.6f}% different pixels")
            comparisons.append(entry)
        a["comparison"] = warnings
        write_index(a_dir, a)
    return comparisons

@contextmanager
def client_lock():
    """Reject concurrent clients started by this executor."""
    import msvcrt
    path = ROOT / "build/client_smoke/.client.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as lock:
        lock.write(b"0")
        lock.flush()
        lock.seek(0)
        try:
            msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError as error:
            raise RuntimeError("another smoke executor owns the client window") from error
        try:
            yield
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


def run_suite(exe, scenarios, output, timeout, points):
    folder = ROOT / "build/client_test"
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ("SR_SAMPLE", "SR_DENORM"):
        env.pop(key, None)
    env.update(SR_TRACE=os.environ.get("SR_TRACE", "3"), SR_DEBUGIT="1", SR_SMOKE_SHOTS=points)
    env["PATH"] = str(ROOT / "build/client_deps") + os.pathsep + env.get("PATH", "")
    driver_present = b"SR_SMOKE" in exe.read_bytes()
    results, stalled = [], Counter()
    for index, scenario in enumerate(scenarios):
        directory = output / scenario
        directory.mkdir(parents=True, exist_ok=True)
        shots = directory / "shots"
        shots.mkdir(exist_ok=True)
        # Remove prior artifacts so a crash cannot reuse a successful capture.
        for path in shots.glob("*.bmp"):
            path.unlink()
        for name in ("shots.jsonl", "screens.json", "index.md"):
            (directory / name).unlink(missing_ok=True)
        env.update(SR_SMOKE=scenario, SR_SMOKE_OUTPUT=str(directory))
        log = directory / "run.log"
        if not driver_present:
            reason = "SR_SMOKE unavailable: executable has no smoke driver"
            log.write_text(reason + "\n", encoding="utf-8")
            result = {"scenario": scenario, "ok": False, "error": reason, "stage": "preflight",
                      "timeout": False, "seconds": 0, "exit_code": None, "log": str(log)}
        else:
            with preserve_inputs(folder):
                result = run_process([str(exe), "--run"], scenario, folder, env, log, timeout or SCENARIOS[scenario])
        collect_artifacts(directory, result)
        results.append(result)
        print(f"{exe.stem}/{scenario}: {'ok' if result['ok'] else 'FAILED'} | {result['error'] or '-'}", flush=True)
        if result["timeout"]:
            stalled[result["stage"]] += 1
            if stalled[result["stage"]] >= 2:
                for remaining in scenarios[index + 1:]:
                    directory = output / remaining
                    directory.mkdir(exist_ok=True)
                    skipped = {"scenario": remaining, "ok": False, "error": "not run: same stage failed twice",
                               "shots": [], "index": str(directory / "index.md")}
                    write_index(directory, skipped)
                    results.append(skipped)
                break
    (output / "summary.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    return results


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("scenarios", nargs="*", metavar="scenario")
    parser.add_argument("--timeout", type=float, help="Process limit in seconds for each scenario.")
    parser.add_argument("--output", type=Path, help="Root folder for logs, captures and summary.json.")
    parser.add_argument("--compare", type=Path, help="Run the same scenarios with a second executable.")
    parser.add_argument("--shots", default="*", help="Comma-separated capture names. The default selects all names.")
    args = parser.parse_args(argv)
    scenarios = args.scenarios or list(SCENARIOS)
    if any(name not in SCENARIOS for name in scenarios):
        parser.error("Choose " + ", ".join(SCENARIOS) + ".")
    if args.timeout is not None and (not math.isfinite(args.timeout) or args.timeout <= 0):
        parser.error("The timeout must be positive.")
    if not args.shots or (args.shots != "*" and any(not name or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-" for c in name)
                                                   for name in args.shots.split(","))):
        parser.error("Capture names must use lowercase letters, digits or hyphens.")
    executables = [args.exe.resolve()] + ([args.compare.resolve()] if args.compare else [])
    for exe in executables:
        if not exe.is_file():
            parser.error(f"File not found: {exe}")
    if len(executables) > 1 and executables[0].stem == executables[1].stem:
        parser.error("Comparison executables need different names.")
    if not (ROOT / "build/client_test/SDL2.dll").is_file():
        parser.error("SDL2.dll not found in build/client_test.")
    root = (args.output or ROOT / "build/client_smoke").resolve()
    try:
        with client_lock():
            suites = [run_suite(exe, scenarios, root / exe.stem, args.timeout, args.shots) for exe in executables]
    except RuntimeError as error:
        parser.error(str(error))
    if len(suites) > 1:
        comparisons = compare_results(*suites)
        (root / "comparison.json").write_text(json.dumps(comparisons, indent=2) + "\n", encoding="utf-8")
        (root / executables[0].stem / "summary.json").write_text(json.dumps(suites[0], indent=2) + "\n", encoding="utf-8")
    print(f"Results: {root}", flush=True)
    return 0 if all(result["ok"] for suite in suites for result in suite) else 1


if __name__ == "__main__":
    raise SystemExit(main())