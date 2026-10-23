// C++-dev only (owner, 2026-10-20): the difficulty menu example in front of Fire Temple (custom/difficulty/DifficultyMenu, FireTempleInstance's
// Hard mode; docs/deviations/C2.md, "C++-dev only: the difficulty menu example"). The menu's choice comes back as HTMLService.getReward's
// message id, the confirmation is a real SM_QUESTION_WINDOW request answered through the player's ResponseRequester, and the entry is a
// test function in place of PortalService::port that creates the Fire Temple instance as port's getNextAvailableInstance would.

#include "InstanceHandlerTestSupport.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/custom/difficulty/DifficultyMenu.h"
#include "aion/gameserver/handlers/instance/FireTempleInstance.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTIONNAIRE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/services/HTMLService.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using custom::difficulty::DifficultyMenu;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_QUESTION_WINDOW;

class DifficultyMenuTest;

/** what the test enter function saw */
struct Entered {
	int8_t difficulty = -1;
	int8_t difficultyOfNewInstance = -1;
	int8_t difficultyOfOtherMaps = -1;
	world::WorldMapInstance* instance = nullptr;
};

DifficultyMenuTest* currentTest = nullptr;
std::optional<Entered> entered;

class DifficultyMenuTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		enabled = configs::main::RuntimeConfig::DIFFICULTY_MENU_ENABLE.exchange(true);
		currentTest = this;
		entered.reset();
		DifficultyMenu::setEnterFunctionForTests(&testEnter);
	}

	void TearDown() override {
		DifficultyMenu::setEnterFunctionForTests(nullptr);
		configs::main::RuntimeConfig::DIFFICULTY_MENU_ENABLE.store(enabled);
		currentTest = nullptr;
		InstanceHandlerTest::TearDown();
	}

	/** PortalService::port's getNextAvailableInstance: a new Fire Temple instance, created on the entering thread */
	static void testEnter(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player, Npc& npc,
		int8_t difficult) {
		static_cast<void>(portalPath);
		static_cast<void>(player);
		static_cast<void>(npc);
		Entered e;
		e.difficulty = difficult;
		e.difficultyOfNewInstance = DifficultyMenu::difficultyOfNewInstance(FIRE_TEMPLE);
		e.difficultyOfOtherMaps = DifficultyMenu::difficultyOfNewInstance(300170000);
		e.instance = &currentTest->create(FIRE_TEMPLE, FireTempleInstance_instanceFactory);
		entered = e;
	}

	/** the player in a Fire Temple instance next to an npc that stands for the entrance portal */
	Npc& portalNear() {
		world::WorldMapInstance& lobby = create(FIRE_TEMPLE, FireTempleInstance_instanceFactory);
		enter(lobby, 127.0f, 176.0f, 99.7f);
		portal = npcs(lobby).at(0);
		(*client)->clearSent();
		return *portal;
	}

	/** the menu offered, answered with the choice items, and the confirmation answered Yes */
	void chooseAndConfirm(std::vector<int32_t> items) {
		Npc& entrance = portalNear();
		ASSERT_TRUE(DifficultyMenu::offerFor(player(), entrance, nullptr, FIRE_TEMPLE));
		runtime::Ptr<custom::difficulty::DifficultyMenuOffer> offer = DifficultyMenu::getOffer(player().getObjectId());
		ASSERT_NE(offer, nullptr);
		services::HTMLService::getReward(runtime::Ptr<model::gameobjects::player::Player>(&player()), offer->messageId, items);
		EXPECT_EQ(DifficultyMenu::getOffer(player().getObjectId()), nullptr) << "answered once";
		ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM, 1)) << "Yes";
	}

	runtime::Ptr<Npc> portal;
	bool enabled = false;
};

/** the switch off: no menu, PortalDialogAI ports at once (Java's entry) */
TEST_F(DifficultyMenuTest, TheSwitchOffKeepsJavasEntry) {
	configs::main::RuntimeConfig::DIFFICULTY_MENU_ENABLE.store(false);
	Npc& entrance = portalNear();
	EXPECT_FALSE(DifficultyMenu::offerFor(player(), entrance, nullptr, FIRE_TEMPLE));
	EXPECT_FALSE(DifficultyMenu::offer(player(), entrance.getAi(), nullptr));
	EXPECT_EQ(DifficultyMenu::getOffer(player().getObjectId()), nullptr);
	EXPECT_TRUE(sent().empty());
}

