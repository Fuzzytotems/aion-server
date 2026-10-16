// P5-06a: QuestEngine.onBonusApplyEvent (QuestEngine.java:616-633; the body was ported at M5a) with the reward list as a mutable list of
// values (header request m5d-h01, m5d-plan.md D17(b)). QuestService.getRewardItems hands the engine the list it is building, the engine hands
// the same list to the first handler registered for the bonus type, and a handler may add a new item to it (QuestService.java:197-203).
// Every statement of the dispatcher is asked: the lookup by bonus type (:618), the loop in registration order that skips a quest without a
// handler (:620-622), setQuestId (:623), the handler's answer as the engine's (:624), UNKNOWN (:628) and the catch's FAILED (:629-631), the
// answer on which getRewardItems skips the bonus (QuestService.java:200).
//
// Fixture rows are copied from the shipped data: quests 80016 "[Event] Sock Hop" (quest_data.xml:74986-74995) and 80018 "[Event] Sock it to
// 'Em" (:75011-75020); the list is seeded the way getRewardItems starts it, with a copy of the template's reward items
// (QuestService.java:165-166). The handler is fabricated on the pattern of _80016EventSockHop.java (and _80018EventSockItToEm.java, the same
// class for quest 80018): it registers for BonusType.MOVIE (:31), answers UNKNOWN for another bonus type or quest id (:74-75), and then either
// adds `new QuestItems(188051106, 1)` and answers SUCCESS (:79-85, a quest in REWARD; Java adds the item only on the tenth completion, :80) or
// answers FAILED (:87, a quest not in REWARD). Which of the two it does is the test's choice: the real handler reads QuestState and plays a
// movie (:77-84), which are P5-06's E-01 and H-04 and not what the engine is asked here. A third choice throws, as a handler that
// dereferences null would.
//
// The player is the in-world fixture of tests/cm_ak (InWorldPacketRunSupport.h, included by relative path as QuestDropTest.cpp does).

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
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
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::test {
namespace {

namespace cp = network::aion::clientpackets::testing;
using gameserver::model::templates::quest::QuestItems;
using gameserver::model::templates::rewards::BonusType;
using handlers::HandlerResult;
using runtime::Ref;

/** quest_data.xml:74986-74995 and :75011-75020 */
constexpr const char* QUESTS_XML =
	R"(<quests>)"
	R"(<quest id="80016" name="[Event] Sock Hop" nameId="1180016" quest_zone="Sanctum" minlevel_permitted="10" max_repeat_count="1")"
	R"( cannot_share="true" race_permitted="ELYOS" category="EVENT">)"
	R"(<collect_items><collect_item item_id="182214008" count="15"/></collect_items>)"
	R"(<rewards gold="100000" exp="50000"><reward_item item_id="188051107" count="1"/><reward_item item_id="125040047" count="1"/></rewards>)"
	R"(<quest_drop npc_id="217280" item_id="182214008"/>)"
	R"(</quest>)"
	R"(<quest id="80018" name="[Event] Sock it to 'Em" nameId="1180018" quest_zone="Pandaemonium" minlevel_permitted="10" max_repeat_count="1")"
	R"( cannot_share="true" race_permitted="ASMODIANS" category="EVENT">)"
	R"(<collect_items><collect_item item_id="182214010" count="15"/></collect_items>)"
	R"(<rewards gold="100000" exp="50000"><reward_item item_id="188051107" count="1"/><reward_item item_id="125040047" count="1"/></rewards>)"
	R"(<quest_drop npc_id="217279" item_id="182214010"/>)"
	R"(</quest>)"
	R"(</quests>)";

constexpr int32_t SOCK_HOP = 80016;
constexpr int32_t SOCK_IT_TO_EM = 80018; // _80018EventSockItToEm.java:24; it registers for BonusType.MOVIE too (:31)
constexpr int32_t HAT_BOX = 188051106;   // _80016EventSockHop.java:81 and _80018EventSockItToEm.java:81 "[Event] Hat Box"

/** What the fabricated handler does once the bonus type and the quest id are its own (see the file comment) */
enum class Answer {
	ADD_THE_HAT_BOX, // _80016EventSockHop.java:79-85, the quest in REWARD (and completed 9 times, :80): add the item, SUCCESS
	FAIL,            // :87, the quest not in REWARD: FAILED, the list untouched
	THROW,           // Java's NullPointerException of a handler that dereferences null; QuestEngine.java:629 catches it
};

/** _80016EventSockHop.java:31, :74-75, :79-87, without the quest-state checks and the movie (see the file comment) */
class SockHopBonusHandler final : public handlers::AbstractQuestHandler {
public:
	SockHopBonusHandler(int32_t questId, Answer answer) : AbstractQuestHandler(questId), answer(answer) {}

	void register_() override { qe.registerOnBonusApply(questId, BonusType::MOVIE); }

	HandlerResult onBonusApplyEvent(model::QuestEnv& env, BonusType bonusType, std::vector<QuestItems>& rewardItems) override {
		if (bonusType != BonusType::MOVIE || env.getQuestId() != questId)
			return HandlerResult::UNKNOWN;
		switch (answer) {
			case Answer::ADD_THE_HAT_BOX:
				rewardItems.push_back(QuestItems(HAT_BOX, 1));
				return HandlerResult::SUCCESS;
			case Answer::FAIL:
				return HandlerResult::FAILED;
			case Answer::THROW:
				break;
		}
		throw runtime::NullPointerException("the handler of quest " + std::to_string(questId) + " dereferenced null");
	}

private:
	const Answer answer;
};

class QuestBonusApplyTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// QuestEngine::clear cancels the daily message in the cron service (QuestEngine.java:119; QuestEngineTest.cpp's fixture)
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
		// Player::postConstruct loads the toy pets from the database; the tests have none
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests([](gameserver::model::gameobjects::player::Player&) {
			return std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>>();
		});
		// the handler's constructor reads its quest template (AbstractQuestHandler.java:57-64)
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), QUESTS_XML));
		f = cp::makePlayer(800111, 9812, "Hopper");
	}

	void TearDown() override {
		QuestEngine::getInstance().clear(); // the handlers themselves are Immortal (RT-11)
		services::cron::CronService::resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		f = {};
		InWorldPacketTest::TearDown();
		dataholders::DataManager::QUEST_DATA.resetForTests();
	}

	/** QuestEngine.addQuestHandler registers the handler (register_: registerOnBonusApply(questId, MOVIE)) after the ones added before it */
	static void addHandler(int32_t questId, Answer answer) {
		QuestEngine::getInstance().addQuestHandler(std::make_unique<SockHopBonusHandler>(questId, answer));
	}

	/** QuestService.java:165-166: getRewardItems starts its list with the reward items of the reward group */
	static std::vector<QuestItems> rewardItemsOf(int32_t questId) {
		const gameserver::model::templates::QuestTemplate* template_ = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
		const std::vector<QuestItems>& items = template_->getRewards().at(0).getRewardItem();
		return {items.begin(), items.end()};
	}

	/** both quests have the same two reward items (quest_data.xml:74991-74992, :75016-75017) */
	static void expectTheTemplatesRewardItems(const std::vector<QuestItems>& rewardItems) {
		ASSERT_GE(rewardItems.size(), 2u);
		EXPECT_EQ(rewardItems[0].getItemId(), 188051107);
		EXPECT_EQ(rewardItems[0].getCount(), 1);
		EXPECT_EQ(rewardItems[1].getItemId(), 125040047);
		EXPECT_EQ(rewardItems[1].getCount(), 1);
	}

	static void expectTheTemplatesRewardItemsAndTheHatBox(const std::vector<QuestItems>& rewardItems) {
		ASSERT_EQ(rewardItems.size(), 3u);
		expectTheTemplatesRewardItems(rewardItems);
		EXPECT_EQ(rewardItems[2].getItemId(), HAT_BOX);
		EXPECT_EQ(rewardItems[2].getCount(), 1);
	}

	std::deque<xml::LoadContext> contexts;
	cp::PlayerFixture f;
};

// QuestEngine.java:620-625: the registered handler gets the event, with the env's quest id set to its own; the item it adds is in the
// caller's list (QuestService.java:199 passes the list, :208 returns it as the reward), and its SUCCESS is the engine's
TEST_F(QuestBonusApplyTest, TheRegisteredHandlerAddsANewItemToTheCallersRewardList) {
	addHandler(SOCK_HOP, Answer::ADD_THE_HAT_BOX);
	Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, *f.player, 0);
	std::vector<QuestItems> rewardItems = rewardItemsOf(SOCK_HOP);

	EXPECT_EQ(QuestEngine::getInstance().onBonusApplyEvent(*env, BonusType::MOVIE, rewardItems), HandlerResult::SUCCESS);

	EXPECT_EQ(env->getQuestId(), SOCK_HOP);
	expectTheTemplatesRewardItemsAndTheHatBox(rewardItems);
}

// QuestEngine.java:624: the handler's answer is the engine's, also FAILED (_80016EventSockHop.java:87, the quest not in REWARD), on which
// getRewardItems skips the bonus (QuestService.java:200)
TEST_F(QuestBonusApplyTest, TheHandlersFailedIsTheEnginesAnswer) {
	addHandler(SOCK_HOP, Answer::FAIL);
	Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, *f.player, 0);
	std::vector<QuestItems> rewardItems = rewardItemsOf(SOCK_HOP);

	EXPECT_EQ(QuestEngine::getInstance().onBonusApplyEvent(*env, BonusType::MOVIE, rewardItems), HandlerResult::FAILED);

	EXPECT_EQ(env->getQuestId(), SOCK_HOP);
	ASSERT_EQ(rewardItems.size(), 2u);
	expectTheTemplatesRewardItems(rewardItems);
}

