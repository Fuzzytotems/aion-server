// The P5-09c part of the small economy services of the enter-world path (m5a-plan.md E2-03): MailService.onPlayerLogin on the test database,
// RelinquishCraftStatus and the craft quest lists, and the AuctionResult companion. Split from tests/economy/EconomyServicesTest.cpp by subject
// when P5-09 became P5-09a/b/c (m5c-plan.md D1, I-01); the cases are unchanged, only their suite is named after this file. EconomyTestSupport.h is
// P5-09a's (tests/economy/P5-09a), found through the test executable's include directories.
//
// Expectations are derived by hand from the Java sources named in each test.

#include <gtest/gtest.h>

#include <span>
#include <vector>

#include "EconomyTestSupport.h"
#include "aion/gameserver/model/craft/CraftQuestsLists.h"
#include "aion/gameserver/model/craft/ProfessionInfo.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/craft/CraftSkillUpdateService.h"
#include "aion/gameserver/services/craft/RelinquishCraftStatus.h"
#include "aion/gameserver/services/mail/AuctionResultInfo.h"
#include "aion/gameserver/services/mail/MailService.h"

namespace aion::gameserver::economy::test {
namespace {

class MailCraftServicesTest : public EconomyTest {};

// MailService.java:265-268
TEST_F(MailCraftServicesTest, MailServiceOnPlayerLoginLoadsTheMailbox) {
	ECONOMY_REQUIRE_DATABASE();
	insertPlayer(100001, "Tester", 1001);
	PlayerFixture fixture = makePlayer(100001, 1001);

	services::mail::MailService::onPlayerLogin(*fixture.player);

	ASSERT_TRUE(fixture.player->getMailbox());
	EXPECT_TRUE(fixture.player->getMailbox()->getLetters().empty());
}

// MasterQuestsList.java / ExpertQuestsList.java getQuestIds, Profession.java
TEST_F(MailCraftServicesTest, CraftQuestListsAndProfessions) {
	using model::Race;
	std::span<const int32_t> masterCooking = model::craft::getMasterQuestIds(40001, Race::ELYOS);
	EXPECT_EQ(std::vector<int32_t>(masterCooking.begin(), masterCooking.end()), (std::vector<int32_t>{19039, 19038}));
	std::span<const int32_t> expertMenusier = model::craft::getExpertQuestIds(40010, Race::ASMODIANS);
	EXPECT_EQ(std::vector<int32_t>(expertMenusier.begin(), expertMenusier.end()), (std::vector<int32_t>{29050, 29053, 29052, 29056, 29055, 29054}));
	try {
		model::craft::getMasterQuestIds(30002, Race::ELYOS);
		FAIL() << "essence tapping has no master quests";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "Invalid craftSkillId: 30002 or race: ELYOS");
	}
	EXPECT_FALSE(model::craft::isCrafting(model::craft::Profession::AETHERTAPPING));
	EXPECT_TRUE(model::craft::isCrafting(model::craft::Profession::CONSTRUCTION));
	EXPECT_EQ(model::craft::getSkillId(model::craft::Profession::ALCHEMY), 40007);
}

// RelinquishCraftStatus.java:875-903 and CraftSkillUpdateService.java:129-157 with a character without crafting skills
TEST_F(MailCraftServicesTest, RemoveExcessCraftStatusWithoutCraftingSkills) {
	PlayerFixture fixture = makePlayer(100001, 1001);
	fixture.player->setSkillList(model::skill::PlayerSkillList::create());
	services::craft::CraftSkillUpdateService& craft = services::craft::CraftSkillUpdateService::getInstance();
	EXPECT_EQ(craft.getTotalExpertCraftingSkills(*fixture.player), 0);
	EXPECT_EQ(craft.getTotalMasterCraftingSkills(*fixture.player), 0);
	EXPECT_NO_THROW(services::craft::RelinquishCraftStatus::removeExcessCraftStatus(*fixture.player, false));
}

// AuctionResult.java
TEST(AuctionResultInfoTest, IdsAndLookup) {
	using services::mail::AuctionResult;
	EXPECT_EQ(services::mail::getId(AuctionResult::FAILED_BID), 0);
	EXPECT_EQ(services::mail::getId(AuctionResult::GRACE_SUCCESS), 7);
	EXPECT_EQ(services::mail::auctionResultOf(4), AuctionResult::WIN_BID);
	EXPECT_EQ(services::mail::auctionResultOf(8), std::nullopt);
	EXPECT_EQ(services::mail::auctionResultOf(-1), std::nullopt);
}

} // namespace
} // namespace aion::gameserver::economy::test
