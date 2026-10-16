// Q03's tests of its two hand-ported Heiron quests (P6-Q slice 2, 2026-09-29): _1643TheStarOfHeiron and _3200PriceOfGoodwill, every hook
// and step through the real QuestEngine on the fixture's World (Q03QuestTestSupport.h), the anonymous Runnables' tasks on its ManualClock.
// The handlers come from Q03's library, which this executable links, through their registry factories.

#include "Q03QuestTestSupport.h"

#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"

namespace aion::gameserver::handlers::quest::heiron {
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _1643TheStarOfHeiron_questFactory();
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _3200PriceOfGoodwill_questFactory();
} // namespace aion::gameserver::handlers::quest::heiron

namespace aion::gameserver::questEngine::handlers::q03::test {

namespace DA = gameserver::model::DialogAction;
namespace heiron = ::aion::gameserver::handlers::quest::heiron;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_PLAY_MOVIE;

class HeironQuestTest : public Q03QuestTest {
protected:
	void setUpQuest(std::unique_ptr<AbstractQuestHandler> created, int32_t id) {
		questId = id;
		install(std::move(created));
		spawnElyos();
	}

	bool talk(Ptr<gameserver::model::gameobjects::VisibleObject> target, int32_t action) {
		clearSent();
		return dialog(target, questId, action);
	}

	bool talk(Npc& npc, int32_t action) { return talk(Ptr<gameserver::model::gameobjects::VisibleObject>(npc), action); }

	/** SM_DIALOG_WINDOW(objectId, page): the constructor without a quest id (quest 0) */
	static std::vector<uint8_t> window(int32_t objectId, int32_t page) { return dialogWindow(objectId, page, 0); }

	/** The handler's own onEnterWorldEvent answer */
	bool enterWorldHook() {
		Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), questId);
		envs.push_back(env);
		return handler->onEnterWorldEvent(*env);
	}

	int32_t questId = 0;
};

// ---- 1643 The Star of Heiron --------------------------------------------------------------------------------------------------------------

inline constexpr int32_t LISKE = 204545;         // the quest giver and rewarder
inline constexpr int32_t ERATO = 204630;
inline constexpr int32_t PRAPERO_SPIRIT = 204614; // spawned by Erato's SETPRO1 (_1643TheStarOfHeiron.java:74)
inline constexpr int32_t HAZY_DISK = 182201764;   // 1643's work item (quest_data.xml:5720)
inline constexpr int32_t PLATINUM_COIN = 186000005; // 1643's reward item, 4 (quest_data.xml:5714)

class StarOfHeironTest : public HeironQuestTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (!IsSkipped())
			setUpQuest(heiron::_1643TheStarOfHeiron_questFactory(), 1643);
	}
};

TEST_F(StarOfHeironTest, RegisterNamesEnterWorldTheGiverAndTheTwoTalkNpcs) {
	// _1643TheStarOfHeiron.java:26-30, in this order (tools/parity checks the statement order)
	EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(LISKE)->getOnQuestStart().contains(1643));
	for (int32_t npcId : {LISKE, ERATO, PRAPERO_SPIRIT})
		EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().snapshot(), std::vector<int32_t>{1643}) << npcId;
	EXPECT_FALSE(QuestEngine::getInstance().getQuestNpc(ERATO)->getOnQuestStart().contains(1643));
	// the enter-world registration: the engine's enter world reaches the hook (var 1 goes back to 0)
	hold(1643, QuestStatus::START, 1);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(1643), 0);
}

TEST_F(StarOfHeironTest, LiskeOffersTheQuestAndAcceptingGivesTheHazyDisk) {
	hold(1642, QuestStatus::COMPLETE); // the start condition (quest_data.xml:5717)
	Npc& liske = spawnNpc(LISKE);
	EXPECT_TRUE(talk(liske, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(liske.getObjectId(), 4762, 1643)));
	EXPECT_TRUE(talk(liske, DA::ASK_QUEST_ACCEPT)) << "sendQuestStartDialog(env, 182201764, 1): ASK_QUEST_ACCEPT sends page 4";
	EXPECT_TRUE(wasSent(dialogWindow(liske.getObjectId(), 4, 1643)));
	EXPECT_FALSE(stateOf(1643));
	EXPECT_TRUE(talk(liske, DA::QUEST_ACCEPT_1));
	ASSERT_TRUE(stateOf(1643));
	EXPECT_EQ(stateOf(1643)->getStatus(), QuestStatus::START);
	EXPECT_EQ(held(HAZY_DISK), 1);
	EXPECT_TRUE(wasSent(dialogWindow(liske.getObjectId(), 1003, 1643)));
	EXPECT_FALSE(talk(liske, DA::QUEST_SELECT)) << "START: Liske has no START arm (only 204630 and 204614)";
}

