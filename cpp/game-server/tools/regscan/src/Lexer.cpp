#include "Lexer.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <tuple>

namespace aion::gameserver::tools::regscan {

std::string Diagnostic::format() const {
	if (line == 0)
		return file + ": error: " + message;
	return file + "(" + std::to_string(line) + "," + std::to_string(column) + "): error: " + message;
}

bool operator<(const Diagnostic& a, const Diagnostic& b) {
	return std::tie(a.file, a.line, a.column, a.message) < std::tie(b.file, b.line, b.column, b.message);
}

namespace {

bool isIdentifierStart(unsigned char c, Language language) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80 || (language == Language::JAVA && c == '$');
}

bool isIdentifierPart(unsigned char c, Language language) noexcept {
	return isIdentifierStart(c, language) || (c >= '0' && c <= '9');
}

bool isDigit(unsigned char c) noexcept {
	return c >= '0' && c <= '9';
}

class Lexer {
public:
	Lexer(std::string_view source, Language language) : src(source), language(language) {}

	LexResult run() {
		while (pos < src.size()) {
			char c = src[pos];
			if (c == '\n') {
				newline();
				atLineStart = true;
				continue;
			}
			if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
				pos++;
				continue;
			}
			if (language == Language::CPP && c == '\\' && spliceLength(pos) > 0) {
				size_t length = spliceLength(pos);
				pos += length - 1;
				newline();
				continue;
			}
			if (c == '/' && peek(1) == '/') {
				skipLineComment();
				continue;
			}
			if (c == '/' && peek(1) == '*') {
				skipBlockComment();
				continue;
			}
			if (language == Language::CPP && c == '#' && atLineStart) {
				lexDirective();
				continue;
			}
			atLineStart = false;
			unsigned char u = static_cast<unsigned char>(c);
			if (isIdentifierStart(u, language)) {
				lexIdentifierOrPrefixedLiteral();
			} else if (isDigit(u) || (c == '.' && isDigit(static_cast<unsigned char>(peek(1))))) {
				lexNumber();
			} else if (c == '"') {
				if (language == Language::JAVA && src.substr(pos, 3) == "\"\"\"")
					lexTextBlock(pos);
				else
					lexQuoted(pos, pos, '"', TokenKind::STRING, true);
			} else if (c == '\'') {
				lexQuoted(pos, pos, '\'', TokenKind::CHAR, false);
			} else if (c == ':' && peek(1) == ':') {
				emit(TokenKind::PUNCT, pos, 2);
				pos += 2;
			} else {
				emit(TokenKind::PUNCT, pos, 1);
				pos++;
			}
		}
		return std::move(result);
	}

private:
	char peek(size_t offset) const noexcept { return pos + offset < src.size() ? src[pos + offset] : '\0'; }

	uint32_t columnOf(size_t index) const noexcept { return static_cast<uint32_t>(index - lineStart + 1); }

	/** pos is at '\n': counts the line and moves past it */
	void newline() noexcept {
		pos++;
		line++;
		lineStart = pos;
	}

	/** @return the length of a backslash-newline splice at index (including the newline), 0 if there is none */
	size_t spliceLength(size_t index) const noexcept {
		if (src[index] != '\\')
			return 0;
		if (index + 1 < src.size() && src[index + 1] == '\n')
			return 2;
		if (index + 2 < src.size() && src[index + 1] == '\r' && src[index + 2] == '\n')
			return 3;
		return 0;
	}

	void error(size_t index, uint32_t atLine, std::string message) {
		result.errors.push_back(Diagnostic{"", atLine, columnOf(index), std::move(message)});
	}

	void emit(TokenKind kind, size_t start, size_t length, bool plainString = false) {
		result.tokens.push_back(Token{kind, src.substr(start, length), line, columnOf(start), plainString});
	}

	/** emits a token that may span lines (its line/column are those of its start) */
	void emitSpanning(TokenKind kind, size_t start, size_t end, uint32_t startLine, uint32_t startColumn, bool plainString) {
		result.tokens.push_back(Token{kind, src.substr(start, end - start), startLine, startColumn, plainString});
	}

