"""Run the native and image storage cases and require identical state digests."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/hook')
    args = parser.parse_args()
    out = ROOT / 'build/c-migration'
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for mode in ('native', 'image'):
        with tempfile.TemporaryDirectory(prefix=f'storage-{mode}-', dir=out) as folder:
            run = subprocess.run([str(args.build_dir.resolve() / f'test_storage_{mode}.exe')], cwd=folder,
                                 capture_output=True, text=True, errors='replace', timeout=120)
        (out / f'{mode}-storage.txt').write_text(run.stdout + run.stderr, encoding='utf-8')
        if run.returncode:
            print(f'{mode}: FAILED ({run.returncode}), see {out / (mode + "-storage.txt")}')
            return 1
        results.append(run.stdout)
    if results[0] != results[1]:
        native = results[0].splitlines()
        image = results[1].splitlines()
        for a, b in zip(native, image):
            if a != b:
                print(f'FAILED: native {a}\n         image  {b}')
                break
        return 1
    match = re.search(r'^initializers: (\d+) storage objects match$', results[0], re.M)
    count = int(match.group(1)) if match else 0
    for stage in ('tables', 'entities'):
        if not count or len(re.findall(r'^' + stage + r' \w+ [0-9a-f]{16}$', results[0], re.M)) != count:
            print(f'FAILED: missing {stage} storage digests')
            return 1
    if ('boundaries: 9 cases passed\n' not in results[0] or
            'client accounts: 65536 records saved and loaded\n' not in results[0] or
            not results[0].endswith('storage tests passed\n')):
        print('FAILED: boundary tests did not finish')
        return 1
    print(f'OK: {count} initial storage objects, {count * 2} table/entity digests, both memory modes agree')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
