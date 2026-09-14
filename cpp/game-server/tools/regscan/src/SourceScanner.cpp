#include "SourceScanner.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>

namespace aion::gameserver::tools::regscan {

namespace {

struct MarkerInfo {
	std::string_view name;
	MarkerKind kind;
	std::string_view category;
};

constexpr std::array<MarkerInfo, 8> MARKERS{{
	{"AION_AI", MarkerKind::AI, "ai"},
	{"AION_INSTANCE_HANDLER", MarkerKind::INSTANCE_HANDLER, "instance"},
	{"AION_ZONE_HANDLER", MarkerKind::ZONE_HANDLER, "zone"},
	{"AION_QUEST_HANDLER", MarkerKind::QUEST_HANDLER, "quest"},
	{"AION_ADMIN_COMMAND", MarkerKind::ADMIN_COMMAND, "admincommands"},
	{"AION_PLAYER_COMMAND", MarkerKind::PLAYER_COMMAND, "playercommands"},
	{"AION_CONSOLE_COMMAND", MarkerKind::CONSOLE_COMMAND, "consolecommands"},
	{"AION_CLIENT_PACKET", MarkerKind::CLIENT_PACKET, ""},
}};

constexpr std::string_view DETAIL_COMMAND_MACRO = "AION_DETAIL_COMMAND";
constexpr std::string_view HANDLERS_PREFIX = "aion/gameserver/handlers/";
constexpr std::string_view CLIENT_PACKETS_DIR = "aion/gameserver/network/aion/clientpackets";

const MarkerInfo* findMarker(std::string_view name) noexcept {
	auto it = std::ranges::find(MARKERS, name, &MarkerInfo::name);
	return it == MARKERS.end() ? nullptr : &*it;
}

std::vector<std::string> splitNamespace(std::string_view ns) {
	std::vector<std::string> segments;
	while (!ns.empty()) {
		size_t sep = ns.find("::");
		segments.emplace_back(ns.substr(0, sep));
		ns = sep == std::string_view::npos ? std::string_view() : ns.substr(sep + 2);
	}
	return segments;
}

std::string joinNamespace(const std::vector<std::string>& segments) {
	std::string result;
	for (const std::string& segment : segments) {
		if (!result.empty())
			result += "::";
		result += segment;
	}
	return result;
}

bool isHeaderFile(std::string_view relPath) noexcept {
	return relPath.ends_with(".h");
}

class Scanner {
public:
	Scanner(std::string_view file, std::string_view relPath, std::string_view content, FileRole role)
		: file(file), relPath(relPath), role(role), expected(splitNamespace(expectedNamespace(relPath))), lexed(lex(content, Language::CPP)), t(lexed.tokens) {
		for (Diagnostic& d : lexed.errors) {
			d.file = std::string(file);
			result.errors.push_back(std::move(d));
		}
	}

	FileScan run() {
		size_t i = 0;
		while (i < t.size()) {
			const Token& tok = t[i];
			switch (tok.kind) {
				case TokenKind::DIRECTIVE:
					handleDirective(tok);
					i++;
					break;
				case TokenKind::IDENTIFIER:
					i = handleIdentifier(i);
					break;
				case TokenKind::PUNCT:
					handlePunct(tok);
					i++;
					break;
				default:
					if (otherDepth == 0)
						checkInsidePackage(tok);
					i++;
					break;
			}
		}
		if (!blocks.empty()) {
			uint32_t line = t.empty() ? 1 : t.back().line;
			error(line, 1, "unbalanced '{': the file ends inside a block");
		}
		if (ppDepth > 0)
			error(t.empty() ? 1 : t.back().line, 1, "unterminated #if block");
		return std::move(result);
	}

private:
	struct Block {
		bool isNamespace;
		size_t segments; // namespace segments this block added
	};

	void error(uint32_t line, uint32_t column, std::string message) { result.errors.push_back(Diagnostic{std::string(file), line, column, std::move(message)}); }
	void error(const Token& tok, std::string message) { error(tok.line, tok.column, std::move(message)); }

