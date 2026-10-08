"""M5j command oracle (m5j/commands.py, m5j-plan.md H-01): the Java literal forms, ChatCommand.parseSyntaxInfo, ChatUtil.split and the help
text on hand-made input, and on the real Java tree the levels and the stage-0 commands the gate asserts.

Expected values are derived by hand from ChatCommand.java (parseSyntaxInfo, run), ChatUtil.java (color, split, findSplitIndex, l10n) and
JLS 3.10.6/3.10.7, and repeated per case.
"""

import unittest
from pathlib import Path

from m5j import commands as c
from staticdata_oracle import run as runner

GAME_SERVER = runner.DEFAULT_STATIC_DATA.parent.parent


class LiteralTest(unittest.TestCase):
	def test_escapes(self):
		self.assertEqual(c.unescape(r"a\tb\n\"q\" \\ é \uuu0041"), 'a\tb\n"q" \\ é A')

	def test_text_block_strips_the_indentation_of_the_closing_line(self):
		self.assertEqual(c.text_block('"""\n\t\t\ton - Allows.\n\t\t\t  off - Blocks.  \n\t\t\t"""'), "on - Allows.\n  off - Blocks.\n")
		self.assertEqual(c.text_block('"""\n  a\n    b"""'), "a\n  b")

	def test_a_concatenation_of_literals_is_one_value_and_anything_else_is_named(self):
		self.assertEqual(c._value(['"a"', "+", '"b\\n"']), ("ab\n", None))
		self.assertEqual(c._value(['"a"', "+", "X"]), (None, '"a" + X'))


class SyntaxTest(unittest.TestCase):
	def test_parse_syntax_info(self):
		# "n <message> - Sends." : the alias in white, each word outside <>[]| in white, the rest kept; a line without " - " as is
		got = c.parse_syntax_info("//announce", "n <message> - Sends.\nfree text\n")
		self.assertEqual(got, "Syntax:\n\t[color://announce;1 1 1] [color:n;1 1 1] <[color:message;1 1 1]> - Sends.\nfree text")

	def test_square_brackets_add_the_note_and_blank_syntax_says_so(self):
		got = c.parse_syntax_info(".id", "[name] - Shows.")
		self.assertTrue(got.endswith("\nNote: Parameters enclosed in square brackets are optional."))
		self.assertEqual(c.parse_syntax_info("//online", "  "), "Syntax:\n\tNo syntax info available.")

	def test_help_without_description(self):
		command = {"prefix": "//", "alias": "x", "description": "", "syntax": ""}
		self.assertEqual(c.help_text(command),
		                 "Command: [color://x;1 1 1]\n\tNo description available.\nSyntax:\n\tNo syntax info available.")


class SplitTest(unittest.TestCase):
	def test_short_messages_stay_whole(self):
		self.assertEqual(c.split("a" * 511), ["a" * 511])

	def test_a_long_message_splits_at_the_last_newline_before_the_limit_and_drops_it(self):
		# 1022 counted chars end the first part; the last '\n' before them is at 600, so part 1 is [0, 600) and part 2 starts at 601
		text = "a" * 600 + "\n" + "b" * 600
		self.assertEqual(c.split(text), ["a" * 600, "b" * 600])

	def test_an_l10n_id_counts_sixteen(self):
		# "$" + an odd code unit + 1 more: 3 units counted as 16. 200 of them with spaces are 799 units (> 511, so findSplitIndex runs).
		# After 60 tokens and their 60 spaces the count is 1020; the 61st token makes 1036 >= 1022, so the part ends at the space before it
		token = "$" + chr(0xF481) + chr(8)
		parts = c.split(" ".join([token] * 200))
		self.assertGreater(len(parts), 1)
		self.assertEqual(parts[0].count("$"), 60)
		self.assertFalse(parts[1].startswith(" "))   # the split space is dropped

	def test_l10n_units(self):
		# 293440 << 1 | 1 = 586881 = 0x8F481
		self.assertEqual(c.l10n_units(293440), [ord("$"), 0xF481, 0x8])


class RealTreeTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		if not (GAME_SERVER / "config" / "administration" / "commands.properties").is_file():
			raise unittest.SkipTest("the Java game-server tree is not beside the static data")
		cls.report = c.commands_report(GAME_SERVER, None, [293440])

	def test_levels(self):
		self.assertEqual(self.report["levels"]["kill"], 7)       # commands.properties:56
		self.assertEqual(self.report["levels"]["levelup"], 9)    # :153
		self.assertEqual(len(self.report["levels"]), 152)

	def test_the_stage0_commands_resolve(self):
		for alias in ("//announce", "//say", "//whisper", "//kick", "//gag", "//movie", "//kill", "//online", ".help", ".id", ".gmlist", "levelup",
		              "clearusercoolt", "//addskill", "addskill"):
			self.assertIn(alias, self.report["commands"], alias)
		self.assertEqual(self.report["commands"]["addskill"]["javaFile"], "data/handlers/consolecommands/Addskill.java")   # not //addskill
		kill = self.report["commands"]["//kill"]
		self.assertEqual(kill["accessMessage"], "<You need access level 7 or higher to use //kill>")
		self.assertEqual(self.report["commands"]["levelup"]["aliasWithPrefix"], "levelup")   # ConsoleCommand.PREFIX ""
		self.assertTrue(self.report["commands"][".help"]["help"][0].startswith("Command: [color:.help;1 1 1]\n\tLists all commands"))

	def test_the_unresolved_are_the_computed_ones(self):
		self.assertIn("consolecommands/Bookmark_add", self.report["unresolved"])        # super(ALIAS, ...)
		self.assertIn("playercommands/Easter", self.report["unresolved"])              # ChatUtil.item(...) in the description

	def test_constants(self):
		self.assertEqual(self.report["chatTypes"]["NORMAL"], 0)
		self.assertEqual(self.report["chatTypes"]["BRIGHT_YELLOW_CENTER"], 36)
		self.assertEqual(self.report["whisperLevel"], 10)
		self.assertEqual(self.report["nonDaevaLevelCap"], 9)
		self.assertEqual(self.report["l10n"]["293440"], [36, 0xF481, 8])


if __name__ == "__main__":
	unittest.main()
