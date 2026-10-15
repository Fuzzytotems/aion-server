"""Unit tests of opcodes.py on small Java fixtures."""
import contextlib
import io
import os
import sys
import tempfile
import textwrap
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import opcodes  # noqa: E402

PKG = 'src/com/aionemu/gameserver/network'

CRYPT = '''
package com.aionemu.gameserver.network;

public class Crypt {
	public static int encodeServerPacketOpcode(int opcode) {
		return (opcode + SM_VERSION_CHECK.INTERNAL_VERSION) ^ 0xDF;
	}

	public static int decodeClientPacketOpcode(int opcode) {
		return ((opcode ^ 0xEF) - 0xC ^ 0xEF) - SM_VERSION_CHECK.INTERNAL_VERSION;
	}
}
'''

VERSION_CHECK = '''
package com.aionemu.gameserver.network.aion.serverpackets;

public class SM_VERSION_CHECK extends AionServerPacket {
	public static final int INTERNAL_VERSION = 207;
}
'''

CONNECTION = '''
package com.aionemu.gameserver.network.aion;

public class AionConnection {
	public enum State {
		/** doc */
		CONNECTED,
		AUTHED,
		IN_GAME
	}
}
'''

SERVER_OPCODES = '''
package com.aionemu.gameserver.network.aion;

public class ServerPacketsOpcodes {

	private static Map<Class<? extends AionServerPacket>, Integer> opcodes = new HashMap<>();

	static {
%s
	}

	static int getOpcode(Class<? extends AionServerPacket> packetClass) {
		Integer opcode = opcodes.get(packetClass);
		if (opcode == null)
			throw new IllegalArgumentException("There is no opcode for " + packetClass + " defined.");

		return opcode;
	}

	private static void addPacketOpcode(int opcode, Class<? extends AionServerPacket> packetClass) {
		if (opcode < 0)
			return;

		if (opcodes.values().contains(opcode))
			throw new IllegalArgumentException(String.format("There already exists another packet with id 0x%%02X", opcode));

		opcodes.put(packetClass, opcode);
	}
}
'''

CLIENT_FACTORY = '''
package com.aionemu.gameserver.network.aion;

public class AionClientPacketFactory {
	private static final PacketInfo<? extends AionClientPacket>[] packets = new PacketInfo<?>[%d];

	static {
		try {
%s
		} catch (NoSuchMethodException e) { // should never happen
			throw new ExceptionInInitializerError(e);
		}
	}

	public static AionClientPacket tryCreatePacket(ByteBuffer data, AionConnection client) {
		State state = client.getState();
		int opcode = Crypt.decodeClientPacketOpcode(data.getShort() & 0xffff);
		data.position(data.position() + 3); // skip static code (short) and secondary opcode (byte)
		PacketInfo<? extends AionClientPacket> packetInfo = opcode < 0 || opcode >= packets.length ? null : packets[opcode];
		return null;
	}
}
'''

SERVER_LINES = '''
		addPacketOpcode(72, SM_KEY.class); // [S_KEY]
		// addPacketOpcode(9, ); // [S_LOGIN_CHECK]
		addPacketOpcode(0, SM_VERSION_CHECK.class); // [S_VERSION_CHECK] 4.8
		addPacketOpcode(-1, SM_UNUSED.class);
		addPacketOpcode(32, SM_PLAYER_INFO.class);
'''

CLIENT_LINES = '''
			packets[8] = new PacketInfo<>(CM_ENTER_WORLD.class, State.AUTHED); // [C_ENTER_WORLD (EnterWorldPacket)]
			packets[0] = new PacketInfo<>(CM_VERSION_CHECK.class, State.CONNECTED); // [C_VERSION (VersionPacket)]
			// packets[91] = new PacketInfo<>(CM_GODSTONE_SOCKET.class, State.IN_GAME); // dead
			packets[18] = new PacketInfo<>(CM_TIME_CHECK.class, State.IN_GAME, State.CONNECTED, State.AUTHED);
'''


def client_packet(name, body='public %s(int opcode, Set<State> states) { super(opcode, states); }'):
    return f'''
package com.aionemu.gameserver.network.aion.clientpackets;

public class {name} extends AionClientPacket {{
	{body % name if '%s' in body else body}
}}
'''


