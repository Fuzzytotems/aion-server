// The ascension lane: WebRewardService.MaxLevelReward.isPendingAscension (WebRewardService.java:102-104), the body of the class the retail
// ascension route calls. _1006Ascension.onQuestCompletedEvent and _2008Ascension's (_1006Ascension.java:278-285, _2008Ascension.java:298-305)
// ask it right after updateDaeva, inside the single try of QuestEngine.onQuestCompleted (QuestEngine.java:254-265, QuestEngine.cpp:255-266),
// so an unported body there logged an ERROR at every ascension and skipped the onQuestCompletedEvent of every handler after it (the
// ascension analysis, "No owner" row). The set it reads is filled only by MaxLevelReward.reward, the web reward's "level 65" action (still
// unported, like the rest of the class).
//
// Expectations are derived by hand from the Java source: `pendingAscension.contains(player.getObjectId())`.

#include <gtest/gtest.h>

#include "EconomyTestSupport.h"
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
		EconomyTest::TearDown();
	}
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

} // namespace
} // namespace aion::gameserver::economy::test