	void handleDirective(const Token& tok) {
		std::string_view name = directiveName(tok);
		if (name == "if" || name == "ifdef" || name == "ifndef") {
			ppDepth++;
		} else if (name == "endif") {
			if (ppDepth == 0)
				error(tok, "#endif without #if");
			else
				ppDepth--;
		}
		LexResult inner = lex(tok.text.substr(1), Language::CPP);
		for (const Token& innerToken : inner.tokens) {
			if (innerToken.kind == TokenKind::IDENTIFIER && (findMarker(innerToken.text) || innerToken.text == DETAIL_COMMAND_MACRO)) {
				error(tok, "registration markers must not appear in preprocessor directives (" + std::string(innerToken.text) + ")");
				break;
			}
		}
	}

	size_t handleIdentifier(size_t i) {
		const Token& tok = t[i];
		if (tok.text == "using" && i + 1 < t.size() && t[i + 1].isIdentifier("namespace")) {
			if (role == FileRole::HANDLER)
				error(tok, "'using namespace' is not allowed in handler files (unity builds): use the category prelude or qualified names");
			else if (otherDepth == 0)
				checkInsidePackage(tok);
			return i + 2; // the name that follows is no namespace declaration
		}
		if (findMarker(tok.text) || tok.text == DETAIL_COMMAND_MACRO)
			return parseMarker(i);
		if (otherDepth == 0) {
			if (tok.text == "inline" && i + 1 < t.size() && t[i + 1].isIdentifier("namespace"))
				return i + 1;
			if (tok.text == "namespace")
				return parseNamespace(i);
			checkInsidePackage(tok);
			if (tok.text == "static" && role == FileRole::HANDLER)
				error(tok, "namespace-scope 'static' is not allowed in handler files (unity builds): use a private member function or a static member");
			bool afterEnum = i > 0 && t[i - 1].isIdentifier("enum"); // "enum class X" is detected at "enum"
			if ((tok.text == "class" || tok.text == "struct" || tok.text == "union" || tok.text == "enum") && !afterEnum)
				detectTypeDefinition(i);
		}
		return i + 1;
	}

	void handlePunct(const Token& tok) {
		if (tok.isPunct("{")) {
			blocks.push_back(Block{false, 0});
			otherDepth++;
		} else if (tok.isPunct("}")) {
			if (blocks.empty()) {
				error(tok, "unbalanced '}'");
				return;
			}
			Block block = blocks.back();
			blocks.pop_back();
			if (block.isNamespace)
				namespaceSegments.resize(namespaceSegments.size() - block.segments);
			else
				otherDepth--;
		} else if (otherDepth == 0 && !tok.isPunct(";")) {
			checkInsidePackage(tok);
		}
	}

	/** Handler files: every declaration at namespace scope must be in the package namespace (reported once per file). */
	void checkInsidePackage(const Token& tok) {
		if (role != FileRole::HANDLER || reportedOutsidePackage || namespaceSegments == expected)
			return;
		reportedOutsidePackage = true;
		std::string where = namespaceSegments.empty() ? "the global namespace" : "namespace " + joinNamespace(namespaceSegments);
		error(tok, "handler code must be inside namespace " + joinNamespace(expected) + " (the file's directory), found it in " + where);
	}

	size_t parseNamespace(size_t i) {
		const Token& keyword = t[i];
		size_t j = i + 1;
		std::vector<std::string> segments;
		if (j < t.size() && t[j].isPunct("{")) {
			if (role == FileRole::HANDLER)
				error(keyword, "anonymous namespaces are not allowed in handler files (unity builds): use private members of the class");
			segments.emplace_back("{anonymous}");
		} else {
			while (j < t.size() && t[j].kind == TokenKind::IDENTIFIER) {
				if (t[j].text == "inline") {
					j++;
					continue;
				}
				segments.emplace_back(t[j].text);
				j++;
				if (j < t.size() && t[j].isPunct("::")) {
					j++;
					continue;
				}
				break;
			}
			if (j < t.size() && t[j].isPunct("=") && segments.size() == 1) { // namespace alias
				checkInsidePackage(keyword);
				while (j < t.size() && !t[j].isPunct(";"))
					j++;
				return j + 1;
			}
		}
		if (segments.empty() || j >= t.size() || !t[j].isPunct("{")) {
			error(keyword, "malformed namespace declaration");
			return j;
		}
		blocks.push_back(Block{true, segments.size()});
		namespaceSegments.insert(namespaceSegments.end(), segments.begin(), segments.end());
		if (role == FileRole::HANDLER) {
			bool isPrefix = namespaceSegments.size() <= expected.size() && std::equal(namespaceSegments.begin(), namespaceSegments.end(), expected.begin());
			if (!isPrefix && segments.front() != "{anonymous}")
				error(keyword, "namespace " + joinNamespace(namespaceSegments) + " does not match the file's directory: handler files use only namespace " +
					joinNamespace(expected));
			if (!isPrefix)
				reportedOutsidePackage = true; // one error per problem
		}
		return j + 1;
	}

