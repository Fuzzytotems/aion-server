// Q09's tests of the two hand-ported Poeta quests of chunk Q05 (route-hand lane, phase 6, 2026-09-29): _1002RequestoftheElim and
// _1114TheNymphsGown, every hook and step through the real QuestEngine on the fixture's World (ZoneQuestTestSupport.h). The Karamatis leg of
// 1002 (Daminu's SETPRO5 and Belpartan inside the instance) is SoloInstanceQuestTest.cpp's.
//
// Q09's test executable links Q09's library only, so this file compiles the two Q05 handlers by #include, the way P5-05's test executable
// compiles the two quest npc AIs of A1 (chunks.cmake, the P5-05 lease row).

#include "../../handlers/aion/gameserver/handlers/quest/poeta/_1002RequestoftheElim.cpp"
#include "../../handlers/aion/gameserver/handlers/quest/poeta/_1114TheNymphsGown.cpp"

#include "ZoneQuestTestSupport.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ASCENSION_MORPH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"

namespace aion::gameserver::questEngine::handlers::zones::test {

namespace DA = gameserver::model::DialogAction;
using network::aion::serverpackets::SM_ASCENSION_MORPH;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_PLAY_MOVIE;

inline constexpr int32_t AMPEIS = 203076;
inline constexpr int32_t NOAH = 730007;
inline constexpr int32_t SLEEPING_ELDER = 730010;
inline constexpr int32_t DAMINU = 730008;
inline constexpr int32_t BELPARTAN = 205000;
inline constexpr int32_t KALIO = 203067;
inline constexpr int32_t FLUTE = 182200002;      // The Flute Of The Elim, 1002's work item
inline constexpr int32_t KOBOLD_AXE = 182200003; // 1002's collect item (quest_data.xml:22, count 3)

inline constexpr int32_t NAMUS = 203075;
inline constexpr int32_t ASTEROS = 203058;
inline constexpr int32_t SEIRENIA_CLOTHES = 700008;
inline constexpr int32_t SEIRENIA = 203175;
inline constexpr int32_t NAMUS_DIARY = 182200214;
inline constexpr int32_t DIARY_PAGE = 182200226;
inline constexpr int32_t NYMPHS_DRESS = 182200217;

class RequestOfTheElimTest : public ZoneQuestTest {
protected:
	void SetUp() override {
		ZoneQuestTest::SetUp();
		if (IsSkipped())
			return;
		install(::aion::gameserver::handlers::quest::poeta::_1002RequestoftheElim_questFactory());
		spawnActor(gameserver::model::Race::ELYOS, 8, POETA, 1000.0f, 1000.0f, 100.0f);
	}

	/** One dialog of 1002 at the npc, the packets of the step cleared first */
	bool talk(Npc& npc, int32_t action) {
		clearSent();
		return dialog(Ptr<gameserver::model::gameobjects::VisibleObject>(npc), 1002, action);
	}
};

TEST_F(RequestOfTheElimTest, RegisterNamesTheSixTalkNpcs) {
	for (int32_t npcId : {AMPEIS, NOAH, SLEEPING_ELDER, DAMINU, BELPARTAN, KALIO})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().contains(1002)) << npcId;
}

TEST_F(RequestOfTheElimTest, AmpeisOpensTheMissionAndSetsStep1) {
	hold(1002, QuestStatus::START, 0);
	Npc& ampeis = spawnNpc(AMPEIS);

	EXPECT_TRUE(talk(ampeis, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(ampeis.getObjectId(), 1011, 1002)));
	EXPECT_TRUE(talk(ampeis, DA::SETPRO1));
	EXPECT_EQ(varOf(1002), 1);
	EXPECT_TRUE(wasSent(questUpdate(1002, START, 1)));

	EXPECT_FALSE(talk(ampeis, DA::QUEST_SELECT)) << "_1002RequestoftheElim.java:57-60: var 1 is no longer Ampeis' step";
}

