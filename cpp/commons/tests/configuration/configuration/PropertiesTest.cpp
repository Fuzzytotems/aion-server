#include "aion/commons/configuration/Properties.h"

#include <regex>
#include <sstream>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"

using namespace aion::commons;
using configuration::Properties;

namespace {

/** Loads bytes like Java's Properties.load(InputStream). */
Properties loadLatin1(std::string_view bytes) {
	Properties p;
	std::istringstream in{std::string(bytes)};
	p.load(in);
	return p;
}

std::string get(const Properties& p, std::string_view key) {
	return p.getProperty(key).value_or("<missing>");
}

} // namespace

TEST(PropertiesTest, KeyValueSeparators) {
	Properties p = loadLatin1("a=1\n"
	                          "b:2\n"
	                          "c 3\n"
	                          "d\t4\n"
	                          "e\f5\n"
	                          "f = 6\n"
	                          "g  =  7  \n"
	                          "h=  = 8\n"
	                          "i =: 9\n"
	                          "j:=10\n"
	                          "k\n"
	                          "l=\n"
	                          "=13\n"
	                          " : 14\n");
	EXPECT_EQ(get(p, "a"), "1");
	EXPECT_EQ(get(p, "b"), "2");
	EXPECT_EQ(get(p, "c"), "3");
	EXPECT_EQ(get(p, "d"), "4");
	EXPECT_EQ(get(p, "e"), "5");
	EXPECT_EQ(get(p, "f"), "6");
	EXPECT_EQ(get(p, "g"), "7  "); // trailing whitespace is kept
	EXPECT_EQ(get(p, "h"), "= 8"); // only one separator is skipped
	EXPECT_EQ(get(p, "i"), ": 9"); // whitespace ends the key, then one '=' or ':' is skipped
	EXPECT_EQ(get(p, "j"), "=10");
	EXPECT_EQ(get(p, "k"), "");
	EXPECT_EQ(get(p, "l"), "");
	EXPECT_EQ(get(p, ""), "14"); // ":" line (after leading whitespace) overrides "=13"
	EXPECT_EQ(p.size(), 13u);
}

TEST(PropertiesTest, EscapedSeparatorsInKeys) {
	Properties p = loadLatin1("a\\=b=c\n"
	                          "x\\ y z\n"
	                          "back\\\\=slash\n"
	                          "\\u0061\\u003d=escaped\n"
	                          "co\\:lon:v\n");
	EXPECT_EQ(get(p, "a=b"), "c");
	EXPECT_EQ(get(p, "x y"), "z");
	EXPECT_EQ(get(p, "back\\"), "slash");
	EXPECT_EQ(get(p, "a="), "escaped");
	EXPECT_EQ(get(p, "co:lon"), "v");
}

TEST(PropertiesTest, Comments) {
	Properties p = loadLatin1("# comment\n"
	                          "! also a comment\n"
	                          "   \t# indented comment\n"
	                          "a=b # not a comment\n"
	                          "#comment ending with backslash \\\n"
	                          "c=d\n"
	                          "e=f\\\n"
	                          "#this continues the value\n");
	EXPECT_EQ(get(p, "a"), "b # not a comment");
	EXPECT_EQ(get(p, "c"), "d");
	EXPECT_EQ(get(p, "e"), "f#this continues the value");
	EXPECT_EQ(p.size(), 3u);
}

TEST(PropertiesTest, LineContinuation) {
	Properties p = loadLatin1("a=b\\\n    c\n"
	                          "crlf=1\\\r\n  2\r\n"
	                          "cr=3\\\r  4\r"
	                          "even=x\\\\\n"
	                          "odd=y\\\\\\\n  z\n"
	                          "emptyNext=\\\n\n"
	                          "next=value\n"
	                          "multi=1,\\\n\t2,\\\n\t3\n"
	                          "eof=last\\");
	EXPECT_EQ(get(p, "a"), "bc");
	EXPECT_EQ(get(p, "crlf"), "12");
	EXPECT_EQ(get(p, "cr"), "34");
	EXPECT_EQ(get(p, "even"), "x\\");
	EXPECT_EQ(get(p, "odd"), "y\\z");
	EXPECT_EQ(get(p, "emptyNext"), ""); // an empty natural line ends the logical line
	EXPECT_EQ(get(p, "next"), "value");
	EXPECT_EQ(get(p, "multi"), "1,2,3");
	EXPECT_EQ(get(p, "eof"), "last"); // backslash at EOF is dropped
}

