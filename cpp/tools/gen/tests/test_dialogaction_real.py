"""dialogaction.py over the real Java tree: counts, an independent scan, and the drift check of the committed outputs."""
import os
import re
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import dialogaction  # noqa: E402


class RealTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.path.isdir(dialogaction.DEFAULT_JAVA_DIR):
            raise AssertionError(f'Java game-server directory missing: {dialogaction.DEFAULT_JAVA_DIR}')
        cls.outputs, cls.summary = dialogaction.generate(dialogaction.DEFAULT_JAVA_DIR)

    def test_committed_outputs_are_up_to_date(self):
        differ = dialogaction.write_outputs(self.outputs, dialogaction.DEFAULT_OUT_DIR, check=True)
        self.assertEqual(differ, [], 'regenerate with: python cpp/tools/gen/dialogaction.py')

    def test_counts(self):
        self.assertEqual(self.summary['constants'], 6205)
        self.assertEqual(self.summary['min_id'], -1)
        self.assertEqual(self.summary['max_id'], 100001)
        self.assertEqual(self.summary['renamed'], ['NULL -> NULL_'])

    def test_matches_an_independent_regex_scan(self):
        with open(os.path.join(dialogaction.DEFAULT_JAVA_DIR, dialogaction.DIALOG_ACTION_JAVA), encoding='utf-8') as f:
            java = re.findall(r'^\tpublic static final int (\w+) = (-?\d+);', f.read(), re.M)
        header = self.outputs[dialogaction.HEADER_OUT]
        generated = re.findall(r'^inline constexpr int32_t (\w+) = (-?\d+);', header, re.M)
        self.assertEqual([(dialogaction.cpp_identifier(n), int(v)) for n, v in java], [(n, int(v)) for n, v in generated])
        table = re.findall(r'^\t\{(\w+), "(\w+)"\},$', self.outputs[dialogaction.SOURCE_OUT], re.M)
        values = {n: int(v) for n, v in java}
        self.assertEqual(sorted(table, key=lambda e: values[e[1]]), table)
        self.assertEqual(sorted((dialogaction.cpp_identifier(n), n) for n, _ in java), sorted(table))


if __name__ == '__main__':
    unittest.main()