TEST_F(RequestOfTheElimTest, NoahPlaysMovie20AndHandsTheFlute) {
	hold(1002, QuestStatus::START, 1);
	Npc& noah = spawnNpc(NOAH);

	EXPECT_TRUE(talk(noah, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 1352, 1002)));
	EXPECT_TRUE(talk(noah, DA::SELECT2_1));
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, noah.getObjectId(), 1002, 20, true))));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 1353, 1002)));
	EXPECT_TRUE(talk(noah, DA::SETPRO2));
	EXPECT_EQ(varOf(1002), 2);
	EXPECT_EQ(held(FLUTE), 1);
}

TEST_F(RequestOfTheElimTest, TheSleepingEldersNeedTheFluteAndStepTwoThenFour) {
	hold(1002, QuestStatus::START, 2);
	Npc& first = spawnNpc(SLEEPING_ELDER);

	EXPECT_FALSE(talk(first, DA::USE_OBJECT)) << "no flute: _1002RequestoftheElim.java:103";
	EXPECT_EQ(varOf(1002), 2);
	EXPECT_TRUE(first.isSpawned());

	give(FLUTE, 1);
	EXPECT_TRUE(talk(first, DA::USE_OBJECT));
	EXPECT_EQ(varOf(1002), 4) << "useQuestObject(env, 2, 4, false, false)";
	EXPECT_FALSE(first.isSpawned()) << "deleteAndScheduleRespawn";

	Npc& second = spawnNpc(SLEEPING_ELDER, 4.0f);
	EXPECT_TRUE(talk(second, DA::USE_OBJECT));
	EXPECT_EQ(varOf(1002), 5);
	EXPECT_FALSE(second.isSpawned());
}

TEST_F(RequestOfTheElimTest, OnCanActLetsOnlySteps2And4UseTheSleepingElder) {
	Npc& elder = spawnNpc(SLEEPING_ELDER);
	Npc& noah = spawnNpc(NOAH, 4.0f);
	auto canAct = [&](Npc& target) {
		Ref<QuestEnv> env = QuestEnv::create(Ptr<gameserver::model::gameobjects::VisibleObject>(target), player(), 1002, DA::USE_OBJECT);
		envs.push_back(env);
		return handler->onCanAct(*env, model::QuestActionType::ACTION_ITEM_USE,
			std::span<const std::any>());
	};
	EXPECT_FALSE(canAct(elder)) << "no quest state";
	EXPECT_TRUE(canAct(noah)) << "any other target";
	Ref<QuestState> qs = hold(1002, QuestStatus::START, 2);
	EXPECT_TRUE(canAct(elder));
	qs->setQuestVarById(0, 4);
	EXPECT_TRUE(canAct(elder));
	qs->setQuestVarById(0, 3);
	EXPECT_FALSE(canAct(elder));
	qs->setQuestVarById(0, 2);
	qs->setStatus(QuestStatus::REWARD);
	EXPECT_FALSE(canAct(elder));
}

TEST_F(RequestOfTheElimTest, NoahTakesTheFluteThenChecksTheThreeKoboldAxes) {
	hold(1002, QuestStatus::START, 5);
	give(FLUTE, 1);
	Npc& noah = spawnNpc(NOAH);

	EXPECT_TRUE(talk(noah, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 1693, 1002)));
	EXPECT_TRUE(talk(noah, DA::SETPRO3));
	EXPECT_EQ(varOf(1002), 6);
	EXPECT_EQ(held(FLUTE), 0);

	EXPECT_TRUE(talk(noah, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 2034, 1002)));
	give(KOBOLD_AXE, 2);
	EXPECT_TRUE(talk(noah, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 2205, 1002))) << "two of three axes: the fail page";
	EXPECT_EQ(varOf(1002), 6);
	give(KOBOLD_AXE, 1);
	EXPECT_TRUE(talk(noah, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 2120, 1002)));
	EXPECT_EQ(varOf(1002), 12);
	EXPECT_EQ(held(KOBOLD_AXE), 0);

	EXPECT_TRUE(talk(noah, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 2120, 1002))) << ":91-92: var 12 shows the page again";
	EXPECT_TRUE(talk(noah, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 2120, 1002)));
	EXPECT_TRUE(talk(noah, DA::SETPRO4));
	EXPECT_EQ(varOf(1002), 13);
	EXPECT_TRUE(talk(noah, DA::FINISH_DIALOG));
	EXPECT_TRUE(wasSent(dialogWindow(noah.getObjectId(), 10, 0))) << "sendQuestSelectionDialog";
}