TEST_F(StarOfHeironTest, OnlyLiskeStartsItAndNothingAnswersWithoutTheQuest) {
	Npc& erato = spawnNpc(ERATO);
	EXPECT_FALSE(talk(erato, DA::QUEST_SELECT));
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(stateOf(1643));
}

TEST_F(StarOfHeironTest, EratoTakesTheDiskAndSpawnsPraperosSpiritForFiveMinutes) {
	hold(1643, QuestStatus::START, 0);
	give(HAZY_DISK, 1);
	Npc& erato = spawnNpc(ERATO);
	EXPECT_TRUE(talk(erato, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(erato.getObjectId(), 1011, 1643)));
	EXPECT_FALSE(talk(erato, DA::SETPRO2)) << "no such case: return false (:84)";

	EXPECT_TRUE(talk(erato, DA::SETPRO1));
	EXPECT_EQ(varOf(1643), 1);
	EXPECT_EQ(held(HAZY_DISK), 0);
	EXPECT_TRUE(wasSent(questUpdate(1643, START, 1)));
	EXPECT_TRUE(wasSent(window(erato.getObjectId(), 0)));
	Ptr<Npc> spirit = player().getWorldMapInstance()->getNpc(PRAPERO_SPIRIT);
	ASSERT_TRUE(spirit);
	spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(spirit));
	EXPECT_FLOAT_EQ(spirit->getX(), 1591.4327f);
	EXPECT_FLOAT_EQ(spirit->getY(), 2774.2283f);
	EXPECT_FLOAT_EQ(spirit->getZ(), 127.63001f);
	EXPECT_EQ(spirit->getHeading(), 0);
	EXPECT_FALSE(talk(erato, DA::QUEST_SELECT)) << "var 1: neither 0 nor 2";
	executor().advance(std::chrono::milliseconds(300000));
	EXPECT_FALSE(spirit->isSpawned()) << "spawnForFiveMinutes";
}

TEST_F(StarOfHeironTest, TheSpiritTakesStep2AndLeavesFortySecondsLater) {
	hold(1643, QuestStatus::START, 0);
	Npc& spirit = spawnNpc(PRAPERO_SPIRIT);
	EXPECT_FALSE(talk(spirit, DA::QUEST_SELECT)) << "var 0";
	stateOf(1643)->setQuestVarById(0, 1);
	EXPECT_TRUE(talk(spirit, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(spirit.getObjectId(), 1011, 1643)));
	EXPECT_FALSE(talk(spirit, DA::SETPRO2)) << "no such case: the switch ends and :117 returns false";

	EXPECT_TRUE(talk(spirit, DA::SETPRO1));
	EXPECT_EQ(varOf(1643), 2);
	EXPECT_TRUE(wasSent(questUpdate(1643, START, 2)));
	EXPECT_TRUE(wasSent(window(spirit.getObjectId(), 10)));
	executor().advance(std::chrono::milliseconds(39999));
	EXPECT_TRUE(spirit.isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(spirit.isSpawned()) << "the Runnable of _1643TheStarOfHeiron.java:98, 40000 ms";
}

TEST_F(StarOfHeironTest, EratoSendsTheLastPageAndSetSucceedSetsTheReward) {
	hold(1643, QuestStatus::START, 2);
	Npc& erato = spawnNpc(ERATO);
	EXPECT_TRUE(talk(erato, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(erato.getObjectId(), 1693, 1643)));
	EXPECT_TRUE(talk(erato, DA::SET_SUCCEED));
	EXPECT_EQ(stateOf(1643)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1643), 2);
	EXPECT_TRUE(wasSent(questUpdate(1643, REWARD, 2)));
	EXPECT_TRUE(wasSent(window(erato.getObjectId(), 0)));
}

