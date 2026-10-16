// P5-06b, the handler base (m5d-plan.md D1; this directory is new with item I-01, and H-07 fills it): the hooks of AbstractQuestHandler.
//
// The reward-list hook takes the list QuestService.getRewardItems is building as a mutable list of values (header request m5d-h01,
// m5d-plan.md D17(b)); its default answers UNKNOWN and leaves the list as it is (AbstractQuestHandler.java:277-279). The list here is seeded
// the way getRewardItems starts it, with a copy of the template's reward items (QuestService.java:165-166), from the shipped row of quest
// 80016 "[Event] Sock Hop" (quest_data.xml:74986-74995), the quest whose handler adds to that list (_80016EventSockHop.java:81).
//
// The player is the in-world fixture of tests/cm_ak (InWorldPacketRunSupport.h, included by relative path as tests/quest/QuestDropTest.cpp
// does); a QuestEnv needs one.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <cstdint>
#include <deque>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/Rewards.h"
#include "aion/gameserver/model/templates/rewards/BonusType.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

namespace cp = network::aion::clientpackets::testing;
using gameserver::model::templates::quest::QuestItems;
using gameserver::model::templates::rewards::BonusType;
using runtime::Ref;

/** quest_data.xml:74986-74995 */
constexpr const char* QUESTS_XML =
	R"(<quests>)"
	R"(<quest id="80016" name="[Event] Sock Hop" nameId="1180016" quest_zone="Sanctum" minlevel_permitted="10" max_repeat_count="1")"
	R"( cannot_share="true" race_permitted="ELYOS" category="EVENT">)"
	R"(<collect_items><collect_item item_id="182214008" count="15"/></collect_items>)"
	R"(<rewards gold="100000" exp="50000"><reward_item item_id="188051107" count="1"/><reward_item item_id="125040047" count="1"/></rewards>)"
	R"(<quest_drop npc_id="217280" item_id="182214008"/>)"
	R"(</quest>)"
	R"(</quests>)";

/** A handler that keeps every default hook */
class DefaultHooksHandler final : public AbstractQuestHandler {
public:
	explicit DefaultHooksHandler(int32_t questId) : AbstractQuestHandler(questId) {}

	void register_() override {}
};

class AbstractQuestHandlerTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// Player::postConstruct loads the toy pets from the database; the tests have none
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests([](gameserver::model::gameobjects::player::Player&) {
			return std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>>();
		});
		// the handler's constructor reads its quest template (AbstractQuestHandler.java:57-64)
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), QUESTS_XML));
		f = cp::makePlayer(800101, 9811, "Dancer");
	}

	void TearDown() override {
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		f = {};
		InWorldPacketTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	/** QuestService.java:165-166: getRewardItems starts its list with the reward items of the reward group */
	static std::vector<QuestItems> rewardItemsOf(int32_t questId) {
		const gameserver::model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
		const std::vector<QuestItems>& items = template_->getRewards().at(0).getRewardItem();
		return {items.begin(), items.end()};
	}

	std::deque<xml::LoadContext> contexts;
	cp::PlayerFixture f;
};

// AbstractQuestHandler.java:277-279: the default answers UNKNOWN (let the other handlers decide) and does not touch the list
TEST_F(AbstractQuestHandlerTest, TheDefaultBonusHookAnswersUnknownAndLeavesTheRewardListAsItIs) {
	DefaultHooksHandler handler(80016);
	AbstractQuestHandler& base = handler;
	Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, *f.player, 80016);
	std::vector<QuestItems> rewardItems = rewardItemsOf(80016);
	ASSERT_EQ(rewardItems.size(), 2u);

	EXPECT_EQ(base.onBonusApplyEvent(*env, BonusType::MOVIE, rewardItems), HandlerResult::UNKNOWN);

	ASSERT_EQ(rewardItems.size(), 2u);
	EXPECT_EQ(rewardItems[0].getItemId(), 188051107);
	EXPECT_EQ(rewardItems[0].getCount(), 1);
	EXPECT_EQ(rewardItems[1].getItemId(), 125040047);
	EXPECT_EQ(rewardItems[1].getCount(), 1);
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