TEST(PropertiesTest, ContinuationOfEmptyLineStartsNewLogicalLine) {
	// Java quirk: a line consisting only of a backslash continues into the next line with length 0, so a following '#' starts a comment
	Properties p = loadLatin1("a=1\n\\\n#b=2\nc=3\n");
	EXPECT_EQ(get(p, "a"), "1");
	EXPECT_EQ(get(p, "c"), "3");
	EXPECT_EQ(p.size(), 2u);
}

TEST(PropertiesTest, LineTerminatorsAndBlankLines) {
	Properties p = loadLatin1("a=1\rb=2\r\nc=3\n\n  \n\t\r\n\fd=4");
	EXPECT_EQ(get(p, "a"), "1");
	EXPECT_EQ(get(p, "b"), "2");
	EXPECT_EQ(get(p, "c"), "3");
	EXPECT_EQ(get(p, "d"), "4");
	EXPECT_EQ(p.size(), 4u);
	EXPECT_TRUE(loadLatin1("").isEmpty());
	EXPECT_TRUE(loadLatin1("\n\n# only comments\n").isEmpty());
	EXPECT_TRUE(loadLatin1("# comment at EOF without newline").isEmpty());
}

TEST(PropertiesTest, Escapes) {
	Properties p = loadLatin1("ctl=\\t\\n\\r\\f\n"
	                          "u=\\u0041\\u00e4\\u4E2D\n"
	                          "pair=\\uD83D\\uDE00\n"
	                          "other=\\q\\\"\\'\\#\\!\\ \n"
	                          "leading=\\  x\n");
	EXPECT_EQ(get(p, "ctl"), "\t\n\r\f");
	EXPECT_EQ(get(p, "u"), "A\xC3\xA4\xE4\xB8\xAD");
	EXPECT_EQ(get(p, "pair"), "\xF0\x9F\x98\x80"); // surrogate pair combined into one UTF-8 sequence
	EXPECT_EQ(get(p, "other"), "q\"'#! ");
	EXPECT_EQ(get(p, "leading"), "  x");
}

TEST(PropertiesTest, UnpairedSurrogateBecomesReplacementCharacter) {
	Properties p = loadLatin1("a=\\uD800x");
	EXPECT_EQ(get(p, "a"), "\xEF\xBF\xBDx");
}

TEST(PropertiesTest, MalformedUnicodeEscape) {
	for (std::string_view input : {"a=\\uZZZZ", "a=\\u12", "a=\\u00G0 more", "\\u12=value"}) {
		try {
			loadLatin1(input);
			FAIL() << "expected an exception for " << input;
		} catch (const utils::IllegalArgumentException& e) {
			EXPECT_STREQ(e.what(), "Malformed \\uxxxx encoding.");
		}
	}
}

TEST(PropertiesTest, StreamIsDecodedAsLatin1) {
	Properties latin1 = loadLatin1("a=\xE4\n");
	EXPECT_EQ(get(latin1, "a"), "\xC3\xA4"); // U+00E4
	Properties utf8Bytes = loadLatin1("a=\xC3\xA4\n");
	EXPECT_EQ(get(utf8Bytes, "a"), "\xC3\x83\xC2\xA4"); // mojibake, like Java

	Properties text;
	text.loadUtf8("a=\xC3\xA4\nb=\\u00e4");
	EXPECT_EQ(get(text, "a"), "\xC3\xA4");
	EXPECT_EQ(get(text, "b"), "\xC3\xA4");

	Properties reader;
	std::istringstream in("c=\xE4\xB8\xAD");
	reader.loadUtf8(in);
	EXPECT_EQ(get(reader, "c"), "\xE4\xB8\xAD");
}

TEST(PropertiesTest, LaterValuesReplaceEarlierOnes) {
	Properties p = loadLatin1("a=1\na=2\n");
	EXPECT_EQ(get(p, "a"), "2");
	std::istringstream more("a=3\nb=4");
	p.load(more);
	EXPECT_EQ(get(p, "a"), "3");
	EXPECT_EQ(get(p, "b"), "4");
}

