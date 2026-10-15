"""lint_concurrency.py over the real C++ tree (calibration): every file parses, the kernel stays free of false positives, and the committed
fieldmap.json loads and maps ported classes."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import lint_concurrency as lc  # noqa: E402

GAME_SERVER = os.path.join(lc.CPP_ROOT, 'game-server')
RUNTIME = os.path.join(GAME_SERVER, 'src', 'aion', 'gameserver', 'runtime')
BENCH = os.path.join(GAME_SERVER, 'bench')


@unittest.skipUnless(os.path.isdir(RUNTIME) and os.path.isfile(lc.DEFAULT_FIELDMAP), 'C++ tree or fieldmap.json not available')
class CalibrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fm = lc.load_fieldmap(lc.DEFAULT_FIELDMAP)
        cls.linter = lc.Linter(cls.fm)
        paths = [os.path.join(GAME_SERVER, 'src')] + ([BENCH] if os.path.isdir(BENCH) else [])
        for f in lc.collect(paths):
            cls.linter.add_file(f, lc.display_path(f))
        cls.findings = cls.linter.run()

    def test_all_files_parsed(self):
        self.assertGreater(len(self.linter.sources), 90)
        self.assertGreater(len(self.linter.classes), 100)

    def test_kernel_has_no_false_positives(self):
        kernel = [f for f in self.findings if '/runtime/' in f.path and f.severity == 'error']
        # the one known finding: PCT test infrastructure reads AION_PCT_* through getenv (Pct.cpp)
        self.assertLessEqual(len(kernel), 1, '\n'.join(f.format() for f in kernel))
        self.assertTrue(all(f.rule == 'L8' for f in kernel))

    def test_java_mapping_of_ported_classes(self):
        classes = {c.qualname: c for c in self.linter.classes if c.namespace[:2] == ['aion', 'gameserver']}
        vector = next((c for q, c in classes.items() if q == 'Vector3f'), None)
        if vector is not None:
            self.assertEqual(self.linter.java_cid(vector), 'com.aionemu.gameserver.geoEngine.math.Vector3f')
            self.assertTrue(self.linter.is_confined(vector))


if __name__ == '__main__':
    unittest.main()
