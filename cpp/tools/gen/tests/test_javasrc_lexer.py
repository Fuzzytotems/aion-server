"""Tokenizer and literal decoding tests for javasrc."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import javasrc  # noqa: E402
from javasrc import CHAR, EOF, FLOAT, IDENT, INT, KEYWORD, OP, STRING, TEXTBLOCK, JavaSyntaxError, literal_value, tokenize  # noqa: E402


def kinds_texts(src):
    tk = tokenize(src)
    return [(k, t) for k, t in zip(tk.kind, tk.text)][:-1]


class TokenKinds(unittest.TestCase):
    def test_identifiers_keywords_contextual(self):
        toks = kinds_texts('public record var yield sealed permits when _ $x a1 int non')
        self.assertEqual(toks, [(KEYWORD, 'public'), (IDENT, 'record'), (IDENT, 'var'), (IDENT, 'yield'), (IDENT, 'sealed'),
                                (IDENT, 'permits'), (IDENT, 'when'), (IDENT, '_'), (IDENT, '$x'), (IDENT, 'a1'), (KEYWORD, 'int'),
                                (IDENT, 'non')])

    def test_operators_longest_match_and_split_shifts(self):
        texts = [t for _, t in kinds_texts('a >>>= b <<= c >>= d ... -> :: ++ -- && || == != <= >= += -= *= /= &= |= ^= %= << >> >>>')]
        self.assertEqual(texts, ['a', '>>>=', 'b', '<<=', 'c', '>>=', 'd', '...', '->', '::', '++', '--', '&&', '||', '==', '!=', '<=',
                                 '>=', '+=', '-=', '*=', '/=', '&=', '|=', '^=', '%=', '<<', '>', '>', '>', '>', '>'])

    def test_generic_closers_are_single_tokens(self):
        texts = [t for _, t in kinds_texts('Map<String, List<Set<Integer>>> m;')]
        self.assertEqual(texts[-5:], ['>', '>', '>', 'm', ';'])

    def test_numbers(self):
        toks = kinds_texts('0 12 0x1F 0XffL 0b1010 017 1_000_000 3L 1.5 .5 1e10 2.5e-3f 3f 4d 0x1.8p1 1.')
        self.assertEqual([k for k, _ in toks], [INT, INT, INT, INT, INT, INT, INT, INT, FLOAT, FLOAT, FLOAT, FLOAT, FLOAT, FLOAT, FLOAT, FLOAT])
        values = [literal_value(k, t) for k, t in toks]
        self.assertEqual(values[:8], [0, 12, 31, 255, 10, 15, 1000000, 3])
        self.assertEqual(values[8:], [1.5, 0.5, 1e10, 2.5e-3, 3.0, 4.0, 3.0, 1.0])

    def test_member_access_after_int_is_not_float(self):
        self.assertEqual([t for _, t in kinds_texts('a[0].b')], ['a', '[', '0', ']', '.', 'b'])

    def test_strings_and_chars(self):
        toks = kinds_texts(r'''"a\"b\\c\n" 'x' '\'' '\\' '' '\101' "»JDev«" ""''')
        self.assertEqual([k for k, _ in toks], [STRING, CHAR, CHAR, CHAR, CHAR, CHAR, STRING, STRING])
        values = [literal_value(k, t) for k, t in toks]
        self.assertEqual(values, ['a"b\\c\n', 'x', "'", '\\', '', 'A', '»JDev«', ''])

    def test_text_block(self):
        src = 'String s = """\n        hello "quoted" ""\n          world\\tx \\\n        joined\n        """;'
        tk = tokenize(src)
        i = tk.kind.index(TEXTBLOCK)
        self.assertEqual(literal_value(TEXTBLOCK, tk.text[i]), 'hello "quoted" ""\n  world\tx joined\n')
        self.assertEqual(tk.text[i + 1], ';')

    def test_text_block_closing_on_content_line(self):
        tk = tokenize('x("""\n    a\n      b""")')
        i = tk.kind.index(TEXTBLOCK)
        self.assertEqual(literal_value(TEXTBLOCK, tk.text[i]), 'a\n  b')

    def test_text_block_escaped_quotes(self):
        tk = tokenize('x("""\n  a \\""" b\n  """)')
        i = tk.kind.index(TEXTBLOCK)
        self.assertEqual(literal_value(TEXTBLOCK, tk.text[i]), 'a """ b\n')

    def test_booleans_null(self):
        self.assertIs(literal_value(KEYWORD, 'true'), True)
        self.assertIs(literal_value(KEYWORD, 'false'), False)
        self.assertIsNone(literal_value(KEYWORD, 'null'))
        with self.assertRaises(ValueError):
            literal_value(IDENT, 'foo')


