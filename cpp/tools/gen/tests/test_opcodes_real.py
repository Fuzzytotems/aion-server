"""opcodes.py over the real Java tree: counts, Java facts, and the drift check of the committed outputs."""
import os
import re
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import opcodes  # noqa: E402


class RealTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.path.isdir(opcodes.DEFAULT_JAVA_DIR):
            raise AssertionError(f'Java game-server directory missing: {opcodes.DEFAULT_JAVA_DIR}')
        cls.outputs, cls.summary = opcodes.generate(opcodes.DEFAULT_JAVA_DIR)

    def test_committed_outputs_are_up_to_date(self):
        differ = opcodes.write_outputs(self.outputs, opcodes.DEFAULT_OUT_DIR, check=True)
        self.assertEqual(differ, [], 'regenerate with: python cpp/tools/gen/opcodes.py')

    def test_counts(self):
        self.assertEqual(self.summary['internal_version'], 207)
        self.assertEqual(self.summary['server_packets'], 237)
        self.assertEqual(self.summary['max_server_opcode'], 303)
        self.assertEqual(self.summary['client_packets'], 186)
        self.assertEqual(self.summary['client_table_size'], 250)
        self.assertEqual(self.summary['client_states'],
                         {'IN_GAME': 167, 'AUTHED': 10, 'AUTHED+IN_GAME': 4, 'CONNECTED': 3, 'CONNECTED+AUTHED+IN_GAME': 2})

    def test_matches_an_independent_regex_scan_of_the_java_files(self):
        java = os.path.join(opcodes.DEFAULT_JAVA_DIR, opcodes.SERVER_OPCODES_JAVA)
        with open(java, encoding='utf-8') as f:
            server = re.findall(r'^\s*addPacketOpcode\((\d+), (SM_\w+)\.class\);', f.read(), re.M)
        header = self.outputs[opcodes.SERVER_OPCODES_OUT]
        generated = re.findall(r'opcodeOf<serverpackets::(SM_\w+)> = (\d+);', header)
        self.assertEqual(sorted((name, int(op)) for op, name in server), sorted((name, int(op)) for name, op in generated))

        java = os.path.join(opcodes.DEFAULT_JAVA_DIR, opcodes.CLIENT_FACTORY_JAVA)
        with open(java, encoding='utf-8') as f:
            client = re.findall(r'^\s*packets\[(\d+)\] = new PacketInfo<>\((CM_\w+)\.class((?:, State\.\w+)+)\);', f.read(), re.M)
        info = self.outputs[opcodes.CLIENT_INFO_OUT]
        generated = re.findall(r'^AION_CLIENT_PACKET_INFO\((\d+), 0x([0-9A-F]{4}), (CM_\w+), "[^"]*", ([A-Z_, ]+)\)$', info, re.M)
        order = ['CONNECTED', 'AUTHED', 'IN_GAME']
        expected = sorted((int(op), name, tuple(sorted(s.replace('State.', '').strip(', ').split(', '), key=order.index)))
                          for op, name, s in client)
        self.assertEqual(expected, sorted((int(op), name, tuple(s.split(', '))) for op, _, name, s in generated))
        for op, wire, _, _ in generated:
            self.assertEqual(opcodes.decode_client_opcode(int(wire, 16), 207), int(op))


if __name__ == '__main__':
    unittest.main()