// QuestEngine.java:618-628: no quest registered for the bonus type - UNKNOWN, and neither the env nor the list is touched
TEST_F(QuestBonusApplyTest, ABonusTypeWithoutARegisteredQuestAnswersUnknownAndLeavesTheListAlone) {
	addHandler(SOCK_HOP, Answer::ADD_THE_HAT_BOX);
	Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, *f.player, 0);
	std::vector<QuestItems> rewardItems = rewardItemsOf(SOCK_HOP);

	EXPECT_EQ(QuestEngine::getInstance().onBonusApplyEvent(*env, BonusType::BOSS, rewardItems), HandlerResult::UNKNOWN);

	EXPECT_EQ(env->getQuestId(), 0);
	ASSERT_EQ(rewardItems.size(), 2u);
	expectTheTemplatesRewardItems(rewardItems);
}

// QuestEngine.java:620-623: a quest registered for the bonus type without a handler is skipped without touching the env; the loop goes on
// to the next registered quest, and only when none has a handler is the answer UNKNOWN (:628). Quest 80018 registers for MOVIE
// (_80018EventSockItToEm.java:31); here its handler is not loaded, so the engine finds none for it
TEST_F(QuestBonusApplyTest, ARegisteredQuestWithoutAHandlerIsSkippedForTheNextRegisteredOne) {
	QuestEngine::getInstance().registerOnBonusApply(SOCK_IT_TO_EM, BonusType::MOVIE);
	Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, *f.player, 0);
	std::vector<QuestItems> rewardItems = rewardItemsOf(SOCK_HOP);

	EXPECT_EQ(QuestEngine::getInstance().onBonusApplyEvent(*env, BonusType::MOVIE, rewardItems), HandlerResult::UNKNOWN);
	EXPECT_EQ(env->getQuestId(), 0);
	ASSERT_EQ(rewardItems.size(), 2u);

	addHandler(SOCK_HOP, Answer::ADD_THE_HAT_BOX); // registered after 80018

	EXPECT_EQ(QuestEngine::getInstance().onBonusApplyEvent(*env, BonusType::MOVIE, rewardItems), HandlerResult::SUCCESS);
	EXPECT_EQ(env->getQuestId(), SOCK_HOP);
	expectTheTemplatesRewardItemsAndTheHatBox(rewardItems);
}

// QuestEngine.java:620-624: the engine returns from the first registered quest with a handler, and a later one never sees the event. The
// lookup is by bonus type only (:618), so the env's quest id is overwritten with the first quest's (:623) even when the caller asks for the
// other quest - Java's behaviour, kept: finishing 80018 asks 80016's handler, which then checks 80016's quest state
TEST_F(QuestBonusApplyTest, OnlyTheFirstRegisteredHandlerGetsTheEventWhateverTheEnvsQuest) {
	addHandler(SOCK_HOP, Answer::ADD_THE_HAT_BOX);
	addHandler(SOCK_IT_TO_EM, Answer::ADD_THE_HAT_BOX);
	Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, *f.player, SOCK_IT_TO_EM);
	std::vector<QuestItems> rewardItems = rewardItemsOf(SOCK_IT_TO_EM);

	EXPECT_EQ(QuestEngine::getInstance().onBonusApplyEvent(*env, BonusType::MOVIE, rewardItems), HandlerResult::SUCCESS);

	EXPECT_EQ(env->getQuestId(), SOCK_HOP);
	expectTheTemplatesRewardItemsAndTheHatBox(rewardItems); // one hat box: 80018's handler was not asked
}

// QuestEngine.java:629-631: an exception from the handler is logged and the answer is FAILED, not UNKNOWN - getRewardItems then skips the
// bonus (QuestService.java:200)
TEST_F(QuestBonusApplyTest, AHandlerThatThrowsMakesTheEngineAnswerFailed) {
	addHandler(SOCK_HOP, Answer::THROW);
	Ref<model::QuestEnv> env = model::QuestEnv::create(nullptr, *f.player, 0);
	std::vector<QuestItems> rewardItems = rewardItemsOf(SOCK_HOP);

	EXPECT_EQ(QuestEngine::getInstance().onBonusApplyEvent(*env, BonusType::MOVIE, rewardItems), HandlerResult::FAILED);

	EXPECT_EQ(env->getQuestId(), SOCK_HOP);
	ASSERT_EQ(rewardItems.size(), 2u);
}

} // namespace
} // namespace aion::gameserver::questEngine::test
