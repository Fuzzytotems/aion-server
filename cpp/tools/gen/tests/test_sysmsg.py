"""Unit tests of sysmsg.py on small Java fixtures."""
import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import sysmsg  # noqa: E402

REST = '''
	private int msgId;
	private Object[] params;

	public SM_SYSTEM_MESSAGE(int msgId, Object... params) {
		this.msgId = msgId;
		this.params = params;
	}

	@Override
	protected void writeImpl(AionConnection con) {
		writeD(msgId);
	}

	public int getId() {
		return msgId;
	}
'''

FACTORIES = '''
	/**
	 * You inflicted %num1 damage on %0.
	 */
	public static SM_SYSTEM_MESSAGE STR_MSG_COMBAT_MY_ATTACK(int num1, String value0) {
		return new SM_SYSTEM_MESSAGE(1200000, num1, value0);
	}

	public static SM_SYSTEM_MESSAGE STR_NO_PARAMS() {
		return new SM_SYSTEM_MESSAGE(0x10);
	}

	/**
	 * Current users: %0
	 */
	public static SM_SYSTEM_MESSAGE STR_LIST_USER(int num0) {
		return new SM_SYSTEM_MESSAGE(1300641, num0);
	}

	/**
	 * Current users: %0
	 */
	public static SM_SYSTEM_MESSAGE STR_LIST_USER(String value0) {
		return new SM_SYSTEM_MESSAGE(1300641, value0);
	}

	/**
	 * First line.
	 * Second line: %1 %0
	 */
	public static SM_SYSTEM_MESSAGE STR_REORDERED(String value1, long value0) {
		return new SM_SYSTEM_MESSAGE(2, value0,
			value1);
	}

	public static SM_SYSTEM_MESSAGE STR_CONCAT(int itemId, String name, byte b, float f) {
		return new SM_SYSTEM_MESSAGE(3, "[item:" + itemId + ";" + name + "]", b + "x", f, 5, -7L, "lit\\"q");
	}

	public static SM_SYSTEM_MESSAGE STR_UNUSED(String value0, int value1) {
		return new SM_SYSTEM_MESSAGE(4, value1);
	}

	public static AionServerPacket STR_PACKET(String name) {
		return new SM_SYSTEM_MESSAGE(5, name);
	}

	public static SM_SYSTEM_MESSAGE _STR_RESERVED__NAME(int template) {
		return new SM_SYSTEM_MESSAGE(6, template);
	}

	public static SM_SYSTEM_MESSAGE STR_PLAYER(Player player, String skill) {
		return new SM_SYSTEM_MESSAGE(7, player.getName(), skill);
	}

	public static SM_SYSTEM_MESSAGE STR_PLAYER(Player player) {
		return SM_SYSTEM_MESSAGE.STR_PLAYER(player, "%SubZone:" + player.getPosition().getMapId());
	}
'''


def java_class(factories=FACTORIES, rest=REST):
    return f'''package com.aionemu.gameserver.network.aion.serverpackets;

import com.aionemu.gameserver.network.aion.AionServerPacket;

/**
 * System message packet.
 */
public final class SM_SYSTEM_MESSAGE extends AionServerPacket {{
{factories}
{rest}
}}
'''


class Fixture:
    def __init__(self, source):
        self.tmp = tempfile.TemporaryDirectory()
        path = os.path.join(self.tmp.name, sysmsg.SYSMSG_JAVA)
        os.makedirs(os.path.dirname(path))
        if source is not None:
            with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
                f.write(source)

    def __enter__(self):
        return self.tmp.name

    def __exit__(self, *args):
        self.tmp.cleanup()


def all_sources(outputs):
    return ''.join(outputs[f'{sysmsg.OUT_PREFIX}.gen{i}.cpp'] for i in range(sysmsg.SOURCE_COUNT))


class GenerateTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with Fixture(java_class()) as root:
            cls.outputs, cls.summary, cls.report = sysmsg.generate(root)
        cls.header = cls.outputs[sysmsg.HEADER_OUT]
        cls.sources = all_sources(cls.outputs)

    def test_summary(self):
        self.assertEqual(self.summary['factories'], 11)
        self.assertEqual(self.summary['distinct_names'], 9)
        self.assertEqual(self.summary['plain'], 4)
        self.assertEqual(self.summary['hand_ported'], ['STR_PLAYER(Player player)', 'STR_PLAYER(Player player, String skill)'])
        self.assertEqual(self.summary['generated_non_plain'], ['STR_CONCAT(int itemId, String name, byte b, float f)', 'STR_PACKET(String name)',
                                                               'STR_REORDERED(String value1, long value0)', 'STR_UNUSED(String value0, int value1)',
                                                               '_STR_RESERVED__NAME(int template)'])
        self.assertEqual(self.summary['parameter_types'], {'Player': 2, 'String': 7, 'byte': 1, 'float': 1, 'int': 5, 'long': 1})

    def test_outputs_are_the_header_and_eight_sources(self):
        self.assertEqual(sorted(self.outputs), [sysmsg.HEADER_OUT] + [f'{sysmsg.OUT_PREFIX}.gen{i}.cpp' for i in range(8)])
        for i in range(8):
            source = self.outputs[f'{sysmsg.OUT_PREFIX}.gen{i}.cpp']
            self.assertIn('#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"\n', source)
            self.assertIn('namespace aion::gameserver::network::aion::serverpackets {\n', source)

    def test_declarations_with_javadoc(self):
        self.assertIn('/** You inflicted %num1 damage on %0. */\n'
                      'static SM_SYSTEM_MESSAGE STR_MSG_COMBAT_MY_ATTACK(int32_t num1, std::string_view value0);\n'
                      'static SM_SYSTEM_MESSAGE STR_NO_PARAMS();\n'
                      '/** Current users: %0 */\n'
                      'static SM_SYSTEM_MESSAGE STR_LIST_USER(int32_t num0);\n'
                      '/** Current users: %0 */\n'
                      'static SM_SYSTEM_MESSAGE STR_LIST_USER(std::string_view value0);\n'
                      '/**\n * First line.\n * Second line: %1 %0\n */\n'
                      'static SM_SYSTEM_MESSAGE STR_REORDERED(std::string_view value1, int64_t value0);\n', self.header)
        self.assertIn('static SM_SYSTEM_MESSAGE STR_RESERVED_NAME_(int32_t template_);\n', self.header)
        self.assertIn('static SM_SYSTEM_MESSAGE STR_PACKET(std::string_view name);\n', self.header)
        self.assertNotIn('STR_PLAYER(', self.header.split('// Report')[1].split('\n\n', 3)[-1])
        self.assertNotIn('#pragma once', self.header)

    def test_definitions(self):
        self.assertIn('SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ATTACK(int32_t num1, std::string_view value0) {\n'
                      '\treturn SM_SYSTEM_MESSAGE(1200000, std::vector<std::string>{toJavaString(num1), std::string(value0)});\n}\n', self.sources)
        self.assertIn('SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_NO_PARAMS() {\n'
                      '\treturn SM_SYSTEM_MESSAGE(16, std::vector<std::string>{});\n}\n', self.sources)
        self.assertIn('\treturn SM_SYSTEM_MESSAGE(2, std::vector<std::string>{toJavaString(value0), std::string(value1)});\n', self.sources)
        self.assertIn('SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_CONCAT(int32_t itemId, std::string_view name, int8_t b, float f) {\n'
                      '\treturn SM_SYSTEM_MESSAGE(3, std::vector<std::string>{std::string("[item:") + toJavaString(itemId) + ";" + std::string(name)'
                      ' + "]", toJavaString(b) + "x", toJavaString(f), toJavaString(int32_t{5}), toJavaString(int64_t{-7}), '
                      'std::string("lit\\"q")});\n',
                      self.sources)
        self.assertIn('SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_UNUSED(std::string_view /*value0*/, int32_t value1) {\n', self.sources)
        self.assertIn('SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::STR_RESERVED_NAME_(int32_t template_) {\n'
                      '\treturn SM_SYSTEM_MESSAGE(6, std::vector<std::string>{toJavaString(template_)});\n', self.sources)
        self.assertNotIn('STR_PLAYER', self.sources)

    def test_sources_keep_source_order_and_split_evenly(self):
        names = ['STR_MSG_COMBAT_MY_ATTACK(', 'STR_NO_PARAMS(', 'STR_LIST_USER(int32_t', 'STR_LIST_USER(std::string_view', 'STR_REORDERED(',
                 'STR_CONCAT(', 'STR_UNUSED(', 'STR_PACKET(', 'STR_RESERVED_NAME_(']
        positions = [self.sources.index('SM_SYSTEM_MESSAGE::' + n) for n in names]
        self.assertEqual(positions, sorted(positions))
        counts = [self.outputs[f'{sysmsg.OUT_PREFIX}.gen{i}.cpp'].count('SM_SYSTEM_MESSAGE SM_SYSTEM_MESSAGE::') for i in range(8)]
        self.assertEqual(counts, [2, 2, 2, 2, 1, 0, 0, 0])

    def test_report(self):
        report = '\n'.join(self.report)
        self.assertIn('11 factories in Java: 9 generated (4 plain, 5 with other bodies), 2 to port by hand.', report)
        self.assertIn('  STR_PLAYER(Player player, String skill)  [parameter Player player]\n'
                      '    return new SM_SYSTEM_MESSAGE(7, player.getName(), skill);', report)
        self.assertIn('  STR_REORDERED(String value1, long value0)  [parameters passed in a different order]\n'
                      '    return new SM_SYSTEM_MESSAGE(2, value0, value1)', report)
        self.assertIn('STR_CONCAT(int itemId, String name, byte b, float f)  [computed argument "[item:" + itemId + ";" + name + "]"; '
                      'computed argument b + "x"; literal argument 5; literal argument -7L; literal argument "lit\\"q"]', report)
        self.assertIn('STR_UNUSED(String value0, int value1)  [unused parameter value0]', report)
        self.assertIn('STR_PACKET(String name)  [Java return type AionServerPacket (C++: SM_SYSTEM_MESSAGE)]', report)
        self.assertIn('_STR_RESERVED__NAME(int template)  [C++ name STR_RESERVED_NAME_]', report)
        self.assertIn('Not generated (instance members, ported by hand): SM_SYSTEM_MESSAGE constructor, getId(), writeImpl()', report)
        self.assertIn('//   11 factories in Java', self.header)

    def test_check_mode_and_line_endings(self):
        with Fixture(java_class()) as root, tempfile.TemporaryDirectory() as out, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(sysmsg.main(['--java-dir', root, '--out-dir', out, '--check']), 1)
            self.assertEqual(sysmsg.main(['--java-dir', root, '--out-dir', out]), 0)
            self.assertEqual(sysmsg.main(['--java-dir', root, '--out-dir', out, '--check']), 0)
            with open(os.path.join(out, sysmsg.HEADER_OUT), 'rb') as f:
                self.assertNotIn(b'\r', f.read())

    def test_deterministic(self):
        with Fixture(java_class()) as root:
            again, _, _ = sysmsg.generate(root)
        self.assertEqual(again, self.outputs)


