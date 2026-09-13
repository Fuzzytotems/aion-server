#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

using namespace aion::commons::utils;

TEST(StringUtilsTest, Utf16RoundTrip) {
	std::string text = "Grüße 😀 abc";
	std::u16string wide = StringUtils::toUtf16(text);
	EXPECT_EQ(wide.size(), 12u); // emoji is a surrogate pair
	EXPECT_EQ(StringUtils::toUtf8(wide), text);
	EXPECT_EQ(StringUtils::utf16Length(text), 12);
}

TEST(StringUtilsTest, InvalidSequencesAreReplaced) {
	EXPECT_EQ(StringUtils::toUtf16("a\xFF"), u"a�");
	std::u16string loneSurrogate = u"x";
	loneSurrogate += static_cast<char16_t>(0xD800);
	EXPECT_EQ(StringUtils::toUtf8(loneSurrogate), "x\xEF\xBF\xBD");
}

TEST(StringUtilsTest, TrimLikeJava) {
	EXPECT_EQ(StringUtils::trim("\t  hi \x01\n"), "hi");
	EXPECT_EQ(StringUtils::trim("   "), "");
	EXPECT_TRUE(StringUtils::isBlank(" \t"));
	EXPECT_FALSE(StringUtils::isBlank(" x "));
}

TEST(StringUtilsTest, SplitJavaSemantics) {
	EXPECT_EQ(StringUtils::splitJava("a,b,,", ","), (std::vector<std::string>{"a", "b"}));
	EXPECT_EQ(StringUtils::splitJava("", ","), (std::vector<std::string>{""}));
	EXPECT_TRUE(StringUtils::splitJava(",", ",").empty());
	EXPECT_EQ(StringUtils::split("a,b,,", ","), (std::vector<std::string>{"a", "b", "", ""}));
}

TEST(StringUtilsTest, CaseInsensitive) {
	EXPECT_TRUE(StringUtils::equalsIgnoreCase("Hello", "hELLO"));
	EXPECT_FALSE(StringUtils::equalsIgnoreCase("Hello", "Hell"));
	EXPECT_EQ(StringUtils::toLowerCase("AbC"), "abc");
	EXPECT_EQ(StringUtils::replace("a.b.c", ".", "::"), "a::b::c");
}

TEST(StringUtilsTest, MalformedUtf8LikeJavaStringDecoding) {
	const std::u16string REPL = u"\uFFFD";
	// one replacement character per malformed subsequence (Java: String.decodeUTF8_UTF16 with malformed3/malformed4)
	EXPECT_EQ(StringUtils::toUtf16("\xE2\x82" "A"), REPL + u"A");      // truncated 3-byte sequence
	EXPECT_EQ(StringUtils::toUtf16("\xED\xA0\x80" "A"), REPL + u"A");  // encoded surrogate
	EXPECT_EQ(StringUtils::toUtf16("\xE0\x80\x80"), REPL + REPL + REPL); // overlong: E0 followed by 80..9F is malformed after one byte
	EXPECT_EQ(StringUtils::toUtf16("\xF0\x9F\x98" "A"), REPL + u"A");  // truncated 4-byte sequence
	EXPECT_EQ(StringUtils::toUtf16("\xF0\x80\x80\x80"), REPL + REPL + REPL + REPL);
	EXPECT_EQ(StringUtils::toUtf16("\xF5\x80\x80\x80"), REPL + REPL + REPL + REPL);
	EXPECT_EQ(StringUtils::toUtf16("\xC0\xAF"), REPL + REPL);
	EXPECT_EQ(StringUtils::toUtf16("\xC3" "A"), REPL + u"A");
	EXPECT_EQ(StringUtils::toUtf16("\x80\xBF"), REPL + REPL);
	// a truncated sequence at the end produces one replacement character for all remaining bytes
	EXPECT_EQ(StringUtils::toUtf16("ab\xE2\x82"), u"ab" + REPL);
	EXPECT_EQ(StringUtils::toUtf16("ab\xF0\x9F\x98"), u"ab" + REPL);
	EXPECT_EQ(StringUtils::toUtf16("ab\xF0\x9F"), u"ab" + REPL);
	EXPECT_EQ(StringUtils::toUtf16("ab\xC3"), u"ab" + REPL);
	EXPECT_EQ(StringUtils::toUtf16("ab\xE2" "A"), u"ab" + REPL + u"A");
	EXPECT_EQ(StringUtils::utf16Length("\xE2\x82" "A"), 2);
	EXPECT_EQ(StringUtils::utf16Length("\xED\xA0\x80"), 1);
	EXPECT_EQ(StringUtils::toUtf16("\xF0\x9F\x98\x80"), u"\U0001F600");
}

