"""Print printable ASCII strings from a binary (like binutils `strings`).

Usage: python -I tools/re/strings.py <file> [min_len=5]
"""
import re
import sys

data = open(sys.argv[1], 'rb').read()
min_len = int(sys.argv[2]) if len(sys.argv) > 2 else 5
for m in re.finditer(rb'[\x20-\x7e]{%d,}' % min_len, data):
    print(m.group().decode())