	void detectTypeDefinition(size_t i) {
		size_t j = i + 1;
		if (t[i].text == "enum" && j < t.size() && (t[j].isIdentifier("class") || t[j].isIdentifier("struct")))
			j++;
		while (j + 1 < t.size() && t[j].isPunct("[") && t[j + 1].isPunct("[")) { // attributes
			while (j < t.size() && !(t[j].isPunct("]") && j + 1 < t.size() && t[j + 1].isPunct("]")))
				j++;
			j += 2;
		}
		if (j >= t.size() || t[j].kind != TokenKind::IDENTIFIER || isCppKeyword(t[j].text))
			return;
		const Token& name = t[j];
		j++;
		if (j < t.size() && t[j].isIdentifier("final"))
			j++;
		if (j >= t.size())
			return;
		bool definition = false;
		if (t[j].isPunct("{")) {
			definition = true;
		} else if (t[j].isPunct(":")) {
			int parens = 0;
			for (size_t k = j + 1; k < t.size(); k++) {
				if (t[k].isPunct("(")) {
					parens++;
				} else if (t[k].isPunct(")")) {
					parens--;
				} else if (parens == 0 && t[k].isPunct(";")) {
					break;
				} else if (parens == 0 && t[k].isPunct("{")) {
					definition = true;
					break;
				}
			}
		}
		if (definition)
			result.types.push_back(TypeDefinition{joinNamespace(namespaceSegments), std::string(name.text), std::string(file), name.line, name.column});
	}

	static bool isAllDigits(std::string_view s) noexcept {
		return !s.empty() && std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
	}

