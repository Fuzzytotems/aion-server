// SQL literal parity of the DAO ports (P4-14, handlers-and-porting-plan.md §3.2 "SQL literal parity"): every string that a Java DAO builds
// only from literals (joined with +, text blocks resolved) and that contains an SQL statement keyword occurs byte-identically among the string
// values of the C++ port (adjacent C++ literals concatenated). Bind parity: per DAO file, the ordered sequence of JDBC setter calls with a
// literal parameter index (setInt(2, ...)) and the ordered sequence of getter calls with a literal column (getString("name")) are the same in
// Java and C++, so a swapped index, a setter of another type or a reordered bind fails without the database (two same-typed values swapped
// under unchanged indexes are left to the round-trip tests). Reads the Java tree (realdata).

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <map>
#include <filesystem>
#include <optional>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace aion::gameserver::dao::test {
namespace {

const std::filesystem::path JAVA_DAO_DIR = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "src/com/aionemu/gameserver/dao";
const std::filesystem::path CPP_DAO_DIR = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "../cpp/game-server/src/aion/gameserver/dao";

std::string readText(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	std::string text = content.str();
	std::erase(text, '\r'); // a checkout with CRLF line ends (Java text blocks normalize them to \n)
	return text;
}

/** Resolves the escapes of a Java or C++ string literal body (the DAO literals only use \\, \", \n and \t) */
std::string unescape(std::string_view body) {
	std::string value;
	for (size_t i = 0; i < body.size(); ++i) {
		if (body[i] != '\\' || i + 1 == body.size()) {
			value += body[i];
			continue;
		}
		char next = body[++i];
		value += next == 'n' ? '\n' : next == 't' ? '\t' : next;
	}
	return value;
}

/** Java text block: the common indentation of the content lines and the closing delimiter line is removed, trailing spaces are stripped */
std::string textBlock(std::string_view raw) {
	std::string_view content = raw.substr(3, raw.size() - 6);
	content.remove_prefix(content.find('\n') + 1);
	std::vector<std::string_view> lines;
	for (size_t start = 0;;) {
		size_t end = content.find('\n', start);
		lines.push_back(content.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
		if (end == std::string_view::npos)
			break;
		start = end + 1;
	}
	size_t indent = std::string::npos;
	for (size_t i = 0; i < lines.size(); ++i) {
		std::string_view line = lines[i];
		size_t first = line.find_first_not_of(" \t");
		if (first == std::string_view::npos && i + 1 != lines.size())
			continue;
		indent = std::min(indent, first == std::string_view::npos ? line.size() : first);
	}
	std::string value;
	for (size_t i = 0; i < lines.size(); ++i) {
		std::string_view line = lines[i].size() > indent ? lines[i].substr(indent) : std::string_view();
		line = line.substr(0, line.find_last_not_of(" \t") + 1);
		value += line;
		if (i + 1 != lines.size())
			value += '\n';
	}
	return unescape(value);
}

/**
 * The values of the maximal runs of string literals: joined by + (Java) or by adjacency and + (C++); any other token between two literals
 * ends the run. Comments and character literals are skipped.
 */
std::vector<std::string> literalRuns(std::string_view src, bool java) {
	std::vector<std::string> runs;
	std::optional<std::string> current;
	bool joined = false;
	auto finish = [&] {
		if (current)
			runs.push_back(*current);
		current.reset();
		joined = false;
	};
	for (size_t i = 0; i < src.size();) {
		char c = src[i];
		if (src.substr(i, 2) == "//") {
			i = src.find('\n', i);
			if (i == std::string_view::npos)
				break;
		} else if (src.substr(i, 2) == "/*") {
			i = src.find("*/", i + 2) + 2;
		} else if (std::isspace(static_cast<unsigned char>(c))) {
			++i;
		} else if (java && src.substr(i, 3) == "\"\"\"") {
			size_t end = src.find("\"\"\"", i + 3) + 3;
			std::string value = textBlock(src.substr(i, end - i));
			if (current && joined)
				*current += value;
			else {
				finish();
				current = value;
			}
			joined = false;
			i = end;
		} else if (c == '"') {
			size_t end = i + 1;
			while (src[end] != '"')
				end += src[end] == '\\' ? 2 : 1;
			std::string value = unescape(src.substr(i + 1, end - i - 1));
			if (current && (joined || !java))
				*current += value;
			else {
				finish();
				current = value;
			}
			joined = false;
			i = end + 1;
		} else if (c == '\'') {
			size_t end = i + 1;
			while (src[end] != '\'')
				end += src[end] == '\\' ? 2 : 1;
			finish();
			i = end + 1;
		} else if (c == '+') {
			if (current)
				joined = true;
			++i;
		} else {
			finish();
			++i;
		}
	}
	finish();
	return runs;
}

TEST(DaoSqlParityTest, EveryJavaSqlLiteralOccursInTheCppPort) {
	const std::regex sql(R"(\b(SELECT|INSERT|UPDATE|DELETE|REPLACE|SET @)\b)", std::regex::icase);
	int checked = 0;
	for (const auto& entry : std::filesystem::directory_iterator(JAVA_DAO_DIR)) {
		if (entry.path().extension() != ".java")
			continue;
		const std::filesystem::path cppFile = CPP_DAO_DIR / (entry.path().stem().string() + ".cpp");
		ASSERT_TRUE(std::filesystem::exists(cppFile)) << cppFile;
		std::vector<std::string> cppRuns = literalRuns(readText(cppFile), false);
		std::unordered_set<std::string> cppValues(cppRuns.begin(), cppRuns.end());
		for (const std::string& value : literalRuns(readText(entry.path()), true)) {
			if (!std::regex_search(value, sql) || value.starts_with("Could") || value.starts_with("Can") || value.starts_with("Error") ||
				value.starts_with("Failed"))
				continue;
			++checked;
			EXPECT_TRUE(cppValues.contains(value)) << entry.path().filename() << ": " << value;
		}
	}
	EXPECT_GT(checked, 250) << "the DAO SQL literals were found";
}

/** The source without // and block comments; string literals (Java text blocks included) and character literals are kept */
std::string withoutComments(std::string_view src) {
	std::string out;
	for (size_t i = 0; i < src.size();) {
		if (src.substr(i, 3) == "\"\"\"") {
			size_t end = src.find("\"\"\"", i + 3);
			end = end == std::string_view::npos ? src.size() : end + 3;
			out.append(src.substr(i, end - i));
			i = end;
		} else if (src[i] == '"' || src[i] == '\'') {
			const char quote = src[i];
			size_t end = i + 1;
			while (end < src.size() && src[end] != quote)
				end += src[end] == '\\' ? 2 : 1;
			end = std::min(end + 1, src.size());
			out.append(src.substr(i, end - i));
			i = end;
		} else if (src.substr(i, 2) == "//") {
			i = src.find('\n', i);
			if (i == std::string_view::npos)
				break;
		} else if (src.substr(i, 2) == "/*") {
			size_t end = src.find("*/", i + 2);
			i = end == std::string_view::npos ? src.size() : end + 2;
			out += ' ';
		} else {
			out += src[i++];
		}
	}
	return out;
}

bool isIdentifierChar(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

size_t skipSpaces(std::string_view src, size_t i) {
	while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i])))
		++i;
	return i;
}

