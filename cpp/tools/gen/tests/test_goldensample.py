"""goldensample (phase6-transliterator.md §7): the parts that need no compiler - the tree's table and held-back list read from GoldenHandlers.h,
the sample's choice, the table header it writes, and the summary of a run (the harness's report lines) - and the harness hooks the tool
relies on (AION_GOLDEN_SAMPLE_TABLE, AION_GOLDEN_EXPECTED_DIR).

Run from cpp/tools/gen: python -m unittest tests.test_goldensample
"""
from __future__ import annotations

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
        fail = 'D:\\GoldenQuestTraceTest.cpp(1400): error: Failed\n1361 onItemUseEvent#3 (setup: own race, minimum level): returned "x", Java "y"\n'
        fail2 = ('D:\\GoldenQuestTraceTest.cpp(1400): error: Failed\n1361 onItemUseEvent#3@questState.vars.0=4 (setup: own race, minimum '
                 'level; overlay: var 0 at the step of changeQuestStep (1)): the quest states afterwards differ\n')
        fail3 = 'D:\\a.cpp(9): error: Failed\n1361 onDialogEvent#2 (setup: other race): the packet opcode sequence differs\n'
        unrep = 'D:\\a.cpp(1): error: Value of: x\nnot reproducible and not listed: 1361 onDialogEvent#9: the replay does not model useQuestItem []\n'
        vac = 'D:\\a.cpp(1): error: Value of: x\nvacuous and not listed: 1361 onDialogEvent#4\n'
        stopped = 'unknown file: error: C++ exception with description "The given level is higher than possible max" thrown in the test body.\n'
        s = goldensample.classify(self.report({1361: [fail, fail2, fail3, unrep, vac], 2900: [stopped]}), stdout)
        self.assertEqual(s['failed'], {'1361': ['onDialogEvent#2', 'onItemUseEvent#3', 'onItemUseEvent#3@questState.vars.0=4']})
        self.assertEqual(s['failedByHook'], {'onItemUseEvent': {'variants': 2, 'quests': [1361]},
                                             'onDialogEvent': {'variants': 1, 'quests': [1361]}})
        self.assertEqual(s['notReproducible'], {'1361': ['onDialogEvent#9: the replay does not model useQuestItem []']})
        self.assertEqual(s['vacuous'], {'1361': ['onDialogEvent#4']})
        self.assertEqual(list(s['stopped']), ['2900'])
        self.assertIn('higher than possible max', s['stopped']['2900'])
        self.assertEqual((s['ran'], s['variantsPassed'], s['runsCompared'], s['questsWithoutFinding']), (2, 13, 49, 1))
        self.assertEqual(len(s['registration']), 1)


if __name__ == '__main__':
    unittest.main()
