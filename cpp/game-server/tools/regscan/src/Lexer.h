#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * A small tokenizer for C++ handler sources and Java handler sources, as far as aion_gs_regscan needs them: comments are skipped, literals are
 * single tokens (raw strings, text blocks, digit separators), preprocessor directives are single tokens. No Java counterpart.
 */
namespace aion::gameserver::tools::regscan {

enum class Language { CPP, JAVA };

enum class TokenKind {
	IDENTIFIER, // also keywords; bytes >= 0x80 count as identifier characters
	NUMBER,     // pp-number
	STRING,     // "..." including prefixes, raw strings and Java text blocks
	CHAR,       // '...'
	PUNCT,      // a single character, or "::"
	DIRECTIVE,  // C++ only: a whole logical preprocessor line starting with '#' (comments inside it are skipped)
};

struct Token {
	TokenKind kind;
	std::string_view text; // view into the lexed source
	uint32_t line;         // 1-based
	uint32_t column;       // 1-based, in bytes
	bool plainString = false; // STRING only: no prefix, not raw, not a text block

	bool is(TokenKind k, std::string_view t) const noexcept { return kind == k && text == t; }
	bool isPunct(std::string_view t) const noexcept { return is(TokenKind::PUNCT, t); }
	bool isIdentifier(std::string_view t) const noexcept { return is(TokenKind::IDENTIFIER, t); }
};

/** A located problem; the tool prints "<file>(<line>,<column>): error: <message>" (MSBuild's canonical error format). */
struct Diagnostic {
	std::string file;
	uint32_t line = 0;
	uint32_t column = 0;
	std::string message;

	std::string format() const;
	friend bool operator<(const Diagnostic& a, const Diagnostic& b);
	friend bool operator==(const Diagnostic& a, const Diagnostic& b) = default;
};

struct LexResult {
	std::vector<Token> tokens;
	std::vector<Diagnostic> errors; // file names are left empty; the caller fills them in
};

/** Tokenizes the source. Unterminated comments and literals are reported as errors (lexing continues after them). */
LexResult lex(std::string_view source, Language language);

/** @return the directive name of a DIRECTIVE token ("if", "define", ...), empty for a null directive */
std::string_view directiveName(const Token& directive) noexcept;

/** @return true if the name is a C++ keyword or alternative operator token */
bool isCppKeyword(std::string_view name) noexcept;

} // namespace aion::gameserver::tools::regscan
