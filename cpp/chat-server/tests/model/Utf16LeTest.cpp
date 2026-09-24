// new String(bytes, StandardCharsets.UTF_16LE) and String.getBytes(UTF_16LE): the expected results follow Java's UnicodeDecoder with
// CodingErrorAction.REPLACE (see Utf16Le.h).

#include <gtest/gtest.h>

#include "aion/chatserver/utils/Utf16Le.h"

namespace aion::chatserver::test {
namespace {

namespace Utf16Le = utils::Utf16Le;
using Bytes = std::vector<uint8_t>;

TEST(Utf16LeTest, DecodesLittleEndianChars) {
	EXPECT_EQ(Utf16Le::decode(Bytes{'A', 0x00, 0x40, 0x00, 0x1B, 0xAC}), u"A@갛");
	EXPECT_EQ(Utf16Le::newString(Bytes{'h', 0, 0xFC, 0}), "h\xC3\xBC"); // "hü"
	EXPECT_EQ(Utf16Le::decode(Bytes{}), u"");
}

TEST(Utf16LeTest, KeepsSurrogatePairs) {
	// U+1F600 = D83D DE00
	EXPECT_EQ(Utf16Le::decode(Bytes{0x3D, 0xD8, 0x00, 0xDE}), u"\U0001F600");
	EXPECT_EQ(Utf16Le::newString(Bytes{0x3D, 0xD8, 0x00, 0xDE}), "\xF0\x9F\x98\x80");
}

TEST(Utf16LeTest, HighSurrogateFollowedByAnotherCharIsOneReplacementForBoth) {
	// malformedForLength(4): the char after the high surrogate is swallowed
	EXPECT_EQ(Utf16Le::decode(Bytes{0x3D, 0xD8, 'x', 0x00, 'y', 0x00}), u"�y");
}

TEST(Utf16LeTest, UnpairedLowSurrogateAndReversedMarkAreReplaced) {
	EXPECT_EQ(Utf16Le::decode(Bytes{0x00, 0xDE, 'a', 0x00}), u"�a");
	EXPECT_EQ(Utf16Le::decode(Bytes{0xFE, 0xFF, 'a', 0x00}), u"�a");
	// a byte order mark is an ordinary char in UTF_16LE
	EXPECT_EQ(Utf16Le::decode(Bytes{0xFF, 0xFE, 'a', 0x00}), u"﻿a");
}

TEST(Utf16LeTest, BytesLeftAtTheEndAreOneReplacement) {
	EXPECT_EQ(Utf16Le::decode(Bytes{'a', 0x00, 'b'}), u"a�");
	EXPECT_EQ(Utf16Le::decode(Bytes{'a', 0x00, 0x3D, 0xD8}), u"a�");
	EXPECT_EQ(Utf16Le::decode(Bytes{'a', 0x00, 0x3D, 0xD8, 0x00}), u"a�");
	EXPECT_EQ(Utf16Le::decode(Bytes{0x01}), u"�");
}

TEST(Utf16LeTest, GetBytesEncodesLittleEndian) {
	EXPECT_EQ(Utf16Le::getBytes("A\xC3\xBC"), (Bytes{'A', 0x00, 0xFC, 0x00}));
	EXPECT_EQ(Utf16Le::getBytes("\xF0\x9F\x98\x80"), (Bytes{0x3D, 0xD8, 0x00, 0xDE}));
	EXPECT_EQ(Utf16Le::getBytes(""), Bytes{});
}

} // namespace
} // namespace aion::chatserver::test
