"""Unit tests of dialogaction.py on small Java fixtures."""
import contextlib
import io
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import dialogaction  # noqa: E402

INITIALIZER_AND_METHODS = '''
	private static final Map<Integer, String> nameById = new HashMap<>();

	static {
		Map<Integer, String> nameById = DialogAction.nameById; // this temporary variable fixes extremely slow map access times
		try {
			for (Field publicField : DialogAction.class.getFields()) {
					if (publicField.get(null) instanceof Integer dialogActionId) {
						String previousName = nameById.put(dialogActionId, publicField.getName());
						if (previousName != null)
							throw new IllegalArgumentException("Duplicate id " + dialogActionId + ": " + publicField.getName() + ", " + previousName);
					}
			}
		} catch (IllegalArgumentException | IllegalAccessException e) {
			throw new ExceptionInInitializerError(e);
		}
	}

	private DialogAction() {
	}

	public static String nameOf(int dialogActionId) {
		return nameById.get(dialogActionId);
	}
'''

CONSTANTS = '''
	public static final int USE_OBJECT = -1; // called ERROR in client
	// 0 doesn't exist
	public static final int NULL = 1;
	public static final int QUEST_SELECT = 0x1F;
	public static final int SETPRO1 = 10000;
	public static final int OPEN_WEB = 100000; /* block comment */
'''


def java_class(constants=CONSTANTS, rest=INITIALIZER_AND_METHODS):
    return f'''package com.aionemu.gameserver.model;

import java.lang.reflect.Field;

/**
 * Converted from enum to regular class.
 *
 * @author Rolandas, Neon
 */
public final class DialogAction {{
{constants}
{rest}
}}
'''


class Fixture:
    def __init__(self, source):
        self.tmp = tempfile.TemporaryDirectory()
        path = os.path.join(self.tmp.name, dialogaction.DIALOG_ACTION_JAVA)
        os.makedirs(os.path.dirname(path))
        if source is not None:
            with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
                f.write(source)

    def __enter__(self):
        return self.tmp.name

    def __exit__(self, *args):
        self.tmp.cleanup()


class CppIdentifierTest(unittest.TestCase):
    def test_renames(self):
        self.assertEqual(dialogaction.cpp_identifier('QUEST_SELECT'), 'QUEST_SELECT')
        self.assertEqual(dialogaction.cpp_identifier('NULL'), 'NULL_')
        self.assertEqual(dialogaction.cpp_identifier('EOF'), 'EOF_')
        self.assertEqual(dialogaction.cpp_identifier('delete'), 'delete_')
        self.assertEqual(dialogaction.cpp_identifier('template'), 'template_')
        self.assertEqual(dialogaction.cpp_identifier('_STR_MSG_Heal_TO_ME'), 'STR_MSG_Heal_TO_ME_')
        self.assertEqual(dialogaction.cpp_identifier('STR_A__B'), 'STR_A_B_')
        self.assertEqual(dialogaction.cpp_identifier('_lower'), '_lower')  # reserved only at namespace scope; not used for members
        self.assertEqual(dialogaction.cpp_identifier('DELETE'), 'DELETE')  # removed by WindowsMacroGuard.h