	void skipLineComment() {
		while (pos < src.size() && src[pos] != '\n') {
			if (language == Language::CPP && spliceLength(pos) > 0) { // a line comment ending with a backslash continues on the next line
				pos += spliceLength(pos) - 1;
				newline();
				continue;
			}
			pos++;
		}
	}

	void skipBlockComment() {
		uint32_t startLine = line;
		uint32_t startColumn = columnOf(pos);
		pos += 2;
		while (pos < src.size()) {
			if (src[pos] == '*' && peek(1) == '/') {
				pos += 2;
				return;
			}
			if (src[pos] == '\n')
				newline();
			else
				pos++;
		}
		result.errors.push_back(Diagnostic{"", startLine, startColumn, "unterminated comment"});
	}

	void lexDirective() {
		size_t start = pos;
		uint32_t startLine = line;
		uint32_t startColumn = columnOf(pos);
		size_t end = pos;
		while (pos < src.size()) {
			char c = src[pos];
			if (c == '\n')
				break;
			if (spliceLength(pos) > 0) {
				pos += spliceLength(pos) - 1;
				newline();
				end = pos;
				continue;
			}
			if (c == '/' && peek(1) == '/') {
				skipLineComment();
				break;
			}
			if (c == '/' && peek(1) == '*') {
				skipBlockComment();
				continue;
			}
			if (c == '"') { // strings may contain "//"
				pos++;
				while (pos < src.size() && src[pos] != '"' && src[pos] != '\n') {
					if (src[pos] == '\\' && pos + 1 < src.size() && src[pos + 1] != '\n')
						pos++;
					pos++;
				}
				if (pos < src.size() && src[pos] == '"')
					pos++;
				end = pos;
				continue;
			}
			pos++;
			if (c != ' ' && c != '\t' && c != '\r')
				end = pos;
		}
		emitSpanning(TokenKind::DIRECTIVE, start, end, startLine, startColumn, false);
		atLineStart = true; // the newline that ends the directive is handled by the main loop
	}

	void lexIdentifierOrPrefixedLiteral() {
		size_t start = pos;
		while (pos < src.size() && isIdentifierPart(static_cast<unsigned char>(src[pos]), language))
			pos++;
		std::string_view identifier = src.substr(start, pos - start);
		if (language == Language::CPP && pos < src.size()) {
			static constexpr std::array<std::string_view, 5> RAW_PREFIXES{"R", "u8R", "uR", "UR", "LR"};
			static constexpr std::array<std::string_view, 4> PREFIXES{"u8", "u", "U", "L"};
			if (src[pos] == '"' && std::ranges::find(RAW_PREFIXES, identifier) != RAW_PREFIXES.end()) {
				lexRawString(start);
				return;
			}
			if ((src[pos] == '"' || src[pos] == '\'') && std::ranges::find(PREFIXES, identifier) != PREFIXES.end()) {
				char quote = src[pos];
				lexQuoted(start, pos, quote, quote == '"' ? TokenKind::STRING : TokenKind::CHAR, false);
				return;
			}
		}
		emit(TokenKind::IDENTIFIER, start, pos - start);
	}

	void lexNumber() {
		size_t start = pos;
		pos++;
		while (pos < src.size()) {
			char c = src[pos];
			unsigned char u = static_cast<unsigned char>(c);
			if ((c == '+' || c == '-') && (src[pos - 1] == 'e' || src[pos - 1] == 'E' || src[pos - 1] == 'p' || src[pos - 1] == 'P')) {
				pos++;
			} else if (language == Language::CPP && c == '\'' && pos + 1 < src.size() && isIdentifierPart(static_cast<unsigned char>(src[pos + 1]), language)) {
				pos += 2; // digit separator
			} else if (isIdentifierPart(u, language) || c == '.') {
				pos++;
			} else {
				break;
			}
		}
		emit(TokenKind::NUMBER, start, pos - start);
	}

