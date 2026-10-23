import copy
import unittest

from gate import compare, rename_baseline


class BaselineRenameTests(unittest.TestCase):
    def setUp(self):
        self.counts = dict(same=48, different=0, crashed=0, skipped=2, note='')
        self.baseline = {'recording': {'total': {'functions': 2, 'same': 51},
                                     'funcs': {'old': self.counts, 'other': dict(self.counts, same=3)}}}

    def test_rename_keeps_counts_and_other_functions(self):
        before = copy.deepcopy(self.baseline)
        result = rename_baseline(self.baseline, ['old:new'])
        self.assertEqual(result['recording']['funcs']['new'], self.counts)
        self.assertEqual(result['recording']['funcs']['other'], before['recording']['funcs']['other'])
        self.assertEqual(result['recording']['total'], before['recording']['total'])
        self.assertEqual(self.baseline, before)
        self.assertNotIn('old', result['recording']['funcs'])

    def test_count_drop_still_fails(self):
        result = rename_baseline(self.baseline, ['old:new'])
        observed = dict(result['recording']['funcs'])
        observed['new'] = dict(self.counts, same=47)
        self.assertTrue(compare('recording', observed, result['recording']['funcs'], None))

    def test_new_difference_still_fails(self):
        result = rename_baseline(self.baseline, ['old:new'])
        observed = dict(result['recording']['funcs'])
        observed['new'] = dict(self.counts, different=1)
        self.assertTrue(compare('recording', observed, result['recording']['funcs'], None))

    def test_collision_and_unknown_name_fail(self):
        for pair in ('old:other', 'missing:new', 'old:old', 'old:new:extra'):
            with self.subTest(pair=pair), self.assertRaises(ValueError):
                rename_baseline(self.baseline, [pair])


if __name__ == '__main__':
    unittest.main()
