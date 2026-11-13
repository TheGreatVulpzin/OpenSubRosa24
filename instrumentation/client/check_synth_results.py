"""Check saved synthetic comparison results for the selected functions."""
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
from gate import parse  # noqa: E402


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('functions', help='Function names separated by commas.')
    args = parser.parse_args()
    wanted = set(args.functions.split(','))
    totals = {}
    fields = ('same', 'different', 'crashed', 'skipped')
    for path in sorted(args.directory.glob('*/*.result.txt')):
        name = path.name.removesuffix('.result.txt')
        if name not in wanted:
            continue
        functions, summary = parse(path.read_text(encoding='utf-8'))
        if not summary or name not in functions:
            print(f'FAIL {path}: no summary for {name}')
            return 1
        counts = totals.setdefault(name, [0, 0, 0, 0])
        for index, field in enumerate(fields):
            counts[index] += functions[name][field]
    missing = wanted - totals.keys()
    for name, counts in sorted(totals.items()):
        print(f'{name}: ' + ', '.join(f'{count} {field}' for count, field in zip(counts, fields)))
    if missing:
        print('FAIL missing functions: ' + ', '.join(sorted(missing)))
        return 1
    combined = [sum(counts[index] for counts in totals.values()) for index in range(4)]
    print('TOTAL: ' + ', '.join(f'{count} {field}' for count, field in zip(combined, fields)))
    return int(any(not counts[0] or any(counts[1:]) for counts in totals.values()))


if __name__ == '__main__':
    sys.exit(main())