TEST_F(StarOfHeironTest, EratosAndTheSpiritsArmsHaveNoVarGuardAJavaBugKept) {
	// _1643TheStarOfHeiron.java:69-82 and :93-105 (the Java review of 2026-09-29; U3, marked `// java-bug kept` in the port): SETPRO1 at Erato
	// adds 1 and spawns another spirit at any START var, SET_SUCCEED at Erato sets REWARD at var 0, and SETPRO1 at the spirit adds 1 at any var
	Ref<QuestState> qs = hold(1643, QuestStatus::START, 2);
	Npc& erato = spawnNpc(ERATO);
	EXPECT_TRUE(talk(erato, DA::SETPRO1)) << "var 2";
	EXPECT_EQ(varOf(1643), 3);
	std::vector<Ptr<Npc>> spirits = player().getWorldMapInstance()->getNpcs({PRAPERO_SPIRIT});
	for (const Ptr<Npc>& spirit : spirits)
		spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(spirit));
	EXPECT_EQ(spirits.size(), 1u) << "a spirit at var 2 as well";
	Npc& spirit = spawnNpc(PRAPERO_SPIRIT, 4.0f);
	EXPECT_TRUE(talk(spirit, DA::SETPRO1)) << "var 3";
	EXPECT_EQ(varOf(1643), 4);
	qs->setQuestVar(0);
	EXPECT_TRUE(talk(erato, DA::SET_SUCCEED)) << "var 0: the quest skips to REWARD";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1643), 0);
}

TEST_F(StarOfHeironTest, LiskeShowsTheRewardAndFinishesTheQuest) {
	hold(1643, QuestStatus::REWARD, 2);
	Npc& liske = spawnNpc(LISKE);
	Npc& erato = spawnNpc(ERATO, 4.0f);
	EXPECT_FALSE(talk(erato, DA::QUEST_SELECT)) << "REWARD: only Liske";
	EXPECT_TRUE(talk(liske, DA::SELECT_QUEST_REWARD));
	EXPECT_TRUE(wasSent(dialogWindow(liske.getObjectId(), 5, 1643)));
	talk(liske, DA::SELECTED_QUEST_NOREWARD);
	EXPECT_EQ(stateOf(1643)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(PLATINUM_COIN), 4) << "quest_data.xml:5713-5715";
}

TEST_F(StarOfHeironTest, EnterWorldPutsStep1BackTo0AndAnswersFalse) {
	EXPECT_FALSE(enterWorldHook()) << "no quest";
	Ref<QuestState> qs = hold(1643, QuestStatus::START, 1);
	clearSent();
	EXPECT_FALSE(enterWorldHook());
	EXPECT_EQ(varOf(1643), 0) << "qs.setQuestVar(0): the spirit Erato spawned is gone after a relog";
	EXPECT_TRUE(wasSent(questUpdate(1643, START, 0)));
	qs->setQuestVar(2);
	clearSent();
	EXPECT_FALSE(enterWorldHook());
	EXPECT_EQ(varOf(1643), 2);
	EXPECT_TRUE(sent().empty());
	qs->setStatus(QuestStatus::REWARD);
	qs->setQuestVar(1);
	EXPECT_FALSE(enterWorldHook());
	EXPECT_EQ(varOf(1643), 1) << "only in START";
}

// ---- 3200 Price of Goodwill ---------------------------------------------------------------------------------------------------------------

inline constexpr int32_t ROIKINERK = 204658;
inline constexpr int32_t HAORUNERK = 798332;
inline constexpr int32_t HAORUNERKS_BAG = 700522;
inline constexpr int32_t GARKBINERK = 279006;
inline constexpr int32_t KURUMINERK = 798322;
inline constexpr int32_t HAORUNERKS_CORPSE = 798333; // in Steel Rake (spawns/Instances/300100000_Steel Rake.xml:124-126)
inline constexpr int32_t TELEPORT_SCROLL = 182209082; // registered quest item (_3200PriceOfGoodwill.java:39)