/** A literal argument at `i`: a string literal (with its quotes) or a decimal number followed by ',' or ')'; empty for an expression */
std::string literalArgument(std::string_view src, size_t& i) {
	i = skipSpaces(src, i);
	const size_t start = i;
	if (i < src.size() && src[i] == '"') {
		for (++i; i < src.size() && src[i] != '"'; i += src[i] == '\\' ? 2 : 1) {
		}
		++i;
	} else {
		while (i < src.size() && std::isdigit(static_cast<unsigned char>(src[i])))
			++i;
	}
	if (i > src.size())
		return {};
	std::string value(src.substr(start, i - start));
	i = skipSpaces(src, i);
	if (value.empty() || i >= src.size() || (src[i] != ',' && src[i] != ')'))
		return {};
	return value;
}

struct JdbcCalls {
	std::vector<std::string> setters; // "setInt 2"
	std::vector<std::string> getters; // "get \"name\"" (the getter type is not compared: C++ reads nullable columns with getObject<T>)
};

/**
 * The JDBC calls `.name(literal` or `->name(literal` in source order (C++ template arguments before the parenthesis allowed), and the C++
 * detail::getUsedIDs(log, query, "column", message) helper as the getInt(column) of the eight Java getUsedIDs bodies it replaces.
 */
JdbcCalls jdbcCalls(std::string_view source) {
	static const std::unordered_set<std::string> setterNames{"setInt", "setLong", "setString", "setFloat", "setDouble", "setBoolean", "setTimestamp",
		"setDate", "setBytes", "setByte", "setShort", "setNull", "setObject"};
	static const std::unordered_set<std::string> getterNames{"getInt", "getLong", "getString", "getFloat", "getDouble", "getBoolean", "getTimestamp",
		"getDate", "getBytes", "getByte", "getShort", "getObject"};
	const std::string src = withoutComments(source);
	JdbcCalls calls;
	for (size_t i = 0; i < src.size(); ++i) {
		if (src.compare(i, 11, "getUsedIDs(") == 0 && (i == 0 || !isIdentifierChar(src[i - 1]))) {
			size_t pos = i + 11;
			int commas = 0;
			while (pos < src.size() && commas < 2 && src[pos] != ')' && src[pos] != ';')
				commas += src[pos++] == ',' ? 1 : 0;
			if (commas == 2) {
				std::string column = literalArgument(src, pos);
				if (!column.empty())
					calls.getters.push_back("get " + column);
			}
			continue;
		}
		size_t name = std::string::npos;
		if (src[i] == '.')
			name = i + 1;
		else if (src.compare(i, 2, "->") == 0)
			name = i + 2;
		if (name == std::string::npos)
			continue;
		name = skipSpaces(src, name);
		size_t end = name;
		while (end < src.size() && isIdentifierChar(src[end]))
			++end;
		const std::string identifier = src.substr(name, end - name);
		const bool setter = setterNames.contains(identifier);
		if (!setter && !getterNames.contains(identifier))
			continue;
		size_t pos = skipSpaces(src, end);
		if (pos < src.size() && src[pos] == '<') { // getObject<std::vector<uint8_t>>(
			const size_t open = src.find('(', pos);
			const size_t close = src.find(')', pos);
			if (open == std::string::npos || close < open)
				continue;
			pos = open;
		}
		if (pos >= src.size() || src[pos] != '(')
			continue;
		++pos;
		std::string argument = literalArgument(src, pos);
		if (argument.empty())
			continue;
		if (setter)
			calls.setters.push_back(identifier + " " + argument);
		else
			calls.getters.push_back("get " + argument);
	}
	return calls;
}

