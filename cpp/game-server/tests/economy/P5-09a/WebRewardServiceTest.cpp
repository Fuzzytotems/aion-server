// The ascension lane: WebRewardService.MaxLevelReward.isPendingAscension (WebRewardService.java:102-104), the body of the class the retail
// ascension route calls. _1006Ascension.onQuestCompletedEvent and _2008Ascension's (_1006Ascension.java:278-285, _2008Ascension.java:298-305)
// ask it right after updateDaeva, inside the single try of QuestEngine.onQuestCompleted (QuestEngine.java:254-265, QuestEngine.cpp:260-271),
// so an unported body there logged an ERROR at every ascension and skipped the onQuestCompletedEvent of every handler after it (the
// ascension analysis, "No owner" row). The set it reads is filled only by MaxLevelReward.reward, the web reward's "level 65" action (still
// unported, like the rest of the class).
//
// Expectations are derived by hand from the Java source: `pendingAscension.contains(player.getObjectId())`.

#include <gtest/gtest.h>

#include "EconomyTestSupport.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/reward/WebRewardService.h"

namespace aion::gameserver::economy::test {
namespace {

using services::reward::WebRewardService;

/** The pending set is a process-wide static (Java: a static CopyOnWriteArraySet); each case starts and ends with it empty */
class WebRewardServiceTest : public EconomyTest {
protected:
	void SetUp() override {
		EconomyTest::SetUp();
		WebRewardService::MaxLevelReward::pendingAscension.clear();
	}

	void TearDown() override {
		WebRewardService::MaxLevelReward::pendingAscension.clear();
		if (tablePublished)
			dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		EconomyTest::TearDown();
	}

	/** an experience table of 67 levels (player_experience_table.xml's size; the values only need to grow): getMaxLevel() - 1 is 66 */
	void publishExperienceTable() {
		std::string table = "<player_experience_table>";
		for (int64_t level = 0; level < 67; level++)
			table += "<exp>" + std::to_string(level * 1000) + "</exp>";
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, table + "</player_experience_table>"));
		tablePublished = true;
	}

	xml::LoadContext context;
	bool tablePublished = false;
};

TEST_F(WebRewardServiceTest, ACharacterIsPendingAscensionOnlyWhileItsObjectIdIsInTheSet) {
	PlayerFixture pending = makePlayer(100101, 1101, "Pending");
	PlayerFixture other = makePlayer(100102, 1102, "Other");

	EXPECT_FALSE(WebRewardService::MaxLevelReward::isPendingAscension(*pending.player)) << "nobody claimed the reward yet";

	// MaxLevelReward.reward's first step for a character that is no Daeva yet: pendingAscension.add(player.getObjectId())
	ASSERT_TRUE(WebRewardService::MaxLevelReward::pendingAscension.add(100101));
	EXPECT_TRUE(WebRewardService::MaxLevelReward::isPendingAscension(*pending.player));
	EXPECT_FALSE(WebRewardService::MaxLevelReward::isPendingAscension(*other.player)) << "the set is keyed by object id";

	// and reward's Daeva arm: pendingAscension.remove(player.getObjectId())
	ASSERT_TRUE(WebRewardService::MaxLevelReward::pendingAscension.remove(100101));
	EXPECT_FALSE(WebRewardService::MaxLevelReward::isPendingAscension(*pending.player));
}

/** D12-CP5: WebRewardService.java:31-34 - no player, nothing */
TEST_F(WebRewardServiceTest, NoPlayerNoRewards) {
	EXPECT_NO_THROW(WebRewardService::getInstance().sendAvailableRewards(nullptr));
}

/** D12-CP5: WebRewardService.java:108-110 - a character who is no Daeva and already pending gets nothing more */
TEST_F(WebRewardServiceTest, AnAlreadyPendingCharacterGetsNothingMore) {
	PlayerFixture pending = makePlayer(100103, 1103, "Pending");
	ASSERT_TRUE(WebRewardService::MaxLevelReward::pendingAscension.add(100103));
	EXPECT_FALSE(WebRewardService::MaxLevelReward::reward(*pending.player));
	EXPECT_TRUE(WebRewardService::MaxLevelReward::isPendingAscension(*pending.player));
}

/** D12-CP5: WebRewardService.java:108-117 - a new character is pending, raised to level 9, and needs his ascension quest's state (Java's
 * NullPointerException without it) */
TEST_F(WebRewardServiceTest, ANewCharacterIsPendingAtLevelNine) {
	publishExperienceTable();
	PlayerFixture fresh = makePlayer(100104, 1104, "Fresh");
	fresh.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
	fresh.player->getCommonData()->setLevel(5);
	ASSERT_EQ(fresh.player->getLevel(), 5);
	EXPECT_THROW(WebRewardService::MaxLevelReward::reward(*fresh.player), runtime::NullPointerException) << "no state of quest 1006";
	EXPECT_TRUE(WebRewardService::MaxLevelReward::isPendingAscension(*fresh.player));
	EXPECT_EQ(fresh.player->getLevel(), 9);
	fresh.player->setQuestStateList(nullptr);
}

/** D12-CP5: WebRewardService.java:129-132 - a Daeva at level 66 (the table's maximum minus one) gets nothing */
TEST_F(WebRewardServiceTest, ADaevaAtTheMaxLevelGetsNothing) {
	publishExperienceTable();
	PlayerFixture daeva = makePlayer(100105, 1105, "Daeva");
	daeva.player->getCommonData()->setDaeva(true);
	daeva.player->getCommonData()->setLevel(66);
	ASSERT_TRUE(WebRewardService::MaxLevelReward::pendingAscension.add(100105));
	EXPECT_FALSE(WebRewardService::MaxLevelReward::reward(*daeva.player));
	EXPECT_TRUE(WebRewardService::MaxLevelReward::isPendingAscension(*daeva.player)) << "returned before the removal";
	EXPECT_EQ(daeva.player->getLevel(), 66);
}

} // namespace
} // namespace aion::gameserver::economy::test