class PriceOfGoodwillTest : public HeironQuestTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (!IsSkipped())
			setUpQuest(heiron::_3200PriceOfGoodwill_questFactory(), 3200);
	}

	HandlerResult use(int32_t itemId) {
		give(itemId, 1);
		return itemUseHook(*itemOf(itemId));
	}
};

TEST_F(PriceOfGoodwillTest, RegisterNamesTheGiverTheScrollAndTheFiveTalkNpcs) {
	// _3200PriceOfGoodwill.java:38-41: the start npc, the quest item, then the talk npcs of npc_ids in their order
	EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(ROIKINERK)->getOnQuestStart().contains(3200));
	EXPECT_TRUE(QuestEngine::getInstance().isRegisteredQuestItem(TELEPORT_SCROLL));
	for (int32_t npcId : {ROIKINERK, HAORUNERK, HAORUNERKS_BAG, GARKBINERK, KURUMINERK})
		EXPECT_EQ(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().snapshot(), std::vector<int32_t>{3200}) << npcId;
	EXPECT_FALSE(QuestEngine::getInstance().getQuestNpc(HAORUNERKS_CORPSE)->getOnTalkEvent().contains(3200));
}

TEST_F(PriceOfGoodwillTest, RoikinerkOffersTheQuestAndOtherNpcsAnswerFalse) {
	Npc& roikinerk = spawnNpc(ROIKINERK);
	Npc& garkbinerk = spawnNpc(GARKBINERK, 4.0f);
	EXPECT_FALSE(talk(garkbinerk, DA::QUEST_SELECT)) << ":62";
	EXPECT_TRUE(talk(roikinerk, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(roikinerk.getObjectId(), 4762, 3200)));
	EXPECT_TRUE(talk(roikinerk, DA::QUEST_ACCEPT_1)) << "sendQuestStartDialog(env)";
	ASSERT_TRUE(stateOf(3200));
	EXPECT_EQ(stateOf(3200)->getStatus(), QuestStatus::START);
	EXPECT_EQ(varOf(3200), 0);
}

TEST_F(PriceOfGoodwillTest, HaorunerkPlaysMovie431AndSetsStep2) {
	hold(3200, QuestStatus::START, 1);
	Npc& haorunerk = spawnNpc(HAORUNERK);
	EXPECT_TRUE(talk(haorunerk, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(haorunerk.getObjectId(), 1352, 3200)));
	EXPECT_FALSE(talk(haorunerk, DA::SELECT2_1)) << ":95-97: the movie, then break and return false";
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, haorunerk.getObjectId(), 3200, 431, true))));
	EXPECT_TRUE(talk(haorunerk, DA::SETPRO2));
	EXPECT_EQ(varOf(3200), 2);
	EXPECT_TRUE(wasSent(questUpdate(3200, START, 2)));
	EXPECT_FALSE(talk(haorunerk, DA::QUEST_SELECT)) << "var 2: Haorunerk's arm is var == 1 only";
}

TEST_F(PriceOfGoodwillTest, TheBagUpdatesTheQuestThreeSecondsAfterAnyTalk) {
	Ref<QuestState> qs = hold(3200, QuestStatus::START, 1);
	Npc& bag = spawnNpc(HAORUNERKS_BAG);
	EXPECT_FALSE(talk(bag, DA::USE_OBJECT)) << "var 1";
	qs->setQuestVar(2);
	EXPECT_TRUE(talk(bag, DA::USE_OBJECT));
	EXPECT_TRUE(sent().empty()) << "nothing before the task";
	executor().advance(std::chrono::milliseconds(2999));
	EXPECT_TRUE(sent().empty());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(wasSent(questUpdate(3200, START, 2))) << "the Runnable of _3200PriceOfGoodwill.java:102: updateQuestStatus(env) after 3000 ms";
	EXPECT_EQ(varOf(3200), 2);
	clearSent();
	EXPECT_TRUE(talk(bag, DA::QUEST_SELECT)) << "any dialog action";
	executor().advance(std::chrono::milliseconds(3000));
	EXPECT_TRUE(wasSent(questUpdate(3200, START, 2)));
	// the bag's arm is `var == 2` only (:101): at step 3 (Garkbinerk's) it answers false and schedules nothing (the review of 2026-09-29)
	qs->setQuestVar(3);
	clearSent();
	EXPECT_FALSE(talk(bag, DA::USE_OBJECT)) << "var 3";
	executor().advance(std::chrono::milliseconds(3000));
	EXPECT_TRUE(sent().empty()) << "no update task at var 3";
}