TEST_F(RequestOfTheElimTest, DaminuShowsThePagesOfSteps13And14AndSetsTheRewardAtStep14) {
	Ref<QuestState> qs = hold(1002, QuestStatus::START, 13);
	Npc& daminu = spawnNpc(DAMINU);

	EXPECT_TRUE(talk(daminu, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(daminu.getObjectId(), 2375, 1002)));
	qs->setQuestVarById(0, 14);
	EXPECT_TRUE(talk(daminu, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(daminu.getObjectId(), 2461, 1002)));
	EXPECT_TRUE(talk(daminu, DA::SETPRO6));
	EXPECT_EQ(stateOf(1002)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1002), 14);
}

TEST_F(RequestOfTheElimTest, BelpartanFliesThePlayerAndAfter43SecondsSetsStep14AndPortsHimToPoeta) {
	hold(1002, QuestStatus::START, 20);
	Npc& belpartan = spawnNpc(BELPARTAN);

	EXPECT_FALSE(talk(belpartan, DA::SETPRO1)) << "only QUEST_SELECT";
	EXPECT_TRUE(talk(belpartan, DA::QUEST_SELECT));
	EXPECT_TRUE(player().isInState(gameserver::model::gameobjects::state::CreatureState::FLYING));
	EXPECT_FALSE(player().isInState(gameserver::model::gameobjects::state::CreatureState::ACTIVE));
	EXPECT_TRUE(wasSent(serializedFor(SM_EMOTION(player(), gameserver::model::EmotionType::START_FLYTELEPORT, 1001, 0))));
	ASSERT_TRUE(player().getFlightPath()) << "player.setFlightTeleportId(1001): Player.java:809-810";
	EXPECT_EQ(player().getFlightPath()->getType(), gameserver::model::templates::flypath::FlightPath_Type::FLIGHT_TRANSPORTER);
	EXPECT_EQ(player().getFlightPath()->getId(), 1001);
	EXPECT_EQ(varOf(1002), 20);

	executor().advance(std::chrono::milliseconds(42999));
	EXPECT_EQ(varOf(1002), 20) << "the task runs after 43000 ms";
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(varOf(1002), 14) << "changeQuestStep(env, 20, 14)";
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), 603.0f) << "teleportTo(player, 210010000, 1, 603, 1537, 116, 20)";
	EXPECT_FLOAT_EQ(player().getY(), 1537.0f);
	EXPECT_FLOAT_EQ(player().getZ(), 116.0f);
	EXPECT_EQ(player().getHeading(), 20);
}

TEST_F(RequestOfTheElimTest, BelpartanIgnoresAnyOtherStep) {
	hold(1002, QuestStatus::START, 14);
	Npc& belpartan = spawnNpc(BELPARTAN);

	EXPECT_FALSE(talk(belpartan, DA::QUEST_SELECT));
	EXPECT_FALSE(player().isInState(gameserver::model::gameobjects::state::CreatureState::FLYING));
	executor().advance(std::chrono::milliseconds(43000));
	EXPECT_EQ(varOf(1002), 14);
}