class GenerateTest(unittest.TestCase):
    def test_outputs(self):
        with Fixture(java_class()) as root:
            outputs, summary = dialogaction.generate(root)
        self.assertEqual(summary, {'constants': 5, 'min_id': -1, 'max_id': 100000, 'renamed': ['NULL -> NULL_']})
        header = outputs['model/DialogAction.h']
        self.assertIn(' * Converted from enum to regular class.\n * <p>\n', header)
        self.assertIn(' * Renamed because the Java name is a C++ keyword or macro: NULL -> NULL_.\n', header)
        self.assertIn(' * @author Rolandas, Neon\n */\nnamespace aion::gameserver::model::DialogAction {\n', header)
        self.assertIn('inline constexpr int32_t USE_OBJECT = -1; // called ERROR in client\n'
                      'inline constexpr int32_t NULL_ = 1; // Java: NULL\n'
                      'inline constexpr int32_t QUEST_SELECT = 31;\n'
                      'inline constexpr int32_t SETPRO1 = 10000;\n'
                      'inline constexpr int32_t OPEN_WEB = 100000;\n', header)
        self.assertIn('std::optional<std::string_view> nameOf(int32_t dialogActionId) noexcept;', header)
        source = outputs['model/DialogAction.gen.cpp']
        self.assertIn('constexpr std::array<Entry, 5> ENTRIES = {{\n'
                      '\t{USE_OBJECT, "USE_OBJECT"},\n'
                      '\t{NULL_, "NULL"},\n'
                      '\t{QUEST_SELECT, "QUEST_SELECT"},\n'
                      '\t{SETPRO1, "SETPRO1"},\n'
                      '\t{OPEN_WEB, "OPEN_WEB"},\n'
                      '}};', source)
        self.assertIn('static_assert(strictlyIncreasingIds(), "DialogAction: duplicate id");', source)

    def test_table_is_sorted_by_id(self):
        constants = '''
	public static final int B = 20;
	public static final int A = 10;
	public static final int C = -5;
'''
        with Fixture(java_class(constants)) as root:
            outputs, _ = dialogaction.generate(root)
        self.assertIn('\t{C, "C"},\n\t{A, "A"},\n\t{B, "B"},\n', outputs['model/DialogAction.gen.cpp'])
        self.assertIn('int32_t B = 20;\ninline constexpr int32_t A = 10;\ninline constexpr int32_t C = -5;', outputs['model/DialogAction.h'])

    def test_int_range(self):
        constants = '\tpublic static final int MIN = -2147483648;\n\tpublic static final int MAX = 2147483647;\n'
        with Fixture(java_class(constants)) as root:
            _, summary = dialogaction.generate(root)
        self.assertEqual((summary['min_id'], summary['max_id']), (-2147483648, 2147483647))

    def test_check_mode(self):
        with Fixture(java_class()) as root, tempfile.TemporaryDirectory() as out, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(dialogaction.main(['--java-dir', root, '--out-dir', out, '--check']), 1)
            self.assertEqual(dialogaction.main(['--java-dir', root, '--out-dir', out]), 0)
            self.assertEqual(dialogaction.main(['--java-dir', root, '--out-dir', out, '--check']), 0)
            with open(os.path.join(out, 'model/DialogAction.h'), 'rb') as f:
                self.assertNotIn(b'\r', f.read())


class ErrorTest(unittest.TestCase):
    def assertGenError(self, source, fragment):
        with Fixture(source) as root:
            with self.assertRaises(dialogaction.GenError) as ctx:
                dialogaction.generate(root)
        self.assertIn(fragment, str(ctx.exception))
        return str(ctx.exception)

    def test_duplicate_id_like_the_java_static_initializer(self):
        message = self.assertGenError(java_class(CONSTANTS + '\tpublic static final int SELECT_AGAIN = 31;\n'),
                                      'Duplicate id 31: SELECT_AGAIN, QUEST_SELECT')
        self.assertIn('DialogAction.java:', message)

    def test_cpp_name_collision(self):
        self.assertGenError(java_class(CONSTANTS + '\tpublic static final int NULL_ = 2;\n'), 'NULL_ and NULL have the same C++ name NULL_')

    def test_non_literal_initializer(self):
        self.assertGenError(java_class(CONSTANTS + '\tpublic static final int SUM = 1 + 2;\n'), 'must be initialized with an int literal')

    def test_long_literal(self):
        self.assertGenError(java_class(CONSTANTS + '\tpublic static final int BIG = 5L;\n'), 'is not an int')

    def test_other_public_field_type(self):
        self.assertGenError(java_class(CONSTANTS + '\tpublic static final long BIG = 5;\n'), 'is not a plain public static final int')

    def test_non_static_public_field(self):
        self.assertGenError(java_class(CONSTANTS + '\tpublic final int X = 5;\n'), 'is not a plain public static final int')

    def test_multiple_declarators(self):
        self.assertGenError(java_class(CONSTANTS + '\tpublic static final int X = 5, Y = 6;\n'), 'expected one declarator per statement')

    def test_changed_initializer(self):
        self.assertGenError(java_class(rest=INITIALIZER_AND_METHODS.replace('previousName != null', 'previousName == null')),
                            'the static initializer changed')

    def test_additional_method(self):
        self.assertGenError(java_class(rest=INITIALIZER_AND_METHODS + '\tpublic static int count() { return 1; }\n'),
                            'expected exactly a private constructor and nameOf(int)')

    def test_unexpected_private_field(self):
        self.assertGenError(java_class(CONSTANTS + '\tprivate static int counter;\n'), 'unexpected non-public field counter')

    def test_no_constants(self):
        self.assertGenError(java_class(''), 'no constants found')

    def test_missing_file(self):
        self.assertGenError(None, 'missing Java source')

    def test_java_syntax_error_exits_with_2(self):
        with Fixture(java_class('\tpublic static final int X = ;;\n\t}')) as root, tempfile.TemporaryDirectory() as out, \
                contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(dialogaction.main(['--java-dir', root, '--out-dir', out]), 2)
        self.assertIn('error:', err.getvalue())


if __name__ == '__main__':
    unittest.main()
