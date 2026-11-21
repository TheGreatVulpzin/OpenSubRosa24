"""Run window and editor smoke scenarios in separate client processes."""
import argparse
from collections import Counter
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = {"crash": 360, "menus": 360, "buildings": 1800, "level": 360, "model": 300}
READY_SECONDS = 15


def last_error(text):
    errors = [line.strip() for line in text.splitlines() if line.startswith(
        ("SMOKE FAIL ", "SMOKE TIMEOUT ", "exception ", "no C function ", "import "))]
    if not errors:
        return "no error line"
    modules = [line.removeprefix("exception module: ") for line in errors if line.startswith("exception module: ")]
    last = errors[-1]
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
    paths = {folder / "config.txt", folder / "Sub Rosa/config.txt", folder / "Sub Rosa/client.srk", folder / "data/city.sbc"}
    paths.update((folder / "data/building").glob("*.sbb"))
    saved = {path: path.read_bytes() if path.exists() else None for path in paths}
    try:
        yield
    finally:
        for path, data in saved.items():
            if data is None:
                path.unlink(missing_ok=True)
            elif not path.exists() or path.read_bytes() != data:
                path.write_bytes(data)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("scenarios", nargs="*", metavar="scenario")
    parser.add_argument("--timeout", type=float, help="Process limit in seconds for each scenario.")
    parser.add_argument("--output", type=Path, help="Folder for logs and summary.json.")
    args = parser.parse_args(argv)
    scenarios = args.scenarios or list(SCENARIOS)
    if any(name not in SCENARIOS for name in scenarios):
        parser.error("Choose menus, buildings, level or model.")
    if args.timeout is not None and (not math.isfinite(args.timeout) or args.timeout <= 0):
        parser.error("The timeout must be positive.")
    exe = args.exe.resolve()
    if not exe.is_file():
        parser.error(f"File not found: {exe}")
    folder = ROOT / "build/client_test"
    if not (folder / "SDL2.dll").is_file():
        parser.error(f"SDL2.dll not found in {folder}")
    output = (args.output or ROOT / "build/client_smoke" / exe.stem).resolve()
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.pop("SR_SAMPLE", None)
    env.pop("SR_DENORM", None)
    env["SR_TRACE"] = os.environ.get("SR_TRACE", "3")
    env["SR_DEBUGIT"] = "1"
    env["PATH"] = str(ROOT / "build/client_deps") + os.pathsep + env.get("PATH", "")
    driver_present = b"SR_SMOKE" in exe.read_bytes()
    results = []
    stalled = Counter()
    for index, scenario in enumerate(scenarios):
        env["SR_SMOKE"] = scenario
        log = output / f"{index + 1:02d}-{scenario}.log"
        if not driver_present:
            # An older client cannot isolate its settings or accept test input.
            reason = "SR_SMOKE unavailable: executable has no smoke driver"
            log.write_text(reason + "\n", encoding="utf-8")
            result = {"scenario": scenario, "ok": False, "error": reason, "stage": "preflight",
                      "timeout": False, "seconds": 0, "exit_code": None, "log": str(log)}
        else:
            with preserve_inputs(folder):
                result = run_process([str(exe), "--run"], scenario, folder, env, log,
                                     args.timeout or SCENARIOS[scenario])
        results.append(result)
        print(f"{scenario}: {'ok' if result['ok'] else 'FAILED'} | {result['error'] or '-'}", flush=True)
        if result["timeout"]:
            stalled[result["stage"]] += 1
            if stalled[result["stage"]] >= 2:
                for remaining in scenarios[index + 1:]:
                    results.append({"scenario": remaining, "ok": False, "error": "not run: same stage failed twice"})
                    print(f"{remaining}: NOT RUN | same stage failed twice", flush=True)
                break
    (output / "summary.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(f"Summary: {output / 'summary.json'}", flush=True)
    return 0 if all(result["ok"] for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