/** the menu: only for Fire Temple; an HTML poll whose answer comes back as HTMLService.getReward's message id */
TEST_F(DifficultyMenuTest, TheMenuIsAnHtmlPollForFireTempleOnly) {
	Npc& entrance = portalNear();
	EXPECT_FALSE(DifficultyMenu::offerFor(player(), entrance, nullptr, 300170000)) << "another map: Java's entry";
	EXPECT_TRUE(sent().empty());
	ASSERT_TRUE(DifficultyMenu::offerFor(player(), entrance, nullptr, FIRE_TEMPLE));
	runtime::Ptr<custom::difficulty::DifficultyMenuOffer> offer = DifficultyMenu::getOffer(player().getObjectId());
	ASSERT_NE(offer, nullptr);
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_QUESTIONNAIRE(offer->messageId, int8_t{0}, int8_t{1}, DifficultyMenu::html()))), 1);
	EXPECT_NE(DifficultyMenu::html().find(std::to_string(DifficultyMenu::HARD_CHOICE_ITEM_ID)), std::string::npos);

	EXPECT_FALSE(DifficultyMenu::onAnswer(player(), offer->messageId + 1, {DifficultyMenu::HARD_CHOICE_ITEM_ID})) << "another poll";
	EXPECT_NE(DifficultyMenu::getOffer(player().getObjectId()), nullptr);
	EXPECT_TRUE(DifficultyMenu::onAnswer(player(), offer->messageId, {})) << "closed without a choice";
	EXPECT_EQ(DifficultyMenu::getOffer(player().getObjectId()), nullptr);
	EXPECT_FALSE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM, 1)) << "no question";
	EXPECT_FALSE(entered);
}

/** Normal: the confirmation, then the entry with difficulty 0; the new instance is Java's (no scaling, no message) */
TEST_F(DifficultyMenuTest, NormalEntersWithDifficultyZeroAndChangesNothing) {
	chooseAndConfirm({DifficultyMenu::NORMAL_CHOICE_ITEM_ID});
	ASSERT_TRUE(entered);
	EXPECT_EQ(entered->difficulty, 0);
	EXPECT_EQ(entered->difficultyOfNewInstance, 0);
	auto& handler = dynamic_cast<FireTempleInstance&>(*entered->instance->getInstanceHandler());
	EXPECT_EQ(handler.getDifficulty(), 0);
	for (runtime::Ptr<Npc> npc : npcs(*entered->instance))
		EXPECT_EQ(npc->getLifeStats()->getMaxHp(), npc->getObjectTemplate()->getStatsTemplate()->getMaxHp()) << npc->getNpcId();
	(*client)->clearSent();
	handler.onEnterInstance(player());
	EXPECT_TRUE(sent().empty());
}

/** Hard: difficulty 1; every npc of the new instance has twice its HP and half again its attack; the entering player is told */
TEST_F(DifficultyMenuTest, HardEntersWithDifficultyOneAndScalesTheNpcs) {
	commons::utils::Rnd::seedCurrentThreadForTests(7);
	world::WorldMapInstance& normal = create(FIRE_TEMPLE, FireTempleInstance_instanceFactory);
	std::vector<int32_t> normalPhysical;
	for (runtime::Ptr<Npc> npc : npcs(normal))
		normalPhysical.push_back(npc->getGameStats()->getMainHandPAttack({})->getCurrent());
	commons::utils::Rnd::seedCurrentThreadForTests(7);
	chooseAndConfirm({DifficultyMenu::HARD_CHOICE_ITEM_ID});
	ASSERT_TRUE(entered);
	EXPECT_EQ(entered->difficulty, 1);
	EXPECT_EQ(entered->difficultyOfNewInstance, 1) << "set for the instance created while entering";
	EXPECT_EQ(entered->difficultyOfOtherMaps, 0);
	EXPECT_EQ(DifficultyMenu::difficultyOfNewInstance(FIRE_TEMPLE), 0) << "only while entering";
	auto& handler = dynamic_cast<FireTempleInstance&>(*entered->instance->getInstanceHandler());
	EXPECT_EQ(handler.getDifficulty(), 1);
	std::vector<runtime::Ptr<Npc>> hard = npcs(*entered->instance);
	ASSERT_FALSE(hard.empty());
	for (size_t i = 0; i < hard.size(); i++) {
		const int32_t templateHp = hard[i]->getObjectTemplate()->getStatsTemplate()->getMaxHp();
		EXPECT_EQ(hard[i]->getLifeStats()->getMaxHp(), templateHp * 2) << hard[i]->getNpcId();
		EXPECT_EQ(hard[i]->getLifeStats()->getCurrentHp(), templateHp * 2) << "full";
		if (i < normalPhysical.size())
			EXPECT_EQ(hard[i]->getGameStats()->getMainHandPAttack({})->getCurrent(), normalPhysical[i] * 3 / 2) << hard[i]->getNpcId();
	}
	(*client)->clearSent();
	handler.onEnterInstance(player());
	EXPECT_EQ(sent(), std::vector<std::vector<uint8_t>>{serialized(network::aion::serverpackets::SM_MESSAGE(0, "",
						  FireTempleInstance::HARD_MODE_MESSAGE, model::ChatType::BRIGHT_YELLOW_CENTER))});
}

/** No: no entry */
TEST_F(DifficultyMenuTest, NoDoesNotEnter) {
	Npc& entrance = portalNear();
	ASSERT_TRUE(DifficultyMenu::offerFor(player(), entrance, nullptr, FIRE_TEMPLE));
	runtime::Ptr<custom::difficulty::DifficultyMenuOffer> offer = DifficultyMenu::getOffer(player().getObjectId());
	ASSERT_TRUE(DifficultyMenu::onAnswer(player(), offer->messageId, {DifficultyMenu::HARD_CHOICE_ITEM_ID}));
	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_INSTANCE_DUNGEON_WITH_DIFFICULTY_ENTER_CONFIRM, 0));
	EXPECT_FALSE(entered);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