TEST_F(RequestOfTheElimTest, KalioShowsTheReportPageThenFinishesTheMission) {
	hold(1002, QuestStatus::REWARD, 14);
	Npc& kalio = spawnNpc(KALIO);

	EXPECT_TRUE(talk(kalio, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(dialogWindow(kalio.getObjectId(), 2716, 1002)));
	talk(kalio, DA::SELECTED_QUEST_REWARD1);
	EXPECT_EQ(stateOf(1002)->getStatus(), QuestStatus::COMPLETE) << "sendQuestEndDialog finishes it with the first selectable item";
	EXPECT_EQ(held(100200613), 1) << "quest_data.xml:25";
}

TEST_F(RequestOfTheElimTest, NoStateOrAnotherStatusAnswersFalse) {
	Npc& ampeis = spawnNpc(AMPEIS);
	EXPECT_FALSE(talk(ampeis, DA::QUEST_SELECT)) << "no quest state";
	hold(1002, QuestStatus::LOCKED, 0);
	EXPECT_FALSE(talk(ampeis, DA::QUEST_SELECT));
}

TEST_F(RequestOfTheElimTest, EnteringKaramatisSendsTheAscensionMorphAndElsewhereStep20FallsBackTo13) {
	Ref<QuestState> qs = hold(1002, QuestStatus::START, 20);
	clearSent();
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(1002), 13) << "_1002RequestoftheElim.java:172-176: outside Karamatis, step 20 goes back to 13";
	EXPECT_FALSE(wasSentClass(serializedFor(SM_ASCENSION_MORPH(1))));

	qs->setQuestVarById(0, 12);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(1002), 12) << "any other step stays";

	// in Karamatis: the morph packet, and the step stays
	qs->setQuestVarById(0, 20);
	world::World::getInstance().despawn(player());
	ASSERT_TRUE(world::World::getInstance().setPosition(Ptr<gameserver::model::gameobjects::VisibleObject>(player()), KARAMATIS, 1, 52.0f, 174.0f,
		229.0f, int8_t{0}));
	clearSent();
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_TRUE(wasSent(serializedFor(SM_ASCENSION_MORPH(1))));
	EXPECT_EQ(varOf(1002), 20);
}

TEST_F(RequestOfTheElimTest, Completing1100StartsTheMission) {
	// defaultOnQuestCompletedEvent(env, 1100): the mission starts once 1100 is complete (level 8 >= minlevel 3)
	hold(1100, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onQuestCompleted(player(), 1100);
	ASSERT_TRUE(stateOf(1002));
	EXPECT_EQ(stateOf(1002)->getStatus(), QuestStatus::START);
}

TEST_F(RequestOfTheElimTest, TheLevelUpStartsTheMissionOnlyAfter1100) {
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_FALSE(stateOf(1002)) << "1100 is not complete";
	hold(1100, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onLevelChanged(player());
	ASSERT_TRUE(stateOf(1002));
	EXPECT_EQ(stateOf(1002)->getStatus(), QuestStatus::START);
}

// ---- 1114 The Nymph's Gown ------------------------------------------------------------------------------------------------------------------

class TheNymphsGownTest : public ZoneQuestTest {
protected:
	void SetUp() override {
		ZoneQuestTest::SetUp();
		if (IsSkipped())
			return;
		install(::aion::gameserver::handlers::quest::poeta::_1114TheNymphsGown_questFactory());
		spawnActor(gameserver::model::Race::ELYOS, 8, POETA, 1000.0f, 1000.0f, 100.0f);
	}

	bool talk(Ptr<gameserver::model::gameobjects::VisibleObject> target, int32_t action) {
		clearSent();
		return dialog(target, 1114, action);
	}

	gameserver::model::gameobjects::Item& diary() {
		give(NAMUS_DIARY, 1);
		return *player().getInventory().getFirstItemByItemId(NAMUS_DIARY);
	}

	HandlerResult use(gameserver::model::gameobjects::Item& item) {
		Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 1114, 0);
		envs.push_back(env);
		return handler->onItemUseEvent(*env, item);
	}
};

TEST_F(TheNymphsGownTest, RegisterNamesTheDiaryAndTheThreeNpcs) {
	for (int32_t npcId : {NAMUS, ASTEROS, SEIRENIA_CLOTHES})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().contains(1114)) << npcId;
	EXPECT_TRUE(QuestEngine::getInstance().isRegisteredQuestItem(NAMUS_DIARY));
	EXPECT_FALSE(QuestEngine::getInstance().isRegisteredQuestItem(DIARY_PAGE));
}

