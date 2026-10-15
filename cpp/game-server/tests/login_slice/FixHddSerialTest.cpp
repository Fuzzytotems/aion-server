// CM_MAC_ADDRESS.fixHddSerial (P5-00, m5a-plan.md S-01/S-10): vectors derived by hand from the Java body (String.length in UTF-16 code units,
// matches("[0-9a-zA-Z _-]+"), the UTF-16LE hex dump in upper case, the "(.)(.)" pair swap when the second or penultimate character is a space,
// String.trim).

#include <gtest/gtest.h>

#include <string>

#include "aion/gameserver/network/aion/clientpackets/CM_MAC_ADDRESS.h"

namespace aion::gameserver::network::aion::clientpackets {
namespace {

std::string fix(std::string_view serial) {
	return CM_MAC_ADDRESS::fixHddSerial(serial);
}

TEST(FixHddSerialTest, EmptyStaysEmpty) {
	// isEmpty() skips the hex branch, the swap pattern needs 3 characters, trim("") is ""
	EXPECT_EQ(fix(""), "");
}

TEST(FixHddSerialTest, PlainSerialIsTrimmed) {
	EXPECT_EQ(fix("W-DXE1A2B3C4"), "W-DXE1A2B3C4");
	// "  WD-123  ": no alphanumeric first character and a space as last character, so no swap; trim removes the spaces
	EXPECT_EQ(fix("  WD-123  "), "WD-123");
	// "ABC ": the penultimate character is no space
	EXPECT_EQ(fix("ABC "), "ABC");
}

TEST(FixHddSerialTest, UpToTwoCharactersAreHexDumped) {
	// "AB": length 2 -> UTF-16LE 41 00 42 00
	EXPECT_EQ(fix("AB"), "0x41004200");
	EXPECT_EQ(fix("7"), "0x3700");
	// U+1F600 is two UTF-16 code units D83D DE00 (length 2): 3D D8 00 DE
	EXPECT_EQ(fix("\xF0\x9F\x98\x80"), "0x3DD800DE");
}

TEST(FixHddSerialTest, CharactersOutsideTheSerialClassAreHexDumped) {
	EXPECT_EQ(fix("ABC$"), "0x4100420043002400");
	// U+00C4 (C3 84 in UTF-8) is one code unit 00C4: C4 00
	EXPECT_EQ(fix("\xC3\x84" "12"), "0xC40031003200");
	EXPECT_EQ(fix("AB\tC"), "0x4100420009004300");
	// U+FFFD, what readS makes of an unpaired surrogate, is FD FF, the bytes Java's getBytes(UTF_16LE) writes for the unpaired surrogate itself
	EXPECT_EQ(fix("\xEF\xBF\xBD"), "0xFDFF");
}

TEST(FixHddSerialTest, SwappedPairsAreRestored) {
	// second character is a space: "S 3A1B2" -> pairs swapped " SA3B12" (the odd last character stays) -> trim
	EXPECT_EQ(fix("S 3A1B2"), "SA3B12");
	// penultimate character is a space: "AB-C D" -> "BAC-D " -> trim
	EXPECT_EQ(fix("AB-C D"), "BAC-D");
	// "A B " matches the first alternative: " A B" -> trim
	EXPECT_EQ(fix("A B "), "A B");
	// a serial with switched characters: "WD-WCAV12345678 " sent as "DWW-AC1V325476 8" -> pairs swapped back, then trimmed
	EXPECT_EQ(fix("DWW-AC1V325476 8"), "WD-WCAV12345678");
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets
