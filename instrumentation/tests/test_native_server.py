"""Run both server builds in isolated folders and compare their idle tick rates."""
import argparse
import ctypes
import os
import re
import shutil
import subprocess
import uuid
from ctypes import wintypes
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def run(executable, source, seconds, label, recording=False):
    folder = ROOT / 'build/c-migration' / f'smoke-{label}-{uuid.uuid4().hex}'
    folder.mkdir(parents=True)
    shutil.copytree(source, folder / 'data')
    (folder / 'user').mkdir()
    (folder / 'config.txt').write_text('servername=Storage test\nport=31847\ngametype=3\n'
                                     'maxnumofplayers=8\nroundtime=1\n', encoding='ascii')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateEventW.argtypes = [ctypes.c_void_p, wintypes.BOOL, wintypes.BOOL, wintypes.LPCWSTR]
    kernel.CreateEventW.restype = wintypes.HANDLE
    kernel.SetEvent.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    name = 'Local\\OpenSubRosaStorage-' + uuid.uuid4().hex
    event = kernel.CreateEventW(None, True, False, name)
    if not event:
        raise ctypes.WinError(ctypes.get_last_error())
    env = dict(os.environ, SV_STOP_EVENT=name, SV_LOG=str(folder / 'server.log'))
    if recording:
        env.update(SV_RECORD_DIR=str(folder), SV_SNAPSHOT_AFTER='2')
    try:
        with (folder / 'console.log').open('w', encoding='utf-8') as log:
            process = subprocess.Popen([str(executable.resolve())], cwd=folder, env=env,
                                       stdout=log, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:
                kernel.SetEvent(event)
                try:
                    process.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                    raise RuntimeError(f'{label}: server did not stop')
    finally:
        kernel.CloseHandle(event)
    text = (folder / 'console.log').read_text(encoding='utf-8', errors='replace')
    rates = re.findall(r'Server status: ([\d.]+) ticks/s', text)
    if process.returncode or 'Server started on port' not in text or 'CRASH:' in text:
        raise RuntimeError(f'{label}: startup/status failed, see {folder}')
    if recording:
        snapshot = folder / 'sv_snapshot.bin'
        calls = folder / 'sv_args.txt'
        if not snapshot.exists() or not calls.exists() or 'R ' not in calls.read_text(errors='replace'):
            raise RuntimeError(f'{label}: recording is incomplete, see {folder}')
        print(f'{label}: clean start/stop, snapshot and calls saved in {folder}', flush=True)
        return 1.0
    if not rates:
        raise RuntimeError(f'{label}: no tick status, see {folder}')
    rate = sum(map(float, rates)) / len(rates)
    print(f'{label}: clean start/stop, {rate:.1f} ticks/s, logs {folder}', flush=True)
    return rate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--before', type=Path)
    parser.add_argument('--after', type=Path, default=ROOT / 'build/hook/opensubrosa24_server.exe')
    parser.add_argument('--data', type=Path, default=ROOT / 'build/instrumentation-server-smoke/data')
    parser.add_argument('--seconds', type=int, default=75)
    parser.add_argument('--record-only', action='store_true')
    parser.add_argument('--record-exe', type=Path, default=ROOT / 'build/hook/opensubrosa24_server_record.exe')
    args = parser.parse_args()
    if args.record_only:
        run(args.record_exe, args.data, args.seconds, 'record', True)
        return 0
    if args.before is None:
        parser.error('--before is required for the timing comparison')
    before = run(args.before, args.data, args.seconds, 'before')
    after = run(args.after, args.data, args.seconds, 'after')
    print(f'idle tick rate after/before: {after / before:.3f}')
    return int(after < before * 0.95)


if __name__ == '__main__':
    raise SystemExit(main())
