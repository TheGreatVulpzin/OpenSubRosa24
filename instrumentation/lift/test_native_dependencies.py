"""Check that the normal target has no transition-runtime dependency."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class NativeDependencies(unittest.TestCase):
    def test_normal_target(self):
        build = ROOT / 'build/hook'
        reply = build / '.cmake/api/v1/reply'
        indexes = list(reply.glob('index-*.json'))
        self.assertTrue(indexes, 'Run tools/build_hook.bat to generate the CMake dependency model')
        index = json.loads(max(indexes, key=lambda p: p.stat().st_mtime).read_text())
        model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
        targets = model['configurations'][0]['targets']
        ref = next(t for t in targets if t['name'] == 'subvulpes-server')
        target = json.loads((reply / ref['jsonFile']).read_text())
        sources = [s['path'] for s in target['sources']]
        self.assertIn('src/server/storage.c', sources)
        forbidden = re.compile(r'instrumentation|lifted|image_024d|fn_table')
        for path in sources:
            self.assertIsNone(forbidden.search(path), path)
        for group in target['compileGroups']:
            for command in group.get('compileCommandFragments', []):
                self.assertNotIn('/Gh', command['fragment'])
            for entry in group.get('includes', []):
                self.assertIsNone(forbidden.search(entry['path']), entry['path'])
        files = [ROOT / p for p in sources if p.endswith('.c')]
        files += list((ROOT / 'src/common').glob('*.h'))
        files += list((ROOT / 'src/game').glob('*.h'))
        files += list((ROOT / 'src/net').glob('*.h'))
        files += [ROOT / f'src/include/{name}' for name in
                  ('sr_runtime.h', 'sr_storage.h', 'sr_storage_types.h', 'sr_limits.h')]
        old_access = re.compile(r'\b(?:CODE|sr_fn)\s*\(|\b(?:sr_base|SR_AT|SR_VA|SR_RVA_\w*)\b')
        for path in files:
            code = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(encoding='utf-8'), flags=re.S)
            self.assertIsNone(old_access.search(code), str(path.relative_to(ROOT)))
        symbols = (build / 'opensubrosa24_server.map').read_text(encoding='utf-8', errors='replace')
        self.assertIsNone(re.search(r'\b_(?:sr_base|sr_fn(?:_init)?|sr_image_\w+|lf_table(?:_count)?|_penter)\b', symbols))


if __name__ == '__main__':
    unittest.main()