TEST_F(TheNymphsGownTest, TheDiaryStartsTheQuestAndAnyOtherItemIsUnknown) {
	give(FLUTE, 1);
	EXPECT_EQ(use(*player().getInventory().getFirstItemByItemId(FLUTE)), HandlerResult::UNKNOWN);
	EXPECT_FALSE(stateOf(1114));
	EXPECT_EQ(use(diary()), HandlerResult::SUCCESS);
	ASSERT_TRUE(stateOf(1114));
	EXPECT_EQ(stateOf(1114)->getStatus(), QuestStatus::START);
	EXPECT_EQ(use(diary()), HandlerResult::SUCCESS) << "a started quest: SUCCESS and nothing else";
	EXPECT_EQ(varOf(1114), 0);
}

TEST_F(TheNymphsGownTest, AcceptingFromTheDiaryStartsTheQuestAndSwapsTheDiaryForThePage) {
	diary();
	EXPECT_FALSE(talk(nullptr, DA::QUEST_REFUSE_1));
	EXPECT_TRUE(wasSent(dialogWindow(0, 0, 0))) << "SM_DIALOG_WINDOW(0, 0) for any other action";
	EXPECT_FALSE(stateOf(1114));

	EXPECT_TRUE(talk(nullptr, DA::QUEST_ACCEPT_1));
	ASSERT_TRUE(stateOf(1114));
	EXPECT_EQ(stateOf(1114)->getStatus(), QuestStatus::START);
	EXPECT_EQ(held(DIARY_PAGE), 1);
	EXPECT_EQ(held(NAMUS_DIARY), 0);
	EXPECT_TRUE(wasSent(dialogWindow(0, 0, 0)));
}

TEST_F(TheNymphsGownTest, NamusTakesThePageThenTheClothesAngerSeireniaAndGiveTheDress) {
	hold(1114, QuestStatus::START, 0);
	give(DIARY_PAGE, 1);
	Npc& namus = spawnNpc(NAMUS);
	Npc& seirenia = spawnNpc(SEIRENIA, 3.0f);
	Npc& other = spawnNpc(210402, 4.0f); // a monster that could hate the player (an Ishalgen mob of 2004, npc_templates.xml row in the data)
	Npc& clothes = spawnNpc(SEIRENIA_CLOTHES, 5.0f);

	EXPECT_TRUE(talk(namus, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 1011, 1114)));
	EXPECT_TRUE(talk(clothes, DA::USE_OBJECT)) << ":113-124: USE_OBJECT answers true at any step";
	EXPECT_EQ(varOf(1114), 0);
	EXPECT_TRUE(talk(namus, DA::SETPRO1));
	EXPECT_EQ(varOf(1114), 1);
	EXPECT_EQ(held(DIARY_PAGE), 0);
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 10, 0)));

	ASSERT_TRUE(player().getKnownList().knows(seirenia));
	EXPECT_TRUE(talk(clothes, DA::USE_OBJECT));
	EXPECT_EQ(varOf(1114), 2);
	EXPECT_EQ(held(NYMPHS_DRESS), 1);
	EXPECT_TRUE(seirenia.getAggroList().isHating(player())) << "addHate(player, 50) on Seirenia (203175)";
	EXPECT_EQ(seirenia.getAggroList().getHate(player()), 50);
	ASSERT_TRUE(player().getKnownList().knows(other));
	EXPECT_FALSE(other.getAggroList().isHating(player())) << "no other npc, the monster beside Seirenia neither";
}