TEST_F(PriceOfGoodwillTest, GarkbinerkSendsItsPageAndSetSucceedSetsTheReward) {
	hold(3200, QuestStatus::START, 3);
	Npc& garkbinerk = spawnNpc(GARKBINERK);
	EXPECT_TRUE(talk(garkbinerk, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(garkbinerk.getObjectId(), 2034, 3200)));
	EXPECT_FALSE(talk(garkbinerk, DA::SETPRO1)) << "no such case";
	EXPECT_TRUE(talk(garkbinerk, DA::SET_SUCCEED)) << "defaultCloseDialog(env, 3, 3, true, false)";
	EXPECT_EQ(stateOf(3200)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(3200), 3);
	EXPECT_TRUE(wasSent(questUpdate(3200, REWARD, 3)));
}

TEST_F(PriceOfGoodwillTest, InStartTheNpcsAnswerOnlyAtTheirStep) {
	hold(3200, QuestStatus::START, 1);
	Npc& roikinerk = spawnNpc(ROIKINERK);
	Npc& garkbinerk = spawnNpc(GARKBINERK, 4.0f);
	Npc& kuruminerk = spawnNpc(KURUMINERK, 6.0f);
	EXPECT_FALSE(talk(roikinerk, DA::QUEST_SELECT)) << "var 1: Roikinerk's arm is var == 0";
	EXPECT_FALSE(talk(garkbinerk, DA::QUEST_SELECT)) << "var 1";
	EXPECT_FALSE(talk(kuruminerk, DA::USE_OBJECT)) << "START: Kuruminerk answers in REWARD only";
	stateOf(3200)->setQuestVar(0);
	EXPECT_TRUE(talk(roikinerk, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(roikinerk.getObjectId(), 1003, 3200)));
	EXPECT_FALSE(talk(roikinerk, DA::SETPRO2)) << "no such case";
}

