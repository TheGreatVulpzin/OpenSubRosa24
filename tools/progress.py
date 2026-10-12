"""Progresso da Fase 3: quanto do 024d já roda em C.

Lê a tabela g_hooks em hook/hooks_024d.c e os tamanhos em analysis/024d/functions.csv.
Uma função conta como "verificada" quando docs/hooks.md marca **Verificada** na linha dela.

Uso: python -I tools/progress.py [--md]
  --md  imprime a tabela em Markdown (para docs/progresso.md)
"""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    md = '--md' in sys.argv
    hooks_src = (ROOT / 'hook/hooks_024d.c').read_text(encoding='utf-8')
    hooked = dict(re.findall(r'\{\s*"(\w+)",\s*(0x[0-9a-fA-F]+),', hooks_src))
    doc = (ROOT / 'docs/hooks.md').read_text(encoding='utf-8')
    verified = set(re.findall(r'^\| `(\w+)` \|[^\n]*\*\*Verificada\*\*', doc, re.M))

    with open(ROOT / 'analysis/024d/functions.csv', encoding='utf-8') as fh:
        rows = list(csv.DictReader(fh))
    size = {int(r['rva'], 16): int(r['size']) for r in rows}
    total_funcs, total_bytes = len(rows), sum(size.values())

    hooked_bytes = sum(size.get(int(rva, 16), 0) for rva in hooked.values())
    verified_bytes = sum(size.get(int(hooked[n], 16), 0) for n in verified if n in hooked)

    lines = [
        ('Funções em C (hook instalado)', len(hooked), total_funcs, hooked_bytes, total_bytes),
        ('Funções verificadas no jogo', len(verified & set(hooked)), total_funcs, verified_bytes, total_bytes),
    ]
    if md:
        print('| Medida | Funções | Bytes de código |')
        print('| --- | --- | --- |')
        for label, n, tn, b, tb in lines:
            print(f'| {label} | {n} de {tn} ({100 * n / tn:.1f}%) | {b} de {tb} ({100 * b / tb:.2f}%) |')
    else:
        for label, n, tn, b, tb in lines:
            print(f'{label:32} {n:4}/{tn} funções ({100 * n / tn:5.1f}%)   {b:7}/{tb} bytes ({100 * b / tb:5.2f}%)')


if __name__ == '__main__':
    main()
