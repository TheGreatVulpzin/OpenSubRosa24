"""Run selected synthetic cases against the saved pre-migration harness."""
import argparse
import os
import runpy
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('suite', choices=('sys', 'city_objects'))
    parser.add_argument('recording', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('cases', nargs='?')
    args = parser.parse_args()
    prior = ROOT / 'build/c-migration/harness-before.exe'
    real_run = subprocess.run

    def run(command, *pos, **kwargs):
        if Path(command[0]).name.lower() == 'harness.exe':
            command = [str(prior), *command[1:]]
        return real_run(command, *pos, **kwargs)

    subprocess.run = run
    os.environ['SV_LOG_FILE'] = str(args.work.resolve() / 'baseline-verify.log')
    args.work.mkdir(parents=True, exist_ok=True)
    sys.argv = [f'synth_{args.suite}.py', '--run', str(args.recording), str(args.work)]
    if args.cases:
        sys.argv.append(args.cases)
    runpy.run_path(str(ROOT / f'instrumentation/lift/synth_{args.suite}.py'), run_name='__main__')


if __name__ == '__main__':
    main()