def server_packet(name, base='AionServerPacket', modifiers='public'):
    return f'''
package com.aionemu.gameserver.network.aion.serverpackets;

{modifiers} class {name} extends {base} {{
}}
'''


class Fixture:
    def __init__(self, server_lines=SERVER_LINES, client_lines=CLIENT_LINES, table_size=250, files=None):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        base = {
            f'{PKG}/Crypt.java': CRYPT,
            f'{PKG}/aion/serverpackets/SM_VERSION_CHECK.java': VERSION_CHECK,
            f'{PKG}/aion/AionConnection.java': CONNECTION,
            f'{PKG}/aion/ServerPacketsOpcodes.java': SERVER_OPCODES % server_lines,
            f'{PKG}/aion/AionClientPacketFactory.java': CLIENT_FACTORY % (table_size, client_lines),
            f'{PKG}/aion/serverpackets/SM_KEY.java': server_packet('SM_KEY'),
            f'{PKG}/aion/serverpackets/AbstractPlayerInfoPacket.java': server_packet('AbstractPlayerInfoPacket', modifiers='public abstract'),
            f'{PKG}/aion/serverpackets/SM_PLAYER_INFO.java': server_packet('SM_PLAYER_INFO', base='AbstractPlayerInfoPacket'),
            f'{PKG}/aion/clientpackets/CM_ENTER_WORLD.java': client_packet('CM_ENTER_WORLD'),
            f'{PKG}/aion/clientpackets/CM_VERSION_CHECK.java': client_packet('CM_VERSION_CHECK'),
            f'{PKG}/aion/clientpackets/CM_TIME_CHECK.java': client_packet('CM_TIME_CHECK'),
        }
        base.update(files or {})
        for rel, content in base.items():
            if content is None:
                continue
            path = os.path.join(self.root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
                f.write(textwrap.dedent(content))

    def __enter__(self):
        return self.root

    def __exit__(self, *args):
        self.tmp.cleanup()


class ObfuscationTest(unittest.TestCase):
    def test_server_opcode(self):
        self.assertEqual(opcodes.encode_server_opcode(72, 207), 0x1C8)
        self.assertEqual(opcodes.encode_server_opcode(0, 207), 0x10)
        self.assertEqual(opcodes.encode_server_opcode(0x7FFFFFFF, 207), opcodes._int32(0x7FFFFFFF + 207) ^ 0xDF)

    def test_client_opcode_round_trip(self):
        # CM_VERSION_CHECK: C3 ^ EF = 2C, - 0C = 20, ^ EF = CF, - 207 = 0
        self.assertEqual(opcodes.decode_client_opcode(0xC3, 207), 0)
        self.assertEqual(opcodes.client_wire_opcode(0, 207), 0xC3)
        for op in range(250):
            self.assertEqual(opcodes.decode_client_opcode(opcodes.client_wire_opcode(op, 207), 207), op)
        decoded = {opcodes.decode_client_opcode(w, 207) for w in range(0x10000)}
        self.assertEqual(len(decoded), 0x10000)

    def test_client_opcode_without_16_bit_wire_value(self):
        with self.assertRaises(opcodes.GenError):
            opcodes.client_wire_opcode(70000, 207)


class GenerateTest(unittest.TestCase):
    def test_outputs(self):
        with Fixture() as root:
            outputs, summary = opcodes.generate(root)
        self.assertEqual(summary['server_packets'], 3)
        self.assertEqual(summary['client_packets'], 3)
        self.assertEqual(summary['internal_version'], 207)
        self.assertEqual(summary['client_states'], {'AUTHED': 1, 'CONNECTED': 1, 'CONNECTED+AUTHED+IN_GAME': 1})
        header = outputs['network/aion/ServerPacketsOpcodes.gen.h']
        self.assertIn('class SM_KEY;\nclass SM_PLAYER_INFO;\nclass SM_VERSION_CHECK;\n', header)
        self.assertNotIn('SM_UNUSED', header)
        self.assertIn('template <>\ninline constexpr int32_t opcodeOf<serverpackets::SM_VERSION_CHECK> = 0; // [S_VERSION_CHECK] 4.8\n'
                      'template <>\ninline constexpr int32_t opcodeOf<serverpackets::SM_PLAYER_INFO> = 32;\n'
                      'template <>\ninline constexpr int32_t opcodeOf<serverpackets::SM_KEY> = 72; // [S_KEY]\n', header)
        self.assertIn('inline constexpr int32_t INTERNAL_VERSION = 207;', header)
        self.assertIn('inline constexpr std::array<Entry, 3> ENTRIES = {{\n'
                      '\t{0, 0x0010, "SM_VERSION_CHECK", "S_VERSION_CHECK"},\n'
                      '\t{32, 0x0030, "SM_PLAYER_INFO", ""},\n'  # (32 + 207) ^ 0xDF = 0xEF ^ 0xDF
                      '\t{72, 0x01C8, "SM_KEY", "S_KEY"},\n'
                      '}};', header)
        info = outputs['network/aion/ClientPacketInfo.gen.inc']
        self.assertIn('AION_CLIENT_PACKET_TABLE_SIZE(250)\n#endif\n'
                      'AION_CLIENT_PACKET_INFO(0, 0x00C3, CM_VERSION_CHECK, "C_VERSION (VersionPacket)", CONNECTED)\n'
                      f'AION_CLIENT_PACKET_INFO(8, 0x{opcodes.client_wire_opcode(8, 207):04X}, CM_ENTER_WORLD, '
                      '"C_ENTER_WORLD (EnterWorldPacket)", AUTHED)\n'
                      f'AION_CLIENT_PACKET_INFO(18, 0x{opcodes.client_wire_opcode(18, 207):04X}, CM_TIME_CHECK, "", CONNECTED, AUTHED, IN_GAME)\n',
                      info)
        self.assertNotIn('GODSTONE', info)
        self.assertTrue(info.endswith(')\n'))

    def test_deterministic(self):
        with Fixture() as root:
            first, _ = opcodes.generate(root)
            second, _ = opcodes.generate(root)
        self.assertEqual(first, second)

    def test_internal_version_changes_wire_opcodes(self):
        files = {f'{PKG}/aion/serverpackets/SM_VERSION_CHECK.java': VERSION_CHECK.replace('207', '206')}
        with Fixture(files=files) as root:
            outputs, summary = opcodes.generate(root)
        self.assertEqual(summary['internal_version'], 206)
        self.assertIn(f'{{72, 0x{(72 + 206) ^ 0xDF:04X}, "SM_KEY", "S_KEY"}}', outputs['network/aion/ServerPacketsOpcodes.gen.h'])

    def test_write_and_check(self):
        with Fixture() as root, tempfile.TemporaryDirectory() as out, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(opcodes.main(['--java-dir', root, '--out-dir', out, '--check']), 1)
            self.assertEqual(os.listdir(out), [])
            self.assertEqual(opcodes.main(['--java-dir', root, '--out-dir', out]), 0)
            self.assertEqual(opcodes.main(['--java-dir', root, '--out-dir', out, '--check']), 0)
            path = os.path.join(out, 'network/aion/ClientPacketInfo.gen.inc')
            with open(path, 'rb') as f:
                self.assertNotIn(b'\r\n', f.read())
            with open(path, 'ab') as f:
                f.write(b'// edited\n')
            self.assertEqual(opcodes.main(['--java-dir', root, '--out-dir', out, '--check']), 1)


class ErrorTest(unittest.TestCase):
    def assertGenError(self, fragment, **fixture):
        with Fixture(**fixture) as root:
            with self.assertRaises((opcodes.GenError, opcodes.javasrc.JavaSyntaxError)) as ctx:
                opcodes.generate(root)
        self.assertIn(fragment, str(ctx.exception))
        return str(ctx.exception)

    def test_duplicate_server_opcode(self):
        message = self.assertGenError('There already exists another packet with id 0x48',
                                      server_lines=SERVER_LINES + '\t\taddPacketOpcode(72, SM_PLAYER_INFO.class);\n')
        self.assertIn('ServerPacketsOpcodes.java:', message)

    def test_server_class_registered_twice(self):
        self.assertGenError('SM_KEY is registered twice', server_lines=SERVER_LINES + '\t\taddPacketOpcode(73, SM_KEY.class);\n')

    def test_unexpected_server_statement(self):
        self.assertGenError('unexpected statement', server_lines=SERVER_LINES + '\t\taddPacketOpcode(OP, SM_KEY.class);\n')

    def test_missing_server_class(self):
        self.assertGenError('SM_MISSING is registered but', server_lines=SERVER_LINES + '\t\taddPacketOpcode(80, SM_MISSING.class);\n')

    def test_server_class_with_wrong_base(self):
        files = {f'{PKG}/aion/serverpackets/SM_KEY.java': server_packet('SM_KEY', base='Object')}
        self.assertGenError('SM_KEY derives from Object', files=files)

    def test_abstract_server_class(self):
        files = {f'{PKG}/aion/serverpackets/SM_KEY.java': server_packet('SM_KEY', modifiers='public abstract')}
        self.assertGenError('SM_KEY is abstract', files=files)

    def test_changed_add_packet_opcode(self):
        files = {f'{PKG}/aion/ServerPacketsOpcodes.java': (SERVER_OPCODES % SERVER_LINES).replace('if (opcode < 0)', 'if (opcode < 1)')}
        self.assertGenError('body of addPacketOpcode changed', files=files)

    def test_changed_crypt_formula(self):
        files = {f'{PKG}/Crypt.java': CRYPT.replace('- 0xC ^ 0xEF', '- 0xD ^ 0xEF')}
        self.assertGenError('body of decodeClientPacketOpcode changed', files=files)

    def test_duplicate_client_index(self):
        lines = CLIENT_LINES + '\t\t\tpackets[8] = new PacketInfo<>(CM_VERSION_CHECK.class, State.AUTHED);\n'
        self.assertGenError('packets[8] is assigned twice', client_lines=lines)

    def test_client_class_registered_twice(self):
        lines = CLIENT_LINES + '\t\t\tpackets[9] = new PacketInfo<>(CM_VERSION_CHECK.class, State.AUTHED);\n'
        self.assertGenError('CM_VERSION_CHECK is registered twice', client_lines=lines)

    def test_client_index_outside_table(self):
        self.assertGenError('opcode 18 outside the packets array (length 10)', table_size=10)

    def test_unknown_state(self):
        lines = CLIENT_LINES + '\t\t\tpackets[9] = new PacketInfo<>(CM_X.class, State.LOGGED_IN);\n'
        self.assertGenError('unknown state LOGGED_IN', client_lines=lines)

    def test_duplicate_state(self):
        lines = CLIENT_LINES.replace('State.IN_GAME, State.CONNECTED', 'State.IN_GAME, State.IN_GAME')
        self.assertGenError('duplicate state', client_lines=lines)

    def test_client_class_without_reflective_constructor(self):
        files = {f'{PKG}/aion/clientpackets/CM_TIME_CHECK.java': client_packet('CM_TIME_CHECK', body='public CM_TIME_CHECK(int opcode) { }')}
        self.assertGenError('CM_TIME_CHECK has no public constructor (int, Set<State>)', files=files)

    def test_changed_try_create_packet(self):
        files = {f'{PKG}/aion/AionClientPacketFactory.java': (CLIENT_FACTORY % (250, CLIENT_LINES)).replace('+ 3', '+ 2')}
        self.assertGenError('tryCreatePacket changed', files=files)

    def test_text_after_statement(self):
        self.assertGenError('unexpected text after the statement', server_lines=SERVER_LINES + '\t\taddPacketOpcode(80, SM_KEY.class); int x;\n')

    def test_missing_source(self):
        with Fixture(files={f'{PKG}/Crypt.java': None}) as root:
            with self.assertRaises(opcodes.GenError) as ctx:
                opcodes.generate(root)
        self.assertIn('missing Java source', str(ctx.exception))

    def test_main_reports_errors_with_exit_code_2(self):
        lines = CLIENT_LINES + '\t\t\tpackets[8] = new PacketInfo<>(CM_VERSION_CHECK.class, State.AUTHED);\n'
        with Fixture(client_lines=lines) as root, tempfile.TemporaryDirectory() as out, contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(opcodes.main(['--java-dir', root, '--out-dir', out]), 2)
            self.assertEqual(os.listdir(out), [])


if __name__ == '__main__':
    unittest.main()