std::string joined(const std::vector<std::string>& values) {
	std::string text;
	for (const std::string& value : values)
		text += value + "\n";
	return text;
}

TEST(DaoSqlParityTest, JdbcBindIndexesAndColumnsMatchTheJavaOrder) {
	// Java-only getter reads that the port has once instead of twice: LegionDAO.loadLegion(int) and loadLegion(String) share the C++ helper
	// readLegion for the ten columns after `new Legion(id, name)`
	const std::map<std::string, std::vector<std::string>, std::less<>> javaOnlyGetters{
		{"LegionDAO",
			{"get \"level\"", "get \"contribution_points\"", "get \"deputy_permission\"", "get \"centurion_permission\"", "get \"legionary_permission\"",
				"get \"volunteer_permission\"", "get \"disband_time\"", "get \"occupied_legion_dominion\"", "get \"last_legion_dominion\"",
				"get \"current_legion_dominion\""}},
	};
	// files whose getter order differs for a stated reason: only the multiset of their getters is compared
	const std::unordered_set<std::string> getterOrderDiffers{
		"LegionDAO", // readLegion is defined before its callers; loadLegionEmblem reads emblem_data into a local before the setEmblem arguments
	};
	int files = 0;
	size_t setters = 0;
	size_t getters = 0;
	for (const auto& entry : std::filesystem::directory_iterator(JAVA_DAO_DIR)) {
		if (entry.path().extension() != ".java")
			continue;
		const std::string dao = entry.path().stem().string();
		const std::string cppText = readText(CPP_DAO_DIR / (dao + ".cpp"));
		if (dao == "CustomInstancePlayerModelEntryDAO") { // unported (no PlayerModelEntry header, P5-13)
			EXPECT_NE(cppText.find("AION_UNPORTED"), std::string::npos) << dao << " is ported now: compare it too";
			continue;
		}
		JdbcCalls java = jdbcCalls(readText(entry.path()));
		JdbcCalls cpp = jdbcCalls(cppText);
		++files;
		setters += java.setters.size();
		getters += java.getters.size();
		EXPECT_EQ(joined(cpp.setters), joined(java.setters)) << dao << ": setter indexes in source order";
		if (auto skipped = javaOnlyGetters.find(dao); skipped != javaOnlyGetters.end()) {
			for (const std::string& getter : skipped->second) {
				auto found = std::ranges::find(java.getters, getter);
				ASSERT_NE(found, java.getters.end()) << dao << ": " << getter;
				java.getters.erase(found);
			}
		}
		if (getterOrderDiffers.contains(dao)) {
			std::ranges::sort(java.getters);
			std::ranges::sort(cpp.getters);
		}
		EXPECT_EQ(joined(cpp.getters), joined(java.getters)) << dao << ": getter columns in source order";
	}
	EXPECT_EQ(files, 55);
	EXPECT_GT(setters, 500u) << "the Java setter calls were found";
	EXPECT_GT(getters, 400u) << "the Java getter calls were found";
}

} // namespace
} // namespace aion::gameserver::dao::test
