// NameRestrictionService (m5a-plan.md F-03, the create flow): the name patterns match the whole name as UTF-16 characters (Java
// Matcher.matches), the forbidden sequence pattern is searched (Matcher.find, absent pattern = nothing forbidden), forbidden words compare
// case-insensitively, and filterMessage stars out forbidden words with one '*' per UTF-16 character. Expectations derived by hand from
// NameRestrictionService.java.

#include <gtest/gtest.h>

#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/services/NameRestrictionService.h"

namespace aion::gameserver::services {
namespace {

using configs::main::LegionConfig;
using configs::main::NameConfig;

class NameRestrictionServiceTest : public testing::Test {
protected:
	void SetUp() override {
		charPattern = *NameConfig::CHAR_NAME_PATTERN.get();
		petPattern = *NameConfig::PET_NAME_PATTERN.get();
		legionPattern = *LegionConfig::LEGION_NAME_PATTERN.get();
		sequencePattern = *NameConfig::FORBIDDEN_SEQUENCE_PATTERN.get();
		words = *NameConfig::FORBIDDEN_WORDS.get();
		// the defaults of NameConfig/LegionConfig
		NameConfig::CHAR_NAME_PATTERN.set(std::wregex(L"[a-zA-Z]{2,16}"));
		NameConfig::PET_NAME_PATTERN.set(std::wregex(L"[a-zA-Z]{2,16}"));
		LegionConfig::LEGION_NAME_PATTERN.set(std::wregex(L"[a-zA-Z0-9]{2,15}"));
		NameConfig::FORBIDDEN_SEQUENCE_PATTERN.set(std::nullopt);
		NameConfig::FORBIDDEN_WORDS.set({});
	}

	void TearDown() override {
		NameConfig::CHAR_NAME_PATTERN.set(charPattern);
		NameConfig::PET_NAME_PATTERN.set(petPattern);
		LegionConfig::LEGION_NAME_PATTERN.set(legionPattern);
		NameConfig::FORBIDDEN_SEQUENCE_PATTERN.set(sequencePattern);
		NameConfig::FORBIDDEN_WORDS.set(words);
	}

	std::wregex charPattern, petPattern, legionPattern;
	std::optional<std::wregex> sequencePattern;
	std::vector<std::string> words;
};

TEST_F(NameRestrictionServiceTest, NamesMatchTheWholePattern) {
	EXPECT_TRUE(NameRestrictionService::isValidName("Elyos"));
	EXPECT_FALSE(NameRestrictionService::isValidName("E"));
	EXPECT_FALSE(NameRestrictionService::isValidName("Elyos1"));
	EXPECT_FALSE(NameRestrictionService::isValidName("Abcdefghijklmnopq")); // 17 characters
	EXPECT_TRUE(NameRestrictionService::isValidPetName("Kitty"));
	EXPECT_TRUE(NameRestrictionService::isValidLegionName("Legion42"));
	EXPECT_FALSE(NameRestrictionService::isValidLegionName("Legion 42"));

	// the pattern counts UTF-16 characters: 16 Cyrillic letters are 32 UTF-8 bytes
	NameConfig::CHAR_NAME_PATTERN.set(std::wregex(L"[\x0400-\x04FF]{2,16}"));
	EXPECT_TRUE(NameRestrictionService::isValidName("\xD0\x90\xD0\x91\xD0\x92\xD0\x93\xD0\x94\xD0\x95\xD0\x96\xD0\x97\xD0\x98\xD0\x99\xD0\x9A"
													"\xD0\x9B\xD0\x9C\xD0\x9D\xD0\x9E\xD0\x9F"));
	EXPECT_FALSE(NameRestrictionService::isValidName("Elyos"));
}

TEST_F(NameRestrictionServiceTest, ForbiddenSequencesAndWords) {
	EXPECT_FALSE(NameRestrictionService::isForbidden("Admin")); // nothing configured

	NameConfig::FORBIDDEN_SEQUENCE_PATTERN.set(std::wregex(L"gm|admin"));
	EXPECT_TRUE(NameRestrictionService::isForbidden("theadminx")); // find, not matches
	EXPECT_FALSE(NameRestrictionService::isForbidden("theAdminx")); // the pattern is case-sensitive

	NameConfig::FORBIDDEN_WORDS.set({"Staff", "Support"});
	EXPECT_TRUE(NameRestrictionService::isForbiddenWord("STAFF"));
	EXPECT_TRUE(NameRestrictionService::isForbidden("support"));
	EXPECT_FALSE(NameRestrictionService::isForbiddenWord("Staffer"));
}

TEST_F(NameRestrictionServiceTest, FilterMessageStarsOutForbiddenWords) {
	NameConfig::FORBIDDEN_WORDS.set({"bad", "\xC3\xBC" "ble"}); // u-umlaut "ble": 5 UTF-8 bytes, 4 UTF-16 characters
	EXPECT_EQ(NameRestrictionService::filterMessage("this is bad"), "this is ***");
	EXPECT_EQ(NameRestrictionService::filterMessage("BAD and bad"), "*** and ***");
	// Java: message.replace(word, stars) replaces the word everywhere in the message, also inside other words
	EXPECT_EQ(NameRestrictionService::filterMessage("badge bad"), "***ge ***");
	EXPECT_EQ(NameRestrictionService::filterMessage("\xC3\xBC" "ble!"), "\xC3\xBC" "ble!");
	EXPECT_EQ(NameRestrictionService::filterMessage("so \xC3\xBC" "ble"), "so ****");
	EXPECT_EQ(NameRestrictionService::filterMessage("nothing here"), "nothing here");
}

} // namespace
} // namespace aion::gameserver::services
