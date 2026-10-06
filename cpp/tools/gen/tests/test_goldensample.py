"""goldensample (phase6-transliterator.md §7): the parts that need no compiler - the tree's table and held-back list read from GoldenHandlers.h,
the sample's choice, the table header it writes, and the summary of a run (the harness's report lines) - and the harness hooks the tool
relies on (AION_GOLDEN_SAMPLE_TABLE, AION_GOLDEN_EXPECTED_DIR).

Run from cpp/tools/gen: python -m unittest tests.test_goldensample
"""
from __future__ import annotations

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.dont_write_bytecode = True

from questgen import goldensample  # noqa: E402

HANDLERS_H = goldensample.GOLDEN / 'GoldenHandlers.h'


class TreeTableTest(unittest.TestCase):
    def test_the_tree_table_and_held_back_list(self):
        text = HANDLERS_H.read_text(encoding='utf-8')
        rows = goldensample.table_rows(text, 'AION_GOLDEN_GENERATED_HANDLERS')
        self.assertIn(('poeta', '_1111InsomniaMedicine', 1111), rows)
        self.assertEqual(len(rows), len({q for _d, _c, q in rows}))
        # the documents of the tree are the table plus the held-back quests (EveryExpectedDocumentHasAGeneratedHandlerAndEveryHandlerADocument)
        docs = {int(f.stem) for f in goldensample.COMMITTED_DOCS.glob('*.json')}
        held = goldensample.held_back(text)
        self.assertIn(4973, held)
        self.assertEqual({q for _d, _c, q in rows} | held, docs)

    def test_the_harness_takes_a_sample_table_and_an_expected_directory(self):
        self.assertIn('#include AION_GOLDEN_SAMPLE_TABLE', HANDLERS_H.read_text(encoding='utf-8'))
        test = (goldensample.GOLDEN / 'GoldenQuestTraceTest.cpp').read_text(encoding='utf-8')
        self.assertIn('fs::path(AION_GOLDEN_EXPECTED_DIR)', test)


class SampleTest(unittest.TestCase):
    def test_the_sample_leaves_out_the_tree_and_files_without_a_case(self):
        tmp = Path(tempfile.mkdtemp())
        (tmp / 'eltnen').mkdir()
        (tmp / 'eltnen' / '_1114TheNymphsGown.cpp').write_text('', encoding='utf-8')   # a file of the tree (a hand port, say)
        docs = {
            'eltnen/_1361FindingDrinkingWater.java': {'questId': 1361, 'cases': [{}]},
            'eltnen/_1114TheNymphsGown.java': {'questId': 1114, 'cases': [{}]},
            'poeta/_1111InsomniaMedicine.java': {'questId': 1111, 'cases': [{}]},     # in the tree's table
            'altgard/_2207ConversingWithaSkurv.java': {'questId': 2207, 'cases': [{}]},  # held back
            'cygnea/_10506MindOverMatter.java': {'questId': 10506, 'cases': []},         # no case
            'abyss_entry/_1044TestingFlightSkills.java': {'questId': 1044, 'cases': [{}]},
        }
        got = goldensample.pick_sample(docs, [('poeta', '_1111InsomniaMedicine', 1111)], {2207}, tree_quest=tmp)
        self.assertEqual(got, [('abyss_entry/_1044TestingFlightSkills.java', 1044), ('eltnen/_1361FindingDrinkingWater.java', 1361)])
        table = goldensample.sample_table(got)
        self.assertIn('#define AION_GOLDEN_SAMPLE_HANDLERS(X) \\\n\tX(abyss_entry, _1044TestingFlightSkills, 1044) \\\n'
                      '\tX(eltnen, _1361FindingDrinkingWater, 1361)\n', table)


class SummaryTest(unittest.TestCase):
    def report(self, tests):
        return json.dumps({'testsuites': [
            {'name': 'Generated/GoldenQuestCases', 'testsuite': [{'name': f'EveryCaseMatchesTheJavaTrace/Quest{q}', 'failures': [
                {'failure': f} for f in fs]} for q, fs in tests.items()]},
            {'name': 'GoldenQuestTraceTest', 'testsuite': [{'name': 'RegistrationTraceMatchesJavaRegister', 'failures': [
                {'failure': 'D:\\x.cpp(1)\nnpc 203001 addOnTalkEvent\n  Expected: 1\nGoogle Test trace:\n_1361FindingDrinkingWater'}]}]}]})

    def test_the_report_lines_are_counted_by_variant_and_hook(self):
        stdout = ('[golden] quest 1361: 10 passed, 2 failed, 1 not reproducible of 12 cases and their high ends; 40 runs compared, 0 overlay '
                  'runs not reproducible, 1 vacuous\n'
                  '[golden] quest 3718: 3 passed, 0 failed, 0 not reproducible of 3 cases and their high ends; 9 runs compared, 0 overlay runs '
                  'not reproducible, 0 vacuous\n')
        fail = 'D:\\GoldenQuestTraceTest.cpp(1400): error: Failed\n1361 onItemUseEvent#3 (setup: own race, minimum level): [returned] returned "x", Java "y"\n'
        fail2 = ('D:\\GoldenQuestTraceTest.cpp(1400): error: Failed\n1361 onItemUseEvent#3@questState.vars.0=4 (setup: own race, minimum '
                 'level; overlay: var 0 at the step of changeQuestStep (1)): [questStates] the quest states afterwards differ\n')
        fail3 = 'D:\\a.cpp(9): error: Failed\n1361 onDialogEvent#2 (setup: other race): [opcodes] the packet opcode sequence differs\n'
        unrep = 'D:\\a.cpp(1): error: Value of: x\nnot reproducible and not listed: 1361 onDialogEvent#9: the replay does not model useQuestItem []\n'
        vac = 'D:\\a.cpp(1): error: Value of: x\nvacuous and not listed: 1361 onDialogEvent#4\n'
        stopped = 'unknown file: error: C++ exception with description "The given level is higher than possible max" thrown in the test body.\n'
        summary = 'D:\\GoldenQuestTraceTest.cpp(1889): error: Expected equality of these values:\n  tally.failed\n    Which is: 3\n  0\n'
        s = goldensample.classify(self.report({1361: [fail, fail2, fail3, unrep, vac, summary], 2900: [stopped]}), stdout)
        self.assertEqual(s['failed'], {'1361': ['onDialogEvent#2', 'onItemUseEvent#3', 'onItemUseEvent#3@questState.vars.0=4']})
        self.assertEqual(s['failedByHook'], {'onItemUseEvent': {'variants': 2, 'quests': [1361]},
                                             'onDialogEvent': {'variants': 1, 'quests': [1361]}})
        self.assertEqual(s['notReproducible'], {'1361': ['onDialogEvent#9: the replay does not model useQuestItem []']})
        self.assertEqual(s['vacuous'], {'1361': ['onDialogEvent#4']})
        self.assertEqual(list(s['stopped']), ['2900'])
        self.assertIn('higher than possible max', s['stopped']['2900'])
        self.assertEqual((s['ran'], s['variantsPassed'], s['runsCompared'], s['questsWithoutFinding']), (2, 13, 49, 1))
        self.assertEqual(len(s['registration']), 1)
        self.assertEqual(s['other'], {})       # the quest's EXPECT_EQ(tally.failed, 0) counts its failed variants again: not an other failure
        # the review of #79, item 9: each check by its name
        self.assertEqual(s['failedChecks'], {'1361': {'returned': 1, 'questStates': 1, 'opcodes': 1}})
        s['exitCode'] = 1
        self.assertEqual(goldensample.problems(s)[:2], ['exit code 1', '1 quests with a failed variant'])


class CrashTest(unittest.TestCase):
    """the review of #79, item 4: a crashed or failed run is never a clean one"""

    STDOUT = ('[ RUN      ] Generated/GoldenQuestCases.EveryCaseMatchesTheJavaTrace/Quest1001\n'
              '[       OK ] Generated/GoldenQuestCases.EveryCaseMatchesTheJavaTrace/Quest1001 (10 ms)\n'
              '[ RUN      ] Generated/GoldenQuestCases.EveryCaseMatchesTheJavaTrace/Quest1003\n')

    def test_a_crash_without_a_report_names_the_running_test(self):
        crash = goldensample.crash_of('', self.STDOUT, 3)
        self.assertEqual(crash, 'no gtest report, exit code 3 (running: Generated/GoldenQuestCases.EveryCaseMatchesTheJavaTrace/Quest1003)')
        s = goldensample.classify('', self.STDOUT)
        s.update(exitCode=3, crash=crash)
        self.assertEqual(goldensample.problems(s), ['the run crashed: ' + crash])

    def test_a_partial_or_broken_report_and_an_exit_code_without_failures(self):
        self.assertIn('a partial gtest report (1 of 2 tests)',
                      goldensample.crash_of(json.dumps({'tests': 1, 'failures': 0, 'testsuites': []}), self.STDOUT, 0))
        self.assertIn('not JSON', goldensample.crash_of('{"tests": 2, "testsu', self.STDOUT, 3))
        done = self.STDOUT + '[  FAILED  ] Generated/GoldenQuestCases.EveryCaseMatchesTheJavaTrace/Quest1003 (1 ms)\n'
        self.assertEqual(goldensample.crash_of(json.dumps({'tests': 2, 'failures': 1, 'testsuites': []}), done, 1), '')
        self.assertIn('exit code 5 with no failure',
                      goldensample.crash_of(json.dumps({'tests': 2, 'failures': 0, 'testsuites': []}), done, 5))

    def test_an_seh_exception_is_an_other_failure(self):
        seh = 'unknown file: error: SEH exception with code 0xc0000005 thrown in the test body.\n'
        rep = json.dumps({'testsuites': [{'name': 'Generated/GoldenQuestCases', 'testsuite': [
            {'name': 'EveryCaseMatchesTheJavaTrace/Quest1001', 'failures': [{'failure': seh}]}]}]})
        s = goldensample.classify(rep, '')
        self.assertEqual(list(s['other']), ['Quest1001'])
        self.assertIn('1 other failures', goldensample.problems(s))


class StageTest(unittest.TestCase):
    """the review of #79, item 7: a directory that holds other files is no stage without --force"""

    def test_only_a_new_empty_or_marked_directory_is_a_stage(self):
        tmp = Path(tempfile.mkdtemp())
        self.assertTrue(goldensample.stage_is_ours(tmp / 'new'))
        self.assertTrue(goldensample.stage_is_ours(tmp))
        (tmp / 'src').mkdir()
        self.assertFalse(goldensample.stage_is_ours(tmp))
        (tmp / goldensample.STAGE_MARKER).write_text('', encoding='utf-8')
        self.assertTrue(goldensample.stage_is_ours(tmp))
        other = Path(tempfile.mkdtemp())
        (other / 'keep.txt').write_text('mine', encoding='utf-8')
        (other / 'src').mkdir()
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            goldensample.main(['--stage', str(other), '--no-run'])
        self.assertTrue((other / 'keep.txt').is_file() and (other / 'src').is_dir())


if __name__ == '__main__':
    unittest.main()