TEST(StringUtilsTest, UnicodeCaseMappingForNames) {
	// game server Util.convertName: name.substring(0, 1).toUpperCase() + name.toLowerCase().substring(1)
	std::string name = "иВАН";
	EXPECT_EQ(StringUtils::toUpperCase(StringUtils::substring(name, 0, 1)) + StringUtils::substring(StringUtils::toLowerCase(name), 1), "Иван");
	EXPECT_EQ(StringUtils::toUpperCase("Ёжик"), "ЁЖИК");
	EXPECT_EQ(StringUtils::toLowerCase("ÀÉÎÕÜ ŁŃŚŹŻ ΑΒΓΔ Ω"), "àéîõü łńśźż αβγδ ω");
	EXPECT_EQ(StringUtils::toUpperCase("àéîõü łńśźż αβγδ ω ς"), "ÀÉÎÕÜ ŁŃŚŹŻ ΑΒΓΔ Ω Σ");
	EXPECT_EQ(StringUtils::toUpperCase("Grüße"), "GRÜSSE");
	EXPECT_EQ(StringUtils::toLowerCase("İ"), "i\xCC\x87");
	EXPECT_EQ(StringUtils::toUpperCase("ÿ"), "Ÿ");
	EXPECT_EQ(StringUtils::toLowerCase("中文 😀 AbC"), "中文 😀 abc");
	EXPECT_EQ(StringUtils::toUpperCase(U'ѳ'), U'Ѳ');
	EXPECT_EQ(StringUtils::toLowerCase(U'Ӂ'), U'ӂ');
	EXPECT_EQ(StringUtils::toLowerCase(U'Ӏ'), U'ӏ');

	// FriendList/BlockList: equalsIgnoreCase lookups
	EXPECT_TRUE(StringUtils::equalsIgnoreCase("Привет", "пРИВЕТ"));
	EXPECT_TRUE(StringUtils::equalsIgnoreCase("ſ", "S")); // Java: toUpperCase('ſ') == 'S'
	EXPECT_TRUE(StringUtils::equalsIgnoreCase("µ", "Μ")); // micro sign and Greek capital mu
	EXPECT_FALSE(StringUtils::equalsIgnoreCase("Привет", "Приветы"));
	EXPECT_FALSE(StringUtils::equalsIgnoreCase("ß", "SS")); // compared per character, like Java
}

TEST(StringUtilsTest, SubstringByUtf16Index) {
	EXPECT_EQ(StringUtils::substring("Grüße", 2, 4), "üß");
	EXPECT_EQ(StringUtils::substring("a😀b", 1, 3), "😀");
	EXPECT_EQ(StringUtils::substring("a😀b", 3), "b");
	EXPECT_EQ(StringUtils::substring("a😀b", 1, 2), "\xEF\xBF\xBD"); // half of a surrogate pair
	EXPECT_EQ(StringUtils::substring("abc", 3), "");
	EXPECT_THROW(StringUtils::substring("abc", 2, 4), IndexOutOfBoundsException);
	EXPECT_THROW(StringUtils::substring("abc", -1, 1), IndexOutOfBoundsException);
	EXPECT_THROW(StringUtils::substring("abc", 2, 1), IndexOutOfBoundsException);
}