	/** start: first character of the token (prefix); quoteIndex: the opening quote */
	void lexQuoted(size_t start, size_t quoteIndex, char quote, TokenKind kind, bool plainCandidate) {
		uint32_t startColumn = columnOf(start);
		pos = quoteIndex + 1;
		while (pos < src.size()) {
			char c = src[pos];
			if (c == quote) {
				pos++;
				emit(kind, start, pos - start, plainCandidate && start == quoteIndex);
				return;
			}
			if (c == '\n')
				break;
			if (c == '\\' && pos + 1 < src.size()) {
				if (language == Language::CPP && spliceLength(pos) > 0) {
					pos += spliceLength(pos) - 1;
					newline();
					continue;
				}
				pos += 2;
				continue;
			}
			pos++;
		}
		result.errors.push_back(Diagnostic{"", line, startColumn, kind == TokenKind::STRING ? "unterminated string literal" : "unterminated character literal"});
	}

	void lexRawString(size_t start) {
		uint32_t startLine = line;
		uint32_t startColumn = columnOf(start);
		size_t open = pos; // at '"'
		size_t paren = src.find('(', open + 1);
		if (paren == std::string_view::npos || paren - open - 1 > 16) {
			error(start, startLine, "malformed raw string literal");
			pos = open + 1;
			return;
		}
		std::string terminator = ")" + std::string(src.substr(open + 1, paren - open - 1)) + "\"";
		size_t close = src.find(terminator, paren + 1);
		if (close == std::string_view::npos) {
			error(start, startLine, "unterminated raw string literal");
			while (pos < src.size()) {
				if (src[pos] == '\n')
					newline();
				else
					pos++;
			}
			return;
		}
		size_t end = close + terminator.size();
		while (pos < end) {
			if (src[pos] == '\n')
				newline();
			else
				pos++;
		}
		emitSpanning(TokenKind::STRING, start, end, startLine, startColumn, false);
	}

	void lexTextBlock(size_t start) {
		uint32_t startLine = line;
		uint32_t startColumn = columnOf(start);
		pos = start + 3;
		while (pos < src.size()) {
			if (src[pos] == '\\' && pos + 1 < src.size()) {
				if (src[pos + 1] == '\n') {
					pos++;
					newline();
				} else {
					pos += 2;
				}
				continue;
			}
			if (src.substr(pos, 3) == "\"\"\"") {
				pos += 3;
				emitSpanning(TokenKind::STRING, start, pos, startLine, startColumn, false);
				return;
			}
			if (src[pos] == '\n')
				newline();
			else
				pos++;
		}
		result.errors.push_back(Diagnostic{"", startLine, startColumn, "unterminated text block"});
	}

	std::string_view src;
	Language language;
	size_t pos = 0;
	uint32_t line = 1;
	size_t lineStart = 0;
	bool atLineStart = true;
	LexResult result;
};

} // namespace

LexResult lex(std::string_view source, Language language) {
	return Lexer(source, language).run();
}

std::string_view directiveName(const Token& directive) noexcept {
	std::string_view text = directive.text;
	size_t i = 1; // after '#'
	while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\\' || text[i] == '\r' || text[i] == '\n'))
		i++;
	size_t start = i;
	while (i < text.size() && isIdentifierPart(static_cast<unsigned char>(text[i]), Language::CPP))
		i++;
	return text.substr(start, i - start);
}

bool isCppKeyword(std::string_view name) noexcept {
	static constexpr std::string_view KEYWORDS[] = {"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case",
		"catch", "char", "char16_t", "char32_t", "char8_t", "class", "co_await", "co_return", "co_yield", "compl", "concept", "const", "const_cast",
		"constexpr", "constinit", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
		"extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
		"nullptr", "operator", "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast", "requires", "return", "short", "signed",
		"sizeof", "static", "static_assert", "static_cast", "struct", "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef",
		"typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"};
	return std::ranges::find(KEYWORDS, name) != std::end(KEYWORDS);
}

} // namespace aion::gameserver::tools::regscan