TEST_F(PriceOfGoodwillTest, KuruminerkShowsHisPagesAndFinishesTheQuest) {
	hold(3200, QuestStatus::REWARD, 3);
	Npc& kuruminerk = spawnNpc(KURUMINERK);
	Npc& garkbinerk = spawnNpc(GARKBINERK, 4.0f);
	EXPECT_FALSE(talk(garkbinerk, DA::QUEST_SELECT)) << "REWARD: :77";
	EXPECT_TRUE(talk(kuruminerk, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(dialogWindow(kuruminerk.getObjectId(), 10002, 3200)));
	EXPECT_TRUE(talk(kuruminerk, DA::SELECT_QUEST_REWARD));
	EXPECT_TRUE(wasSent(dialogWindow(kuruminerk.getObjectId(), 5, 3200)));
	int64_t expBefore = player().getCommonData()->getExp();
	talk(kuruminerk, DA::SELECTED_QUEST_NOREWARD);
	EXPECT_EQ(stateOf(3200)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getCommonData()->getExp() - expBefore, 1974220) << "quest_data.xml:20169";
}

TEST_F(PriceOfGoodwillTest, AnyOtherItemOrStepIsUnknown) {
	EXPECT_EQ(use(HAZY_DISK), HandlerResult::UNKNOWN) << "another item";
	EXPECT_EQ(use(TELEPORT_SCROLL), HandlerResult::UNKNOWN) << "no quest";
	Ref<QuestState> qs = hold(3200, QuestStatus::START, 1);
	clearSent();
	EXPECT_EQ(itemUseHook(*itemOf(TELEPORT_SCROLL)), HandlerResult::UNKNOWN) << "var 1";
	qs->setQuestVar(3);
	EXPECT_EQ(itemUseHook(*itemOf(TELEPORT_SCROLL)), HandlerResult::UNKNOWN) << "var 3";
	executor().advance(std::chrono::milliseconds(3000));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(held(TELEPORT_SCROLL), 1) << "nothing removed";
}

TEST_F(PriceOfGoodwillTest, TheScrollAnimatesThenTakesThePlayerToReshantaAndSetsStep3) {
	hold(3200, QuestStatus::START, 2);
	give(TELEPORT_SCROLL, 1);
	Ptr<gameserver::model::gameobjects::Item> scroll = itemOf(TELEPORT_SCROLL);
	ASSERT_TRUE(scroll);
	int32_t scrollObjId = scroll->getObjectId();
	clearSent();
	EXPECT_EQ(useItem(*scroll), HandlerResult::SUCCESS) << "through QuestEngine.onItemUseEvent (the registered quest item)";
	EXPECT_TRUE(wasSent(serializedFor(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), scrollObjId, TELEPORT_SCROLL, 3000, 0, 0))));
	EXPECT_EQ(varOf(3200), 2);
	EXPECT_EQ(held(TELEPORT_SCROLL), 1);
	executor().advance(std::chrono::milliseconds(2999));
	EXPECT_EQ(player().getWorldId(), HEIRON);
	EXPECT_EQ(held(TELEPORT_SCROLL), 1);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(wasSent(serializedFor(SM_ITEM_USAGE_ANIMATION(player().getObjectId(), scrollObjId, TELEPORT_SCROLL, 0, 1, 0))));
	EXPECT_EQ(held(TELEPORT_SCROLL), 0);
	EXPECT_EQ(player().getWorldId(), RESHANTA);
	EXPECT_FLOAT_EQ(player().getX(), 3419.16f);
	EXPECT_FLOAT_EQ(player().getY(), 2445.43f);
	EXPECT_FLOAT_EQ(player().getZ(), 2766.54f);
	EXPECT_EQ(player().getHeading(), 57);
	EXPECT_EQ(varOf(3200), 3);
	EXPECT_TRUE(wasSent(questUpdate(3200, START, 3)));
}

TEST_F(PriceOfGoodwillTest, RoikinerkSendsThePlayerIntoANewSteelRakeWhereHaorunerkReplacesHisCorpse) {
	if (!needDatabase())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the instance case (SpawnEngine.spawnInstance reads the houses)";
	hold(3200, QuestStatus::START, 0);
	Npc& roikinerk = spawnNpc(ROIKINERK);
	EXPECT_TRUE(talk(roikinerk, DA::SETPRO1));
	EXPECT_EQ(varOf(3200), 1) << "defaultCloseDialog(env, 0, 1)";
	EXPECT_EQ(player().getWorldId(), STEEL_RAKE);
	EXPECT_GT(player().getInstanceId(), 1) << "InstanceService.getNextAvailableInstance: a new instance";
	EXPECT_FLOAT_EQ(player().getX(), 403.55f);
	EXPECT_FLOAT_EQ(player().getY(), 508.11f);
	EXPECT_FLOAT_EQ(player().getZ(), 885.77f);
	Ptr<world::WorldMapInstance> steelRake = world::World::getInstance().getWorldMap(STEEL_RAKE)->getWorldMapInstance(player().getInstanceId());
	ASSERT_TRUE(steelRake);
	EXPECT_FALSE(steelRake->getNpc(HAORUNERKS_CORPSE)) << "the corpse is deleted";
	Ptr<Npc> haorunerk = steelRake->getNpc(HAORUNERK);
	ASSERT_TRUE(haorunerk) << "spawned where the corpse lay";
	EXPECT_FLOAT_EQ(haorunerk->getX(), 406.312f);
	EXPECT_FLOAT_EQ(haorunerk->getY(), 499.564f);
	EXPECT_FLOAT_EQ(haorunerk->getZ(), 885.76f);
	EXPECT_EQ(haorunerk->getHeading(), 30);
	EXPECT_TRUE(steelRake->getNpc(HAORUNERKS_BAG)) << "the rest of the instance's spawns stay";
}

} // namespace aion::gameserver::questEngine::handlers::q03::test
