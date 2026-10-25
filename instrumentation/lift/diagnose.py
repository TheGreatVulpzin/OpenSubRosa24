"""Compile each function that failed in the last lift alone and print its errors with the source line.

Usage: python -I instrumentation/lift/diagnose.py [rva ...]   (default: every compile_error in build/lift/report.csv)
"""
import csv
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lift  # noqa: E402

ROOT = lift.ROOT


def main():
    if len(sys.argv) > 1:
        rvas = [int(x, 16) for x in sys.argv[1:]]
    else:
        with open(ROOT / 'build/lift/report.csv', encoding='utf-8') as fh:
            rvas = [int(r['rva'], 16) for r in csv.DictReader(fh) if r['status'] == 'compile_error']
    funcs = {}
    with open(ROOT / 'build/lift/ded024d.jsonl', encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            funcs[int(f['rva'], 16)] = f
    headers = {rva: h for rva, f in funcs.items() if (h := lift.parse_header(f['c']))}
    out = ROOT / 'build/lift/diagnose.c'
    for rva in rvas:
        imports_used = {}
        text, err = lift.lift_function(funcs[rva], rva, headers, imports_used)
        if err:
            print(f'== 0x{rva:x}: not lifted ({err})')
            continue
        failed = {}
        status = {}
        left = lift.compile_loop([(rva, text)], imports_used, status, out, verbose=False, failed=failed)
        if left:
            print(f'== 0x{rva:x}: compiles after the automatic fixes')
            continue
        code, _ = lift.gen_lifted([(rva, failed[rva])], imports_used)
        out.write_text(code, encoding='utf-8', newline=chr(10))
        errs = lift.compile_errors(out)
        lines = code.splitlines()
        print(f'== 0x{rva:x}: {len(errs)} error(s)')
        for ln, msg in list(errs.items())[:4]:
            print(f'   {msg[:110]}')
            print(f'   > {lines[ln - 1].strip()[:200]}')


if __name__ == '__main__':
    main()