TEST(PropertiesTest, DefaultsChain) {
	auto grandDefaults = std::make_shared<Properties>();
	grandDefaults->setProperty("level", "grand");
	grandDefaults->setProperty("onlyGrand", "g");
	auto defaults = std::make_shared<Properties>(grandDefaults);
	defaults->setProperty("level", "defaults");
	defaults->setProperty("onlyDefaults", "d");
	Properties p(defaults);

	EXPECT_TRUE(p.isEmpty());
	EXPECT_EQ(p.size(), 0u);
	EXPECT_EQ(get(p, "level"), "defaults");
	EXPECT_EQ(get(p, "onlyGrand"), "g");
	EXPECT_FALSE(p.getProperty("missing").has_value());
	EXPECT_EQ(p.getProperty("missing", "fallback"), "fallback");
	EXPECT_EQ(p.getProperty("onlyDefaults", "fallback"), "d");
	EXPECT_FALSE(p.containsKey("level"));

	EXPECT_FALSE(p.setProperty("level", "own").has_value());
	EXPECT_EQ(p.setProperty("level", "own2"), "own");
	EXPECT_EQ(get(p, "level"), "own2");
	EXPECT_EQ(get(*defaults, "level"), "defaults");
	EXPECT_FALSE(p.isEmpty());
	EXPECT_EQ(p.size(), 1u);
	EXPECT_EQ(p.stringPropertyNames(), (std::set<std::string>{"level", "onlyDefaults", "onlyGrand"}));

	EXPECT_EQ(p.remove("level"), "own2");
	EXPECT_EQ(get(p, "level"), "defaults");
	EXPECT_FALSE(p.remove("level").has_value());
}

TEST(PropertiesTest, PutAllCopiesOnlyOwnEntries) {
	auto defaults = std::make_shared<Properties>();
	defaults->setProperty("fromDefaults", "x");
	Properties source(defaults);
	source.setProperty("a", "1");
	Properties target;
	target.setProperty("a", "0");
	target.setProperty("b", "2");
	target.putAll(source);
	EXPECT_EQ(get(target, "a"), "1");
	EXPECT_EQ(get(target, "b"), "2");
	EXPECT_FALSE(target.getProperty("fromDefaults").has_value());
	target.putAll(target);
	EXPECT_EQ(target.size(), 2u);
}

namespace {

/** @return the output without the date comment line (the first line, or the second if a comment was written) */
std::vector<std::string> storedLines(const std::string& output) {
	std::vector<std::string> lines;
	std::istringstream in(output);
	for (std::string line; std::getline(in, line);)
		lines.push_back(line);
	return lines;
}

} // namespace

TEST(PropertiesTest, StoreEscapesLikeJava) {
	Properties p;
	p.setProperty("a b", " x=y");
	p.setProperty("key#!:", "tab\there\\");
	p.setProperty("uml", "\xC3\xA4\xE4\xB8\xAD");
	p.setProperty("ctl", std::string("\x01", 1));

	std::ostringstream out;
	p.store(out, "Comment");
	std::vector<std::string> lines = storedLines(out.str());
	ASSERT_EQ(lines.size(), 6u);
	EXPECT_EQ(lines[0], "#Comment");
	// Java: "#" + new Date(), e.g. "#Sat Sep 12 14:03:00 CEST 2026"
	EXPECT_TRUE(std::regex_match(lines[1], std::regex(R"(#[A-Z][a-z]{2} [A-Z][a-z]{2} \d{2} \d{2}:\d{2}:\d{2} \S+ \d{4})"))) << lines[1];
	EXPECT_EQ(lines[2], "a\\ b=\\ x\\=y");
	EXPECT_EQ(lines[3], "ctl=\\u0001");
	EXPECT_EQ(lines[4], "key\\#\\!\\:=tab\\there\\\\");
	EXPECT_EQ(lines[5], "uml=\\u00E4\\u4E2D");

	std::ostringstream utf8Out;
	p.storeUtf8(utf8Out, std::nullopt);
	std::vector<std::string> utf8Lines = storedLines(utf8Out.str());
	ASSERT_EQ(utf8Lines.size(), 5u);
	EXPECT_TRUE(utf8Lines[0].starts_with('#'));
	EXPECT_EQ(utf8Lines[4], "uml=\xC3\xA4\xE4\xB8\xAD");

	// round trip
	Properties reloaded = loadLatin1(out.str());
	EXPECT_EQ(reloaded.entries(), p.entries());
	Properties reloadedUtf8;
	reloadedUtf8.loadUtf8(utf8Out.str());
	EXPECT_EQ(reloadedUtf8.entries(), p.entries());
}

TEST(PropertiesTest, StoreMultilineComments) {
	Properties p;
	std::ostringstream out;
	p.store(out, "line1\nline2\r\n#line3\n!line4\r\xE4\xB8\xAD");
	std::vector<std::string> lines = storedLines(out.str());
	ASSERT_EQ(lines.size(), 6u);
	EXPECT_EQ(lines[0], "#line1");
	EXPECT_EQ(lines[1], "#line2");
	EXPECT_EQ(lines[2], "#line3");
	EXPECT_EQ(lines[3], "!line4");
	EXPECT_EQ(lines[4], "#\\u4E2D");
	EXPECT_TRUE(lines[5].starts_with('#'));
}
