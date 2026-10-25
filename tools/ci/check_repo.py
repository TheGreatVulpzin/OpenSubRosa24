"""Check that the repository holds no proprietary or private files.

Rules (see CLAUDE.md):
- original/ holds only README.md and MANIFEST.sha256. The game files are proprietary.
- No config.txt: the server config files have passwords.
- No build/ and no Ghidra project: they are local and large, and build/recordings has
  player names and chat.
- No game data or executables, except the plugin DLLs of the friend's server manager.

Usage: python -I tools/ci/check_repo.py    (exit code 1 when a rule fails)
"""
import re
import subprocess
import sys

ALLOWED_ORIGINAL = {'original/README.md', 'original/MANIFEST.sha256'}
BINARY = re.compile(r'\.(exe|dll|so|dylib|bin|srk|sbl|sbb|sbc|csm|pak|ogg|wav|gbf|gpr)$', re.I)
ALLOWED_BINARY = re.compile(r'^third_party/server-manager/plugins/native/[^/]+\.dll$')
LEGACY_INSTRUMENTATION = ('hook/', 'src/tests/', 'tools/ghidra/', 'tools/recording/')


def main():
    files = subprocess.run(['git', 'ls-files'], capture_output=True, text=True, check=True).stdout.split('\n')
    bad = []
    for f in filter(None, files):
        if f.startswith('original/') and f not in ALLOWED_ORIGINAL:
            bad.append((f, 'proprietary game file in original/'))
        elif f.split('/')[-1].lower() == 'config.txt':
            bad.append((f, 'server config (passwords)'))
        elif f.startswith(('build/', 'ghidra-project/')) or '.rep/' in f:
            bad.append((f, 'local build or Ghidra project'))
        elif (f.startswith(LEGACY_INSTRUMENTATION)
              or (f.startswith('tools/lift/') and f != 'tools/lift/pipeline.sh')
              or f in ('src/platform/plat_test.c', 'src/platform/plat_test.h',
                       'src/standalone/record_shim.c')):
            bad.append((f, 'verification source outside instrumentation/'))
        elif BINARY.search(f) and not ALLOWED_BINARY.match(f):
            bad.append((f, 'binary or game data'))
    for f, why in bad:
        print(f'{f}: {why}')
    print(f'{len(files) - 1} files checked, {len(bad)} problem(s)')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