TEST_F(TheNymphsGownTest, NamusRewardsGroup1StraightFromStep2) {
	hold(1114, QuestStatus::START, 2);
	give(NYMPHS_DRESS, 1);
	Npc& namus = spawnNpc(NAMUS);

	EXPECT_TRUE(talk(namus, DA::SELECT_QUEST_REWARD)) << "_1114TheNymphsGown.java:85: var == 2 || var == 3";
	EXPECT_EQ(stateOf(1114)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1114), 4);
	EXPECT_EQ(stateOf(1114)->getRewardGroup(), 1);
	EXPECT_EQ(held(NYMPHS_DRESS), 0);
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 6, 1114)));
}

TEST_F(TheNymphsGownTest, NamusMovesStep2To3AndRewardsGroup1AtStep3) {
	hold(1114, QuestStatus::START, 2);
	give(NYMPHS_DRESS, 1);
	Npc& namus = spawnNpc(NAMUS);

	EXPECT_TRUE(talk(namus, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 1693, 1114)));
	EXPECT_TRUE(talk(namus, DA::SETPRO2));
	EXPECT_EQ(varOf(1114), 3);
	EXPECT_TRUE(talk(namus, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 2375, 1114)));
	EXPECT_TRUE(talk(namus, DA::SELECT_QUEST_REWARD));
	EXPECT_EQ(stateOf(1114)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1114), 4);
	EXPECT_EQ(stateOf(1114)->getRewardGroup(), 1);
	EXPECT_EQ(held(NYMPHS_DRESS), 0);
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 6, 1114)));

	// REWARD at Namus (var 4): the report page, the reward page, then the end dialog finishes group 1 (quest_data.xml:991: gold 960)
	EXPECT_TRUE(talk(namus, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 2375, 1114)));
	EXPECT_TRUE(talk(namus, DA::SELECT_QUEST_REWARD));
	EXPECT_TRUE(wasSent(dialogWindow(namus.getObjectId(), 6, 1114)));
	int64_t kinah = player().getInventory().getKinah();
	talk(namus, DA::SELECTED_QUEST_NOREWARD);
	EXPECT_EQ(stateOf(1114)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getInventory().getKinah() - kinah, 960);
}

TEST_F(TheNymphsGownTest, AsterosRewardsGroup0AtStep3) {
	hold(1114, QuestStatus::START, 3);
	give(NYMPHS_DRESS, 1);
	Npc& asteros = spawnNpc(ASTEROS);

	EXPECT_TRUE(talk(asteros, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(asteros.getObjectId(), 2034, 1114)));
	EXPECT_TRUE(talk(asteros, DA::SETPRO2));
	EXPECT_TRUE(wasSent(dialogWindow(asteros.getObjectId(), 10, 0)));
	EXPECT_EQ(varOf(1114), 3) << ":142-146: Asteros' SETPRO2 only closes";
	EXPECT_TRUE(talk(asteros, DA::SETPRO3));
	EXPECT_EQ(stateOf(1114)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(stateOf(1114)->getRewardGroup(), 0);
	EXPECT_EQ(held(NYMPHS_DRESS), 0);
	EXPECT_TRUE(wasSent(dialogWindow(asteros.getObjectId(), 5, 1114)));

	int64_t kinah = player().getInventory().getKinah();
	talk(asteros, DA::SELECTED_QUEST_NOREWARD);
	EXPECT_EQ(stateOf(1114)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getInventory().getKinah() - kinah, 1920) << "quest_data.xml:990: group 0, gold 1920";
}

TEST_F(TheNymphsGownTest, AsterosAtAnotherStepAndACompleteQuestAnswerFalse) {
	Ref<QuestState> qs = hold(1114, QuestStatus::START, 2);
	Npc& asteros = spawnNpc(ASTEROS);
	EXPECT_FALSE(talk(asteros, DA::QUEST_SELECT));
	EXPECT_FALSE(talk(asteros, DA::SETPRO3));
	qs->setStatus(QuestStatus::COMPLETE);
	EXPECT_FALSE(talk(asteros, DA::QUEST_SELECT));
}

} // namespace aion::gameserver::questEngine::handlers::zones::test
