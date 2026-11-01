#include <gtest/gtest.h>

#include <string>

#include "aion/clientprotocol/PacketWriter.h"
#include "aion/clientprotocol/Utf.h"

namespace aion::clientprotocol {
namespace {

TEST(UtfTest, RoundTrips) {
	for (const std::string text : {"", "Tester", "Ünïcødé", "Даэва", "天族", "\xF0\x9F\x90\x89 dragon"}) {
		EXPECT_EQ(utf16ToUtf8(utf8ToUtf16(text)), text);
	}
	EXPECT_EQ(utf8ToUtf16("\xD0\x94"), u"Д");
	EXPECT_EQ(utf8ToUtf16("\xF0\x9F\x90\x89"), u"\U0001F409");
	EXPECT_EQ(utf16ToUtf8(u"\U0001F409"), "\xF0\x9F\x90\x89");
}

TEST(UtfTest, UnpairedSurrogatesBecomeReplacementChars) {
	EXPECT_EQ(utf16ToUtf8(std::u16string{u'a', char16_t(0xD800), u'b'}), "a\xEF\xBF\xBD" "b");
	EXPECT_EQ(utf16ToUtf8(std::u16string{char16_t(0xDC00)}), "\xEF\xBF\xBD");
	EXPECT_EQ(utf16ToUtf8(std::u16string{char16_t(0xD83D)}), "\xEF\xBF\xBD");
}

TEST(UtfTest, MalformedUtf8BecomesReplacementChars) {
	EXPECT_EQ(utf8ToUtf16("a\x80" "b"), u"a�b");          // stray continuation byte
	EXPECT_EQ(utf8ToUtf16("\xC0\xAF"), u"�");             // overlong '/'
	EXPECT_EQ(utf8ToUtf16("\xED\xA0\x80"), u"�");         // encoded surrogate
	EXPECT_EQ(utf8ToUtf16("x\xE2\x82"), u"x�");           // truncated at the end
	EXPECT_EQ(utf8ToUtf16("\xF8\x88\x80\x80\x80"), u"�"); // no 5-byte forms
}

TEST(UtfTest, PacketWriterWritesUtf16LeWithTerminator) {
	EXPECT_EQ(PacketWriter().S("Aй").data, (std::vector<uint8_t>{0x41, 0x00, 0x39, 0x04, 0x00, 0x00}));
}

} // namespace
} // namespace aion::clientprotocol