	size_t parseMarker(size_t i) {
		const Token& tok = t[i];
		std::string name(tok.text);
		auto skipLine = [&] {
			size_t j = i + 1;
			while (j < t.size() && t[j].line == tok.line && !t[j].isPunct(";"))
				j++;
			if (j < t.size() && t[j].isPunct(";") && t[j].line == tok.line)
				j++;
			return j;
		};
		auto fail = [&](const std::string& message) {
			error(tok, message);
			return skipLine();
		};
		const MarkerInfo* info = findMarker(tok.text);
		if (info == nullptr)
			return fail(name + " is internal: use AION_ADMIN_COMMAND, AION_PLAYER_COMMAND or AION_CONSOLE_COMMAND");
		if (otherDepth > 0)
			return fail(name + " must be at namespace scope, directly in the package namespace (not inside a class, function or other block)");
		if (ppDepth > 0)
			return fail(name + " must not be inside an #if block");
		if (role == FileRole::HANDLER && isHeaderFile(relPath))
			return fail(name + " belongs in the handler's .cpp file, not in a header");
		if (role == FileRole::HANDLER && info->kind == MarkerKind::CLIENT_PACKET)
			return fail("AION_CLIENT_PACKET is only allowed in " + std::string(CLIENT_PACKETS_DIR));
		if (role == FileRole::CLIENT_PACKET && info->kind != MarkerKind::CLIENT_PACKET)
			return fail(name + " is only allowed in handler files (aion/gameserver/handlers/" + std::string(info->category) + ")");
		if (i > 0 && t[i - 1].line == tok.line)
			return fail(name + " must start its own line");

		// syntax: NAME ( literal , ... ) ;  on one line
		size_t j = i + 1;
		std::string usage = usageOf(info->kind);
		if (j >= t.size() || !t[j].isPunct("(") || t[j].line != tok.line)
			return fail("malformed marker, expected " + usage + " on one line");
		j++;
		std::vector<const Token*> args;
		while (true) {
			if (j >= t.size() || t[j].line != tok.line)
				return fail("malformed marker, expected " + usage + " on one line");
			const Token& arg = t[j];
			if (arg.kind != TokenKind::IDENTIFIER && arg.kind != TokenKind::NUMBER && arg.kind != TokenKind::STRING)
				return fail("marker arguments must be literals (a class name, a plain string literal or a decimal int), expected " + usage);
			args.push_back(&arg);
			j++;
			if (j < t.size() && t[j].isPunct(",") && t[j].line == tok.line) {
				j++;
				continue;
			}
			if (j < t.size() && t[j].isPunct(")") && t[j].line == tok.line) {
				j++;
				break;
			}
			return fail("malformed marker, expected " + usage + " on one line");
		}
		if (j >= t.size() || !t[j].isPunct(";") || t[j].line != tok.line)
			return fail("malformed marker, expected " + usage + " on one line (ending with ';')");
		j++;
		if (j < t.size() && t[j].line == tok.line) {
			error(t[j], "nothing but a comment may follow " + name + " on its line");
			return j;
		}

		Marker marker{info->kind, "", "", std::nullopt, joinNamespace(namespaceSegments), std::string(file), std::string(relPath), tok.line, tok.column};
		if (!validateArguments(*info, args, marker, usage))
			return j;

		std::string expectedNs = joinNamespace(expected);
		if (marker.ns != expectedNs) {
			error(tok, name + " must be in namespace " + expectedNs + " (the file's directory), found " + (marker.ns.empty() ? "the global namespace" : marker.ns));
			return j;
		}
		if (role == FileRole::HANDLER) {
			std::string_view category;
			if (relPath.starts_with(HANDLERS_PREFIX)) {
				std::string_view rest = relPath.substr(HANDLERS_PREFIX.size());
				size_t slash = rest.find('/');
				if (slash != std::string_view::npos)
					category = rest.substr(0, slash);
			}
			if (category != info->category) {
				error(tok, name + " belongs in aion/gameserver/handlers/" + std::string(info->category) + "/..., not in " + std::string(relPath));
				return j;
			}
		} else {
			size_t slash = relPath.rfind('/');
			if (slash == std::string_view::npos || relPath.substr(0, slash) != CLIENT_PACKETS_DIR) {
				error(tok, "AION_CLIENT_PACKET belongs directly in " + std::string(CLIENT_PACKETS_DIR) + ", not in " + std::string(relPath));
				return j;
			}
		}
		result.markers.push_back(std::move(marker));
		return j;
	}

	static std::string usageOf(MarkerKind kind) {
		switch (kind) {
			case MarkerKind::AI:
				return "AION_AI(Class, \"name\");";
			case MarkerKind::INSTANCE_HANDLER:
				return "AION_INSTANCE_HANDLER(Class, mapId);";
			case MarkerKind::ZONE_HANDLER:
				return "AION_ZONE_HANDLER(Class, \"ZONE_1 ZONE_2\"[, questId]);";
			case MarkerKind::QUEST_HANDLER:
				return "AION_QUEST_HANDLER(Class, questId);";
			case MarkerKind::ADMIN_COMMAND:
				return "AION_ADMIN_COMMAND(Class);";
			case MarkerKind::PLAYER_COMMAND:
				return "AION_PLAYER_COMMAND(Class);";
			case MarkerKind::CONSOLE_COMMAND:
				return "AION_CONSOLE_COMMAND(Class);";
			case MarkerKind::CLIENT_PACKET:
				return "AION_CLIENT_PACKET(Class);";
		}
		return {};
	}

