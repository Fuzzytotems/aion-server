// StarterKitService.onLevelUp (m5c-plan.md R-02, StarterKitService.java:63-75): every level from fromLevel to toLevel, both inclusive, that has
// a kit mails each of its items as an EXPRESS SystemMailService letter from "Beyond Aion". The fixture is the mail lane's
// (tests/economy/P5-09c/MailTestSupport.h, found through the executable's include directories); the recipient is online, so the letters land in
// his mailbox, and the letter rows need the economy test database.

#include "MailTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "aion/gameserver/services/reward/StarterKitService.h"

namespace aion::gameserver::economy::test::mail {
namespace {

using services::reward::StarterKitService;

class StarterKitServiceTest : public MailTest {
protected:
	/** (item id, count) of B's letters in the order they were sent (letter ids rise with IDFactory) */
	std::vector<std::pair<int32_t, int64_t>> mailedItems() {
		std::vector<runtime::Ptr<Letter>> letters = partner().getMailbox()->getLetters();
		std::sort(letters.begin(), letters.end(), [](const runtime::Ptr<Letter>& x, const runtime::Ptr<Letter>& y) {
			return x->getObjectId() < y->getObjectId();
		});
		std::vector<std::pair<int32_t, int64_t>> mailed;
		for (const runtime::Ptr<Letter>& letter : letters) {
			EXPECT_EQ(letter->getSenderName(), "Beyond Aion");
			EXPECT_EQ(letter->getTitle(), "Starter Kit");
			EXPECT_EQ(letter->getMessage(), "Greetings Daeva!\n\nIn gratitude for your decision to join our server, we would like to support you with an "
											"additional item pack during the leveling.\n\nEnjoy your stay on Beyond Aion!");
			EXPECT_EQ(letter->getLetterType(), LetterType::EXPRESS);
			EXPECT_EQ(letter->getAttachedKinah(), 0);
			runtime::Ptr<Item> item = letter->getAttachedItem();
			mailed.emplace_back(item ? item->getItemId() : 0, item ? item->getItemCount() : 0);
		}
		return mailed;
	}
};

// Level 25's kit (StarterKitService.java:44-47), in the kit's order; levels 21-24 have none
TEST_F(StarterKitServiceTest, LevelTwentyFiveMailsItsFourItems) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	StarterKitService::getInstance().onLevelUp(partner(), 21, 25);
	EXPECT_EQ(mailedItems(), (std::vector<std::pair<int32_t, int64_t>>{{190100032, 1}, {164002272, 25}, {162000039, 25}, {162002018, 25}}));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail WHERE express = 1"), 4);
}

// Both bounds are inclusive: 1..1 is level 1's title card, 25..25 level 25's kit
TEST_F(StarterKitServiceTest, BothBoundsAreInclusive) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	StarterKitService::getInstance().onLevelUp(partner(), 1, 1);
	EXPECT_EQ(mailedItems(), (std::vector<std::pair<int32_t, int64_t>>{{169610056, 1}}));
	StarterKitService::getInstance().onLevelUp(partner(), 25, 25);
	EXPECT_EQ(mailedItems().size(), 5u);
}

TEST_F(StarterKitServiceTest, LevelsWithoutAKitMailNothing) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	StarterKitService::getInstance().onLevelUp(partner(), 2, 19);
	StarterKitService::getInstance().onLevelUp(partner(), 26, 20); // an empty range
	EXPECT_TRUE(partner().getMailbox()->getLetters().empty());
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 0);
}

} // namespace
} // namespace aion::gameserver::economy::test::mail