class CommentsAndDocs(unittest.TestCase):
    def test_comments_skipped_docs_attached(self):
        src = '/* block */ // line\n/** Doc of A. */\n/* other */ class A { /**/ int x; /***/ int y; }'
        tk = tokenize(src)
        self.assertEqual(tk.text[:3], ['class', 'A', '{'])
        self.assertEqual(tk.docs, {0: '/** Doc of A. */', 6: '/***/'})

    def test_comment_like_text_in_strings(self):
        self.assertEqual([t for _, t in kinds_texts('"/* not a comment */" + "//"')], ['"/* not a comment */"', '+', '"//"'])

    def test_star_heavy_comment(self):
        self.assertEqual([t for _, t in kinds_texts('/***** x ** / **/ a /** b ***/ c')], ['a', 'c'])


class Locations(unittest.TestCase):
    def test_line_col_crlf_and_tabs(self):
        tk = tokenize('class A {\r\n\tint x;\r\n}\r\n')
        self.assertEqual(tk.loc(tk.text.index('int')), (2, 2))
        self.assertEqual(tk.loc(tk.text.index('}')), (3, 1))
        self.assertEqual(tk.kind[-1], EOF)
        self.assertEqual(tk.token(1).text, 'A')
        self.assertEqual((tk.token(1).line, tk.token(1).col), (1, 7))

    def test_bom_stripped(self):
        tk = tokenize('﻿class A {}')
        self.assertEqual(tk.text[0], 'class')
        self.assertEqual(tk.loc(0), (1, 1))

    def test_match_and_parent(self):
        tk = tokenize('f(a[1], {b}) ;')
        o = tk.text.index('(')
        c = tk.text.index(')')
        self.assertEqual((tk.match[o], tk.match[c]), (c, o))
        self.assertEqual(tk.parent[tk.text.index('1')], tk.text.index('['))
        self.assertEqual(tk.parent[tk.text.index('b')], tk.text.index('{'))
        self.assertEqual(tk.parent[tk.text.index('a')], o)
        self.assertEqual(tk.parent[c], -1)
        self.assertEqual(tk.parent[tk.text.index(';')], -1)
        self.assertEqual(tk.match[tk.text.index('a')], -1)

    def test_source_text(self):
        tk = tokenize('int  x =  1 + 2 ;')
        self.assertEqual(tk.source_text(3, 6), '1 + 2')
        self.assertEqual(tk.source_text(3, 3), '')


class Errors(unittest.TestCase):
    def assertError(self, src, line, col, fragment):
        with self.assertRaises(JavaSyntaxError) as cm:
            tokenize(src, 'X.java')
        e = cm.exception
        self.assertEqual((e.path, e.line, e.col), ('X.java', line, col), str(e))
        self.assertIn(fragment, e.message)
        self.assertTrue(str(e).startswith(f'X.java:{line}:{col}: '))

    def test_unterminated_comment(self):
        self.assertError('a\n  /* never closed', 2, 3, 'unterminated comment')

    def test_unterminated_string(self):
        self.assertError('a = "abc\n;', 1, 5, 'unterminated')

    def test_unterminated_text_block(self):
        self.assertError('a = """\nabc', 1, 5, 'unterminated text block')

    def test_bad_character(self):
        self.assertError('a # b', 1, 3, 'unexpected character')

    def test_number_glued_to_identifier(self):
        self.assertError('int x = 123abc;', 1, 9, 'unexpected character')

    def test_unbalanced_brackets(self):
        self.assertError('f(a];', 1, 4, "unbalanced ']'")
        self.assertError('class A {\n  void f() {\n', 2, 12, "unclosed '{'")

    def test_bad_escape(self):
        with self.assertRaises(JavaSyntaxError):
            literal_value(STRING, r'"\q"')


class Module(unittest.TestCase):
    def test_all_exports_exist(self):
        for name in javasrc.__all__:
            self.assertTrue(hasattr(javasrc, name), name)

    def test_docstring_documents_api(self):
        for name in ('tokenize', 'parse_source', 'ProjectIndex', 'lambdas()', 'local_variables()', 'field_assignments()',
                     'method_calls()', 'new_expressions()', 'identifier_refs()', 'resolve('):
            self.assertIn(name, javasrc.__doc__)

    def test_op_kind(self):
        self.assertEqual(kinds_texts('@')[0], (OP, '@'))


if __name__ == '__main__':
    unittest.main()