class ErrorTest(unittest.TestCase):
    def assertGenError(self, factories, fragment, rest=REST):
        with Fixture(java_class(factories, rest)) as root:
            with self.assertRaises(sysmsg.GenError) as ctx:
                sysmsg.generate(root)
        self.assertIn(fragment, str(ctx.exception))
        return str(ctx.exception)

    def factory(self, params, body):
        return f'\tpublic static SM_SYSTEM_MESSAGE STR_X({params}) {{\n\t\t{body}\n\t}}\n'

    def test_numeric_addition(self):
        message = self.assertGenError(self.factory('int a, int b', 'return new SM_SYSTEM_MESSAGE(1, a + b + "x");'), 'numeric addition')
        self.assertIn('SM_SYSTEM_MESSAGE.java:', message)

    def test_unsupported_parameter_type(self):
        self.assertGenError(self.factory('double d', 'return new SM_SYSTEM_MESSAGE(1, d);'), 'unsupported parameter type double')

    def test_unsupported_body(self):
        self.assertGenError(self.factory('int a', 'int b = a; return new SM_SYSTEM_MESSAGE(1, b);'), 'unsupported body')

    def test_method_call_argument(self):
        self.assertGenError(self.factory('String a', 'return new SM_SYSTEM_MESSAGE(1, a.trim());'), 'unsupported')

    def test_unknown_name_argument(self):
        self.assertGenError(self.factory('int a', 'return new SM_SYSTEM_MESSAGE(1, b);'), 'argument b is not a parameter')

    def test_message_id_must_be_a_literal(self):
        self.assertGenError(self.factory('int a', 'return new SM_SYSTEM_MESSAGE(a);'), 'the message id must be an int literal')

    def test_varargs(self):
        self.assertGenError(self.factory('String... a', 'return new SM_SYSTEM_MESSAGE(1, a);'), 'unsupported parameter form')

    def test_duplicate_cpp_signature(self):
        factories = ('\tpublic static SM_SYSTEM_MESSAGE STR__A(int a) { return new SM_SYSTEM_MESSAGE(1, a); }\n'
                     '\tpublic static SM_SYSTEM_MESSAGE STR_A_(int a) { return new SM_SYSTEM_MESSAGE(2, a); }\n')
        self.assertGenError(factories, 'have the same C++ signature')

    def test_non_ascii_string_literal(self):
        self.assertGenError(self.factory('int a', 'return new SM_SYSTEM_MESSAGE(1, "é" + a);'), 'unsupported character U+00E9')

    def test_unexpected_static_member(self):
        factories = FACTORIES + '\tprivate static SM_SYSTEM_MESSAGE helper() { return null; }\n'
        self.assertGenError(factories, 'unexpected static member helper')

    def test_static_field(self):
        self.assertGenError(FACTORIES, 'unexpected static field CACHE', rest=REST + '\tprivate static final Object CACHE = null;\n')

    def test_no_factories(self):
        self.assertGenError('', 'no factories found')

    def test_missing_file(self):
        with Fixture(None) as root:
            with self.assertRaises(sysmsg.GenError):
                sysmsg.generate(root)


if __name__ == '__main__':
    unittest.main()
