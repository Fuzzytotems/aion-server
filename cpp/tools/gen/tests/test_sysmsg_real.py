"""sysmsg.py over the real Java tree: counts, the hand-ported list, an independent regex scan, and the drift check of the committed outputs."""
import os
import re
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import sysmsg  # noqa: E402
from dialogaction import cpp_identifier  # noqa: E402

CPP_TYPES = {'String': 'std::string_view', 'int': 'int32_t', 'long': 'int64_t', 'byte': 'int8_t', 'float': 'float'}


class RealTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.path.isdir(sysmsg.DEFAULT_JAVA_DIR):
            raise AssertionError(f'Java game-server directory missing: {sysmsg.DEFAULT_JAVA_DIR}')
        cls.outputs, cls.summary, cls.report = sysmsg.generate(sysmsg.DEFAULT_JAVA_DIR)

    def test_committed_outputs_are_up_to_date(self):
        differ = sysmsg.write_outputs(self.outputs, sysmsg.DEFAULT_OUT_DIR, check=True)
        self.assertEqual(differ, [], 'regenerate with: python cpp/tools/gen/sysmsg.py')

    def test_counts(self):
        self.assertEqual(self.summary['factories'], 4120)
        self.assertEqual(self.summary['distinct_names'], 4115)
        self.assertEqual(self.summary['plain'] + len(self.summary['generated_non_plain']) + len(self.summary['hand_ported']), 4120)
        self.assertEqual(self.summary['hand_ported'], ['STR_ABYSS_ORDER_RANKER_DIE(Player victim)',
                                                       'STR_ABYSS_ORDER_RANKER_DIE(Player victim, String zoneName)',
                                                       'STR_SKILL_ABYSS_SKILL_IS_FIRED(Player player, String skill)'])
        self.assertEqual(self.summary['parameter_types'], {'Player': 3, 'String': 3134, 'byte': 13, 'float': 3, 'int': 451, 'long': 33})

    def test_matches_an_independent_regex_scan(self):
        with open(os.path.join(sysmsg.DEFAULT_JAVA_DIR, sysmsg.SYSMSG_JAVA), encoding='utf-8') as f:
            java = f.read()
        declared = re.findall(r'^\tpublic static (SM_SYSTEM_MESSAGE|AionServerPacket) (\w+)\(([^)]*)\) \{\r?\n\t\t(return [^\r\n]*)', java, re.M)
        self.assertEqual(len(declared), 4120)
        sources = ''.join(self.outputs[f'{sysmsg.OUT_PREFIX}.gen{i}.cpp'] for i in range(sysmsg.SOURCE_COUNT))
        defined = re.findall(r'^SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::(\w+)\(([^)]*)\) \{\n'
                             r'\treturn SM_SYSTEM_MESSAGE\((\d+), std::vector<std::string>\{(.*)\}\);\n\}$', sources, re.M)
        header_declarations = re.findall(r'^static SM_SYSTEM_MESSAGE (\w+)\(([^)]*)\);$', self.outputs[sysmsg.HEADER_OUT], re.M)
        expected = [(ret, name, params, body) for ret, name, params, body in declared if 'Player' not in params]
        self.assertEqual(len(defined), len(expected))
        self.assertEqual(len(header_declarations), len(expected))
        plain = 0
        for (ret, name, params, body), (cpp_name, cpp_params, msg_id, args), (h_name, h_params) in zip(expected, defined, header_declarations):
            java_params = [p.split()[-2:] for p in params.split(',')] if params.strip() else []
            self.assertEqual(cpp_name, cpp_identifier(name))
            self.assertEqual(h_name, cpp_name)
            self.assertEqual(h_params, ', '.join(f'{CPP_TYPES[t]} {cpp_identifier(n)}' for t, n in java_params))
            self.assertEqual(cpp_params.replace('/*', '').replace('*/', ''), h_params)
            m = re.match(r'return new SM_SYSTEM_MESSAGE\((\d+)((?:, \w+)*)\);$', body)
            if m and [a for a in m.group(2).split(', ') if a] == [n for _, n in java_params]:
                if cpp_name == name and ret == 'SM_SYSTEM_MESSAGE':
                    plain += 1
                else:  # renamed or different Java return type
                    self.assertIn(f'  {name}({params})  [', '\n'.join(self.report), f'{name} is not plain but not reported')
                self.assertEqual(int(m.group(1)), int(msg_id), name)
                conversions = [f'std::string({n})' if t == 'String' else f'toJavaString({n})' for t, n in java_params]
                self.assertEqual(args, ', '.join(conversions), name)
            else:
                self.assertIn(f'  {name}({params})  [', '\n'.join(self.report), f'{name} is not plain but not reported')
                self.assertIn(f'({msg_id}', body)
        self.assertEqual(plain, self.summary['plain'])

    def test_report_is_in_the_header(self):
        header = self.outputs[sysmsg.HEADER_OUT]
        for line in self.report:
            self.assertIn(f'//   {line}'.rstrip() + '\n', header)
        self.assertIn('_STR_MSG_Heal_TO_ME(int num0)  [C++ name STR_MSG_Heal_TO_ME_]', header)
        self.assertIn('STR_MSG_MERCHANT_PET_GET_SELL_ITEM(String name)  [Java return type AionServerPacket (C++: SM_SYSTEM_MESSAGE)]', header)


if __name__ == '__main__':
    unittest.main()
