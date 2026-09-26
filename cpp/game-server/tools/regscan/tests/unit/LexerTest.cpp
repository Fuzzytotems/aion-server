#include "RegscanTestSupport.h"

using namespace aion::gameserver::tools::regscan;

namespace {

std::vector<std::string> texts(const LexResult& result) {
	std::vector<std::string> out;
	for (const Token& token : result.tokens)
		out.emplace_back(token.text);
	return out;
}

} // namespace

TEST(LexerTest, CommentsAreSkippedAndPositionsAreTracked) {
	LexResult result = lex("a // one AION_AI(X, \"y\");\n/* two\n three */ b\n  c", Language::CPP);
	EXPECT_NO_ERRORS(result.errors);
	ASSERT_EQ(texts(result), (std::vector<std::string>{"a", "b", "c"}));
	EXPECT_EQ(result.tokens[1].line, 3u);
	EXPECT_EQ(result.tokens[1].column, 11u);
	EXPECT_EQ(result.tokens[2].line, 4u);
	EXPECT_EQ(result.tokens[2].column, 3u);
}

TEST(LexerTest, LineCommentContinuedByBackslash) {
	LexResult result = lex("// comment \\\nstill comment\nx", Language::CPP);
	EXPECT_EQ(texts(result), (std::vector<std::string>{"x"}));
	EXPECT_EQ(result.tokens[0].line, 3u);
}

TEST(LexerTest, StringAndCharLiterals) {
	LexResult result = lex(R"cpp(f("a \" // b", 'x', '\'', u8"p", L'w', R"d(raw ) "( // )d", "tail");)cpp", Language::CPP);
	EXPECT_NO_ERRORS(result.errors);
	std::vector<std::string> expected{"f", "(", "\"a \\\" // b\"", ",", "'x'", ",", "'\\''", ",", "u8\"p\"", ",", "L'w'", ",", "R\"d(raw ) \"( // )d\"", ",", "\"tail\"", ")", ";"};
	EXPECT_EQ(texts(result), expected);
	EXPECT_TRUE(result.tokens[2].plainString);
	EXPECT_FALSE(result.tokens[8].plainString);  // prefix
	EXPECT_FALSE(result.tokens[12].plainString); // raw
	EXPECT_EQ(result.tokens[4].kind, TokenKind::CHAR);
}

TEST(LexerTest, MultiLineRawStringKeepsLines) {
	LexResult result = lex("R\"(a\nb\nc)\" x", Language::CPP);
	ASSERT_EQ(result.tokens.size(), 2u);
	EXPECT_EQ(result.tokens[0].line, 1u);
	EXPECT_EQ(result.tokens[1].line, 3u);
}

TEST(LexerTest, NumbersWithSeparatorsAndExponents) {
	LexResult result = lex("1'000'000 0x1p+3 1.5e-3f 300110000 .5", Language::CPP);
	EXPECT_EQ(texts(result), (std::vector<std::string>{"1'000'000", "0x1p+3", "1.5e-3f", "300110000", ".5"}));
	for (const Token& token : result.tokens)
		EXPECT_EQ(token.kind, TokenKind::NUMBER);
}

TEST(LexerTest, DirectivesAreSingleTokens) {
	LexResult result = lex("#include \"a//b.h\" // comment\n  #  define X(a) \\\n  a + 1\nint y; # not a directive\n#if 0\n#endif", Language::CPP);
	EXPECT_NO_ERRORS(result.errors);
	ASSERT_GE(result.tokens.size(), 6u);
	EXPECT_EQ(result.tokens[0].kind, TokenKind::DIRECTIVE);
	EXPECT_EQ(result.tokens[0].text, "#include \"a//b.h\"");
	EXPECT_EQ(directiveName(result.tokens[0]), "include");
	EXPECT_EQ(result.tokens[1].kind, TokenKind::DIRECTIVE);
	EXPECT_EQ(directiveName(result.tokens[1]), "define");
	EXPECT_EQ(result.tokens[2].text, "int");
	EXPECT_EQ(result.tokens[2].line, 4u);
	EXPECT_TRUE(result.tokens[5].isPunct("#")); // '#' that does not start a line
	EXPECT_EQ(directiveName(result.tokens[result.tokens.size() - 2]), "if");
	EXPECT_EQ(directiveName(result.tokens.back()), "endif");
}

TEST(LexerTest, ScopeOperatorIsOneToken) {
	LexResult result = lex("a::b : c", Language::CPP);
	EXPECT_EQ(texts(result), (std::vector<std::string>{"a", "::", "b", ":", "c"}));
}

TEST(LexerTest, UnterminatedLiteralsAndCommentsAreErrors) {
	EXPECT_ERROR(lex("\"abc\nx", Language::CPP).errors, 1, "unterminated string");
	EXPECT_ERROR(lex("'a\n", Language::CPP).errors, 1, "unterminated character");
	EXPECT_ERROR(lex("x /* never closed", Language::CPP).errors, 1, "unterminated comment");
	EXPECT_ERROR(lex("R\"(abc", Language::CPP).errors, 1, "unterminated raw string");
	EXPECT_ERROR(lex("\"\"\"\ntext", Language::JAVA).errors, 1, "unterminated text block");
}

TEST(LexerTest, JavaTextBlocksAnnotationsAndNoDirectives) {
	LexResult result = lex("@AIName(\"x\")\nString s = \"\"\"\n  a \"quoted\" b\n  \"\"\";\n#x 1_000", Language::JAVA);
	EXPECT_NO_ERRORS(result.errors);
	std::vector<std::string> t = texts(result);
	ASSERT_GE(t.size(), 11u);
	EXPECT_EQ(t[0], "@");
	EXPECT_EQ(t[1], "AIName");
	EXPECT_EQ(result.tokens[8].kind, TokenKind::STRING);
	EXPECT_FALSE(result.tokens[8].plainString);
	EXPECT_EQ(t[10], "#");
	EXPECT_EQ(t.back(), "1_000");
}

TEST(LexerTest, KeywordTable) {
	EXPECT_TRUE(isCppKeyword("template"));
	EXPECT_TRUE(isCppKeyword("delete"));
	EXPECT_TRUE(isCppKeyword("register"));
	EXPECT_TRUE(isCppKeyword("and"));
	EXPECT_FALSE(isCppKeyword("final"));
	EXPECT_FALSE(isCppKeyword("ai"));
	EXPECT_FALSE(isCppKeyword("template_"));
}

TEST(LexerTest, DiagnosticFormat) {
	EXPECT_EQ((Diagnostic{"a/b.cpp", 12, 5, "msg"}).format(), "a/b.cpp(12,5): error: msg");
	EXPECT_EQ((Diagnostic{"a/dir", 0, 0, "directory not found"}).format(), "a/dir: error: directory not found");
}