	bool validateArguments(const MarkerInfo& info, const std::vector<const Token*>& args, Marker& marker, const std::string& usage) {
		enum class Arg { CLASS, TEXT, NUMBER };
		std::vector<Arg> shape{Arg::CLASS};
		size_t optionalFrom = 1;
		switch (info.kind) {
			case MarkerKind::AI:
				shape = {Arg::CLASS, Arg::TEXT};
				optionalFrom = 2;
				break;
			case MarkerKind::INSTANCE_HANDLER:
			case MarkerKind::QUEST_HANDLER:
				shape = {Arg::CLASS, Arg::NUMBER};
				optionalFrom = 2;
				break;
			case MarkerKind::ZONE_HANDLER:
				shape = {Arg::CLASS, Arg::TEXT, Arg::NUMBER};
				optionalFrom = 2;
				break;
			default:
				break;
		}
		if (args.size() < optionalFrom || args.size() > shape.size()) {
			error(*args.front(), "wrong number of arguments, expected " + usage);
			return false;
		}
		for (size_t k = 0; k < args.size(); k++) {
			const Token& arg = *args[k];
			switch (shape[k]) {
				case Arg::CLASS:
					if (arg.kind != TokenKind::IDENTIFIER || isCppKeyword(arg.text)) {
						error(arg, "the first argument must be the handler's class name, expected " + usage);
						return false;
					}
					marker.className = std::string(arg.text);
					break;
				case Arg::TEXT: {
					if (arg.kind != TokenKind::STRING || !arg.plainString) {
						error(arg, "expected a plain string literal (no prefix, not raw), expected " + usage);
						return false;
					}
					std::string_view content = arg.text.substr(1, arg.text.size() - 2);
					if (content.empty()) {
						error(arg, "the string must not be empty");
						return false;
					}
					if (std::ranges::any_of(content, [](char c) { return c == '\\' || c < 0x20 || c > 0x7E; })) {
						error(arg, "the string must consist of printable ASCII characters without escapes");
						return false;
					}
					if (info.kind == MarkerKind::ZONE_HANDLER &&
						(content.front() == ' ' || content.back() == ' ' || content.find("  ") != std::string_view::npos)) {
						error(arg, "zone names must be separated by single spaces");
						return false;
					}
					marker.text = std::string(content);
					break;
				}
				case Arg::NUMBER: {
					int32_t value = 0;
					bool valid = arg.kind == TokenKind::NUMBER && isAllDigits(arg.text) && (arg.text.size() == 1 || arg.text[0] != '0');
					if (valid) {
						auto [ptr, ec] = std::from_chars(arg.text.data(), arg.text.data() + arg.text.size(), value);
						valid = ec == std::errc() && ptr == arg.text.data() + arg.text.size();
					}
					if (!valid) {
						error(arg, "expected a decimal int literal (no sign, suffix, separator or leading zero) in the int32 range, expected " + usage);
						return false;
					}
					if (info.kind != MarkerKind::ZONE_HANDLER && value <= 0) {
						error(arg, std::string(info.kind == MarkerKind::QUEST_HANDLER ? "the quest id" : "the map id") + " must be greater than 0");
						return false;
					}
					marker.number = value;
					break;
				}
			}
		}
		return true;
	}

	std::string_view file;
	std::string_view relPath;
	FileRole role;
	std::vector<std::string> expected;
	LexResult lexed;
	const std::vector<Token>& t;
	FileScan result;
	std::vector<Block> blocks;
	std::vector<std::string> namespaceSegments;
	size_t otherDepth = 0;
	int ppDepth = 0;
	bool reportedOutsidePackage = false;
};

} // namespace

std::string_view markerName(MarkerKind kind) noexcept {
	return std::ranges::find(MARKERS, kind, &MarkerInfo::kind)->name;
}

std::string_view markerCategory(MarkerKind kind) noexcept {
	return std::ranges::find(MARKERS, kind, &MarkerInfo::kind)->category;
}

std::string expectedNamespace(std::string_view relPath) {
	size_t slash = relPath.rfind('/');
	std::string_view dir = slash == std::string_view::npos ? std::string_view() : relPath.substr(0, slash);
	std::string result;
	while (!dir.empty()) {
		size_t sep = dir.find('/');
		std::string_view segment = dir.substr(0, sep);
		if (!result.empty())
			result += "::";
		result += segment;
		if (isCppKeyword(segment))
			result += '_';
		dir = sep == std::string_view::npos ? std::string_view() : dir.substr(sep + 1);
	}
	return result;
}

FileScan scanCppFile(std::string_view file, std::string_view relPath, std::string_view content, FileRole role) {
	return Scanner(file, relPath, content, role).run();
}

} // namespace aion::gameserver::tools::regscan
