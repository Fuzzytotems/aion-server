// Q09's tests of its four hand-ported Ishalgen quests (route-hand lane, phase 6, 2026-09-29): _2002WheresRae, _2004ACharmedCube,
// _2007WheresRaeThisTime and _2136TheLostAxe, every hook and step through the real QuestEngine on the fixture's World
// (ZoneQuestTestSupport.h), the timers on its ManualClock. The Ataxiar leg of 2002 (SETPRO5's instance and Rae's flight back from inside) is
// SoloInstanceQuestTest.cpp's. The handlers come from Q09's library, which this executable links, through their registry factories.

#include "ZoneQuestTestSupport.h"

#include <cmath>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::quest::ishalgen {
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _2002WheresRae_questFactory();
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _2004ACharmedCube_questFactory();
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _2007WheresRaeThisTime_questFactory();
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _2136TheLostAxe_questFactory();
} // namespace aion::gameserver::handlers::quest::ishalgen

namespace aion::gameserver::questEngine::handlers::zones::test {

namespace DA = gameserver::model::DialogAction;
namespace ishalgen = ::aion::gameserver::handlers::quest::ishalgen;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_PLAY_MOVIE;
using CreatureState = gameserver::model::gameobjects::state::CreatureState;

class IshalgenQuestTest : public ZoneQuestTest {
protected:
	void setUpQuest(std::unique_ptr<AbstractQuestHandler> created, int32_t id) {
		questId = id;
		install(std::move(created));
		spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	}

	bool talk(Ptr<gameserver::model::gameobjects::VisibleObject> target, int32_t action) {
		clearSent();
		return dialog(target, questId, action);
	}

	bool talk(Npc& npc, int32_t action) { return talk(Ptr<gameserver::model::gameobjects::VisibleObject>(npc), action); }

	/** SM_DIALOG_WINDOW(objectId, page): the constructor without a quest id (quest 0) */
	static std::vector<uint8_t> window(int32_t objectId, int32_t page) { return dialogWindow(objectId, page, 0); }

	int32_t questId = 0;
};

// ---- 2002 Where's Rae? --------------------------------------------------------------------------------------------------------------------

inline constexpr int32_t RAE_203519 = 203519;
inline constexpr int32_t VIDAR_203534 = 203534;
inline constexpr int32_t RAE_790002 = 790002; // handled in onDialogEvent though register() does not name it (the quest id routes the dialog)
inline constexpr int32_t SPIRIT_STONE = 700045;
inline constexpr int32_t RAE_ROCK = 203538;
inline constexpr int32_t RAE_203553 = 203553;
inline constexpr int32_t ATAXIAR_HAGEN = 205020; // Hagen (npc_templates.xml:23709; spawns/Instances/320010000_Ataxiar.xml:63-66)
inline constexpr int32_t ULGORN = 203516;
inline constexpr int32_t ATAXIAR_SPY = 210377;
inline constexpr int32_t ATAXIAR_SPY_2 = 210378;
inline constexpr int32_t RAES_SPIRIT_STONE_ITEM = 182203003; // 2002's collect item (quest_data.xml:9236)

class WheresRaeTest : public IshalgenQuestTest {
protected:
	void SetUp() override {
		ZoneQuestTest::SetUp();
		if (!IsSkipped())
			setUpQuest(ishalgen::_2002WheresRae_questFactory(), 2002);
	}
};

TEST_F(WheresRaeTest, RegisterNamesTheTalkNpcsAndTheTwoKills) {
	for (int32_t npcId : {RAE_203519, VIDAR_203534, RAE_203553, SPIRIT_STONE, ULGORN, RAE_ROCK})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().contains(2002)) << npcId;
	for (int32_t npcId : {ATAXIAR_SPY, ATAXIAR_SPY_2})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnKillEvent().contains(2002)) << npcId;
	EXPECT_FALSE(QuestEngine::getInstance().getQuestNpc(RAE_790002)->getOnTalkEvent().contains(2002)) << "_2002WheresRae.java:33";
}

TEST_F(WheresRaeTest, TheFirstTwoNpcsSetSteps1And2AndVidarPlaysMovie52) {
	hold(2002, QuestStatus::START, 0);
	Npc& rae = spawnNpc(RAE_203519);
	Npc& vidar = spawnNpc(VIDAR_203534, 4.0f);

	EXPECT_TRUE(talk(rae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 1011, 2002)));
	EXPECT_TRUE(talk(rae, DA::SETPRO1));
	EXPECT_EQ(varOf(2002), 1);
	EXPECT_TRUE(wasSent(window(rae.getObjectId(), 10)));
	EXPECT_FALSE(talk(rae, DA::SETPRO1)) << "var 1: :63-71 falls out to return false";

	EXPECT_TRUE(talk(vidar, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(vidar.getObjectId(), 1352, 2002)));
	EXPECT_FALSE(talk(vidar, DA::SELECT2_1)) << ":78-80, :89: the movie, then break and return false";
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, vidar.getObjectId(), 2002, 52, true))));
	EXPECT_TRUE(talk(vidar, DA::SETPRO2));
	EXPECT_EQ(varOf(2002), 2);
}

TEST_F(WheresRaeTest, RaeSendsThePlayerHuntingAndTheKillsCountSteps3To10) {
	hold(2002, QuestStatus::START, 2);
	Npc& rae = spawnNpc(RAE_790002);
	Npc& spy = spawnNpc(ATAXIAR_SPY, 6.0f);
	Npc& spy2 = spawnNpc(ATAXIAR_SPY_2, 8.0f);

	EXPECT_TRUE(talk(rae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 1693, 2002)));
	EXPECT_TRUE(talk(rae, DA::SETPRO3));
	EXPECT_EQ(varOf(2002), 3);

	for (int32_t i = 0; i < 7; i++)
		kill(i % 2 == 0 ? spy : spy2);
	EXPECT_EQ(varOf(2002), 10);
	EXPECT_TRUE(wasSent(questUpdate(2002, START, 10)));
	kill(spy);
	EXPECT_EQ(varOf(2002), 10) << "var 10 is past the hunt (var >= 3 && var < 10)";
	EXPECT_FALSE(killHook(spy));

	EXPECT_TRUE(talk(rae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 2034, 2002)));
	EXPECT_TRUE(talk(rae, DA::SETPRO4)) << "SETPRO3, SETPRO4 and SETPRO6 share one arm";
	EXPECT_EQ(varOf(2002), 11);
}

TEST_F(WheresRaeTest, TheKillsCountOnlyInStartAndBetweenSteps3And9) {
	Ref<QuestState> qs = hold(2002, QuestStatus::START, 2);
	Npc& spy = spawnNpc(ATAXIAR_SPY);
	Npc& other = spawnNpc(RAE_203519, 4.0f);
	EXPECT_FALSE(killHook(spy));
	EXPECT_EQ(varOf(2002), 2);
	qs->setQuestVarById(0, 9);
	EXPECT_FALSE(killHook(other)) << "another npc";
	EXPECT_EQ(varOf(2002), 9);
	EXPECT_TRUE(killHook(spy));
	EXPECT_EQ(varOf(2002), 10);
	qs->setQuestVarById(0, 5);
	qs->setStatus(QuestStatus::REWARD);
	EXPECT_FALSE(killHook(spy));
	EXPECT_EQ(varOf(2002), 5);
}

TEST_F(WheresRaeTest, TheSpiritStoneTurnsThePlayerIntoAFrogAndTheItemIsCheckedAtStep11) {
	hold(2002, QuestStatus::START, 11);
	Npc& rae = spawnNpc(RAE_790002);
	Npc& stone = spawnNpc(SPIRIT_STONE, 4.0f);

	EXPECT_TRUE(talk(rae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 2375, 2002)));
	EXPECT_TRUE(talk(stone, DA::USE_OBJECT)) << ":141-146: applyEffectDirectly(8343) and true";
	EXPECT_TRUE(player().getEffectController()->hasAbnormalEffect(8343)) << "Ribbit Transformation (skill_templates.xml:81678)";
	EXPECT_EQ(player().getTransformModel().getModelId(), 210273) << "its shapechange model";
	EXPECT_FALSE(talk(stone, DA::QUEST_SELECT));

	EXPECT_TRUE(talk(rae, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 2376, 2002))) << "no item: the fail page";
	EXPECT_EQ(varOf(2002), 11);
	give(RAES_SPIRIT_STONE_ITEM, 1);
	EXPECT_TRUE(talk(rae, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 2461, 2002)));
	EXPECT_EQ(varOf(2002), 12);
	EXPECT_EQ(held(RAES_SPIRIT_STONE_ITEM), 0) << "collectItemCheck(env, true) removes it";
	EXPECT_TRUE(talk(rae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 2462, 2002)));
}

TEST_F(WheresRaeTest, TheAtaxiarHagenFliesThePlayerBackToIshalgenAfter40SecondsFromStep12On) {
	Ref<QuestState> qs = hold(2002, QuestStatus::START, 11);
	Npc& hagen = spawnNpc(ATAXIAR_HAGEN);
	EXPECT_FALSE(talk(hagen, DA::QUEST_SELECT)) << "var 11 < 12";
	EXPECT_FALSE(talk(hagen, DA::SETPRO1)) << "default: false";
	qs->setQuestVar(99); // SETPRO5's step, set before the instance is entered: slot 0 reads 35 (99 & 0x3F), which is >= 12
	EXPECT_EQ(varOf(2002), 35);

	EXPECT_TRUE(talk(hagen, DA::QUEST_SELECT));
	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_FALSE(player().isInState(CreatureState::ACTIVE));
	EXPECT_TRUE(wasSent(serializedFor(SM_EMOTION(player(), gameserver::model::EmotionType::START_FLYTELEPORT, 3001, 0))));
	ASSERT_TRUE(player().getFlightPath()) << "player.setFlightTeleportId(3001): Player.java:809-810";
	EXPECT_EQ(player().getFlightPath()->getType(), gameserver::model::templates::flypath::FlightPath_Type::FLIGHT_TRANSPORTER);
	EXPECT_EQ(player().getFlightPath()->getId(), 3001);
	executor().advance(std::chrono::milliseconds(39999));
	EXPECT_EQ(rawVarsOf(2002), 99);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(varOf(2002), 13) << "qs.setQuestVar(13) after the teleport";
	EXPECT_TRUE(wasSent(questUpdate(2002, START, 13)));
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_FLOAT_EQ(player().getX(), 940.15f);
	EXPECT_FLOAT_EQ(player().getY(), 2295.64f);
	EXPECT_FLOAT_EQ(player().getZ(), 265.7f);
	EXPECT_EQ(player().getHeading(), 43);
}

TEST_F(WheresRaeTest, RaeAndTheRockLeadToTheSpawnedRaeWhoSetsTheReward) {
	hold(2002, QuestStatus::START, 13);
	Npc& rae = spawnNpc(RAE_790002);
	Npc& rock = spawnNpc(RAE_ROCK, 4.0f);

	EXPECT_TRUE(talk(rae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 2716, 2002)));
	EXPECT_TRUE(talk(rae, DA::SETPRO6));
	EXPECT_EQ(varOf(2002), 14);
	EXPECT_FALSE(talk(rock, DA::QUEST_SELECT));

	EXPECT_TRUE(talk(rock, DA::USE_OBJECT));
	EXPECT_EQ(varOf(2002), 15);
	EXPECT_TRUE(wasSent(window(rock.getObjectId(), 10)));
	EXPECT_FALSE(rock.isSpawned()) << "deleteAndScheduleRespawn";
	Ptr<Npc> spawnedRae = player().getWorldMapInstance()->getNpc(RAE_203553);
	ASSERT_TRUE(spawnedRae) << "spawnForFiveMinutes(203553, npc.getPosition())";
	EXPECT_FLOAT_EQ(spawnedRae->getX(), rock.getX());
	EXPECT_FLOAT_EQ(spawnedRae->getY(), rock.getY());
	spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(spawnedRae));

	EXPECT_TRUE(talk(*spawnedRae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(spawnedRae->getObjectId(), 3057, 2002)));
	EXPECT_TRUE(talk(*spawnedRae, DA::SETPRO7));
	EXPECT_EQ(stateOf(2002)->getStatus(), QuestStatus::REWARD);
	EXPECT_FALSE(spawnedRae->isSpawned()) << "getController().delete()";
	EXPECT_TRUE(wasSent(window(spawnedRae->getObjectId(), 10)));
}

TEST_F(WheresRaeTest, TheSpawnedRaeGoesAfterFiveMinutes) {
	hold(2002, QuestStatus::START, 14);
	Npc& rock = spawnNpc(RAE_ROCK);
	EXPECT_TRUE(talk(rock, DA::USE_OBJECT));
	Ptr<Npc> spawnedRae = player().getWorldMapInstance()->getNpc(RAE_203553);
	ASSERT_TRUE(spawnedRae);
	spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(spawnedRae));
	executor().advance(std::chrono::milliseconds(299999));
	EXPECT_TRUE(spawnedRae->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(spawnedRae->isSpawned());
}

TEST_F(WheresRaeTest, UlgornReportsAndFinishesTheMission) {
	hold(2002, QuestStatus::REWARD, 15);
	Npc& ulgorn = spawnNpc(ULGORN);

	EXPECT_TRUE(talk(ulgorn, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(dialogWindow(ulgorn.getObjectId(), 3398, 2002)));
	EXPECT_TRUE(talk(ulgorn, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(ulgorn.getObjectId(), 3398, 2002)));
	EXPECT_TRUE(talk(ulgorn, DA::SETPRO8));
	EXPECT_TRUE(wasSent(dialogWindow(ulgorn.getObjectId(), 5, 2002)));
	talk(ulgorn, DA::SELECTED_QUEST_REWARD1);
	EXPECT_EQ(stateOf(2002)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(100200604), 1) << "quest_data.xml:9239, the first selectable item";
}

TEST_F(WheresRaeTest, TheLevelUpBefore2100DoesNothingAndCompleting2100StartsTheMission) {
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_FALSE(stateOf(2002));
	hold(2100, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onQuestCompleted(player(), 2100);
	ASSERT_TRUE(stateOf(2002));
	EXPECT_EQ(stateOf(2002)->getStatus(), QuestStatus::START);
}

TEST_F(WheresRaeTest, TheLevelUpStartsTheMissionAfter2100) {
	hold(2100, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onLevelChanged(player());
	ASSERT_TRUE(stateOf(2002));
	EXPECT_EQ(stateOf(2002)->getStatus(), QuestStatus::START);
}

// ---- 2004 A Charmed Cube ------------------------------------------------------------------------------------------------------------------

inline constexpr int32_t DEROT = 203539;
inline constexpr int32_t TOMBSTONE = 700047;
inline constexpr int32_t MUNIN = 203550;
inline constexpr int32_t CUBE_MOB = 210402;
inline constexpr int32_t CUBE_MOB_2 = 210403;
inline constexpr int32_t CHARMED_CUBE_GUARD = 211755;
inline constexpr int32_t CHARMED_CUBE = 182203005;

class ACharmedCubeTest : public IshalgenQuestTest {
protected:
	void SetUp() override {
		ZoneQuestTest::SetUp();
		if (!IsSkipped())
			setUpQuest(ishalgen::_2004ACharmedCube_questFactory(), 2004);
	}
};

TEST_F(ACharmedCubeTest, RegisterNamesTheThreeTalkNpcsAndTheTwoMobs) {
	for (int32_t npcId : {DEROT, TOMBSTONE, MUNIN})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().contains(2004)) << npcId;
	for (int32_t npcId : {CUBE_MOB, CUBE_MOB_2})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnKillEvent().contains(2004)) << npcId;
}

TEST_F(ACharmedCubeTest, DerotSendsThePlayerToTheTombstoneAndChecksTheCube) {
	hold(2004, QuestStatus::START, 0);
	Npc& derot = spawnNpc(DEROT);

	EXPECT_TRUE(talk(derot, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(derot.getObjectId(), 1011, 2004)));
	EXPECT_TRUE(talk(derot, DA::SETPRO1));
	EXPECT_EQ(varOf(2004), 1);
	EXPECT_TRUE(talk(derot, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(derot.getObjectId(), 1352, 2004)));
	EXPECT_TRUE(talk(derot, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(wasSent(dialogWindow(derot.getObjectId(), 1353, 2004))) << "no cube: checkQuestItems' fail page";
	EXPECT_EQ(varOf(2004), 1);
	EXPECT_TRUE(talk(derot, DA::SETPRO2));
	EXPECT_EQ(held(CHARMED_CUBE), 1) << "giveQuestItem(env, 182203005, 1)";
	EXPECT_TRUE(wasSent(window(derot.getObjectId(), 10))) << "sendQuestSelectionDialog";
	EXPECT_TRUE(talk(derot, DA::CHECK_USER_HAS_QUEST_ITEM));
	EXPECT_TRUE(wasSent(dialogWindow(derot.getObjectId(), 1438, 2004)));
	EXPECT_EQ(varOf(2004), 2);
	EXPECT_TRUE(talk(derot, DA::FINISH_DIALOG));
	EXPECT_TRUE(wasSent(window(derot.getObjectId(), 10)));
}

TEST_F(ACharmedCubeTest, TheTombstoneSpawnsTheGuardInFrontOfItForFiveMinutesAtStep1) {
	Ref<QuestState> qs = hold(2004, QuestStatus::START, 0);
	Npc& tombstone = spawnNpc(TOMBSTONE, 2.0f, int8_t{103}); // a shipped tombstone's heading (spawns/Npcs/220010000_Ishalgen.xml:1643)
	EXPECT_FALSE(talk(tombstone, DA::USE_OBJECT)) << "var 0";
	EXPECT_FALSE(player().getWorldMapInstance()->getNpc(CHARMED_CUBE_GUARD));
	qs->setQuestVarById(0, 1);
	EXPECT_FALSE(talk(tombstone, DA::QUEST_SELECT));

	EXPECT_TRUE(talk(tombstone, DA::USE_OBJECT));
	Ptr<Npc> guard = player().getWorldMapInstance()->getNpc(CHARMED_CUBE_GUARD);
	ASSERT_TRUE(guard) << "spawnForFiveMinutesInFront(211755, tombstone, heading, 2)";
	spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(guard));
	double radian = utils::PositionUtil::convertHeadingToAngle(tombstone.getHeading()) * 0.017453292519943295;
	EXPECT_NEAR(guard->getX(), tombstone.getX() + std::cos(radian) * 2, 0.01) << "2 m in front of the tombstone";
	EXPECT_NEAR(guard->getY(), tombstone.getY() + std::sin(radian) * 2, 0.01);
	EXPECT_EQ(guard->getHeading(), 103) << "env.getVisibleObject().getHeading(): the guard takes the tombstone's heading";
	EXPECT_EQ(varOf(2004), 1);
	executor().advance(std::chrono::milliseconds(299999));
	EXPECT_TRUE(guard->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(guard->isSpawned()) << "spawnForFiveMinutesInFront";
}

TEST_F(ACharmedCubeTest, MuninTakesTheCubeTheKillsCountAndMuninSetsTheReward) {
	hold(2004, QuestStatus::START, 2);
	give(CHARMED_CUBE, 1);
	Npc& munin = spawnNpc(MUNIN);
	Npc& mob = spawnNpc(CUBE_MOB, 6.0f);
	Npc& mob2 = spawnNpc(CUBE_MOB_2, 8.0f);

	EXPECT_TRUE(talk(munin, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 1693, 2004)));
	EXPECT_TRUE(talk(munin, DA::SETPRO3));
	EXPECT_EQ(varOf(2004), 3);
	EXPECT_EQ(held(CHARMED_CUBE), 0);

	EXPECT_TRUE(killHook(mob));
	EXPECT_EQ(varOf(2004), 4);
	kill(mob2);
	EXPECT_EQ(varOf(2004), 5);
	kill(mob);
	EXPECT_EQ(varOf(2004), 6) << "defaultOnKillEvent(env, mobs, 3, 6)";
	EXPECT_FALSE(killHook(mob));
	EXPECT_EQ(varOf(2004), 6);

	EXPECT_TRUE(talk(munin, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 2034, 2004)));
	EXPECT_TRUE(talk(munin, DA::SETPRO4));
	EXPECT_EQ(stateOf(2004)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(2004), 6) << "defaultCloseDialog(env, 6, 6, true, false)";
	EXPECT_TRUE(wasSent(questUpdate(2004, REWARD, 6)));
	EXPECT_TRUE(wasSent(window(munin.getObjectId(), 0))) << "closeDialogWindow";
}

TEST_F(ACharmedCubeTest, DerotReportsAndFinishesTheMission) {
	hold(2004, QuestStatus::REWARD, 6);
	Npc& derot = spawnNpc(DEROT);
	Npc& munin = spawnNpc(MUNIN, 4.0f);
	EXPECT_FALSE(talk(munin, DA::USE_OBJECT)) << "only Derot in REWARD";

	EXPECT_TRUE(talk(derot, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(dialogWindow(derot.getObjectId(), 2375, 2004)));
	talk(derot, DA::SELECTED_QUEST_NOREWARD);
	EXPECT_EQ(stateOf(2004)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(122000869), 1) << "quest_data.xml:9264";
}

TEST_F(ACharmedCubeTest, TheMissionStartsAfter2100) {
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_FALSE(stateOf(2004));
	hold(2100, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onQuestCompleted(player(), 2100);
	ASSERT_TRUE(stateOf(2004));
	EXPECT_EQ(stateOf(2004)->getStatus(), QuestStatus::START);
}

// ---- 2007 Where's Rae This Time? ---------------------------------------------------------------------------------------------------------

inline constexpr int32_t NPC_203552 = 203552;
inline constexpr int32_t NPC_203554 = 203554;
inline constexpr int32_t ALTAR_1 = 700085;
inline constexpr int32_t ALTAR_2 = 700086;
inline constexpr int32_t ALTAR_3 = 700087;

class WheresRaeThisTimeTest : public IshalgenQuestTest {
protected:
	void SetUp() override {
		ZoneQuestTest::SetUp();
		if (!IsSkipped())
			setUpQuest(ishalgen::_2007WheresRaeThisTime_questFactory(), 2007);
	}
};

TEST_F(WheresRaeThisTimeTest, RegisterNamesTheEightTalkNpcs) {
	for (int32_t npcId : {ULGORN, RAE_203519, DEROT, NPC_203552, NPC_203554, ALTAR_1, ALTAR_2, ALTAR_3})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().contains(2007)) << npcId;
}

TEST_F(WheresRaeThisTimeTest, FourNpcsSetSteps1To4AndDerotPlaysMovie55) {
	hold(2007, QuestStatus::START, 0);
	Npc& ulgorn = spawnNpc(ULGORN);
	Npc& rae = spawnNpc(RAE_203519, 3.0f);
	Npc& derot = spawnNpc(DEROT, 4.0f);
	Npc& npc4 = spawnNpc(NPC_203552, 5.0f);

	EXPECT_TRUE(talk(ulgorn, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(ulgorn.getObjectId(), 1011, 2007)));
	EXPECT_TRUE(talk(ulgorn, DA::SETPRO1));
	EXPECT_EQ(varOf(2007), 1);
	EXPECT_TRUE(wasSent(window(ulgorn.getObjectId(), 0))) << "closeDialogWindow";
	EXPECT_FALSE(talk(ulgorn, DA::QUEST_SELECT));

	EXPECT_TRUE(talk(rae, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 1352, 2007)));
	EXPECT_TRUE(talk(rae, DA::SETPRO2));
	EXPECT_EQ(varOf(2007), 2);

	EXPECT_TRUE(talk(derot, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(derot.getObjectId(), 1693, 2007)));
	EXPECT_FALSE(talk(derot, DA::SELECT3_1));
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, derot.getObjectId(), 2007, 55, true))));
	EXPECT_TRUE(talk(derot, DA::SETPRO3));
	EXPECT_EQ(varOf(2007), 3);

	EXPECT_TRUE(talk(npc4, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(npc4.getObjectId(), 2034, 2007)));
	EXPECT_TRUE(talk(npc4, DA::SETPRO4));
	EXPECT_EQ(varOf(2007), 4);
}

TEST_F(WheresRaeThisTimeTest, TheThreeAltarsGoInOrderAndTheLastPlaysMovie56) {
	hold(2007, QuestStatus::START, 4);
	Npc& npc5 = spawnNpc(NPC_203554);
	Npc& altar1 = spawnNpc(ALTAR_1, 3.0f);
	Npc& altar2 = spawnNpc(ALTAR_2, 4.0f);
	Npc& altar3 = spawnNpc(ALTAR_3, 5.0f);

	EXPECT_TRUE(talk(npc5, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(npc5.getObjectId(), 2375, 2007)));
	EXPECT_TRUE(talk(npc5, DA::SETPRO5));
	EXPECT_EQ(varOf(2007), 5);

	EXPECT_FALSE(talk(altar2, DA::USE_OBJECT));
	EXPECT_EQ(varOf(2007), 5) << "the second altar before the first does nothing";
	EXPECT_FALSE(talk(altar1, DA::USE_OBJECT)) << "destroy(6) and return false";
	EXPECT_EQ(varOf(2007), 6);
	EXPECT_TRUE(wasSent(questUpdate(2007, START, 6)));
	EXPECT_FALSE(talk(altar2, DA::USE_OBJECT));
	EXPECT_EQ(varOf(2007), 7);
	EXPECT_FALSE(talk(altar3, DA::USE_OBJECT));
	EXPECT_EQ(varOf(2007), 8);
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, altar3.getObjectId(), 2007, 56, true))));
}

TEST_F(WheresRaeThisTimeTest, TheLastStepSetsTheRewardAndTeleportsThePlayerInFrontOfUlgorn) {
	hold(2007, QuestStatus::START, 8);
	Npc& npc5 = spawnNpc(NPC_203554);

	EXPECT_TRUE(talk(npc5, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(npc5.getObjectId(), 2716, 2007)));
	EXPECT_TRUE(talk(npc5, DA::SETPRO6));
	EXPECT_EQ(stateOf(2007)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(2007), 8) << "setQuestVar(9), then setQuestVar(8) before the REWARD";
	EXPECT_TRUE(wasSent(questUpdate(2007, START, 9)));
	EXPECT_TRUE(wasSent(questUpdate(2007, REWARD, 8)));

	// TeleportService.teleportToNpc(player, 203516): Ulgorn's spot (spawns/Npcs/220010000_Ishalgen.xml:1676: 589.35, 2450.09, 278.375, h 23),
	// 1 m plus his bound radius (npc_templates.xml: front 0.25... read from the holder) in front of him, facing him (h 23 + 60)
	float radius = dataholders::DataManager::NPC_DATA->getNpcTemplate(ULGORN)->getBoundRadius()->getFront();
	double radian = utils::PositionUtil::convertHeadingToAngle(int8_t{23}) * 0.017453292519943295;
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_FLOAT_EQ(player().getX(), 589.35f + static_cast<float>(std::cos(radian)) * (1.0f + radius));
	EXPECT_FLOAT_EQ(player().getY(), 2450.09f + static_cast<float>(std::sin(radian)) * (1.0f + radius));
	EXPECT_FLOAT_EQ(player().getZ(), 278.375f + 0.5f) << "geo data off: GeoService answers NaN, spot z + 0.5";
	EXPECT_EQ(player().getHeading(), 83);
}

TEST_F(WheresRaeThisTimeTest, UlgornPlaysMovie58AndFinishesTheMission) {
	hold(2007, QuestStatus::REWARD, 8);
	Npc& ulgorn = spawnNpc(ULGORN);

	EXPECT_TRUE(talk(ulgorn, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, ulgorn.getObjectId(), 2007, 58, true))));
	EXPECT_TRUE(wasSent(dialogWindow(ulgorn.getObjectId(), 3057, 2007)));
	talk(ulgorn, DA::SELECTED_QUEST_REWARD1);
	EXPECT_EQ(stateOf(2007)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(110101250), 1) << "quest_data.xml:9294";
}

TEST_F(WheresRaeThisTimeTest, TheMissionStartsOnlyWithAllSixEarlierMissionsAndLocksWithSomeOfThem) {
	for (int32_t id : {2006, 2005, 2004, 2003, 2002})
		hold(id, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onQuestCompleted(player(), 2006);
	ASSERT_TRUE(stateOf(2007)) << "2001 is missing, 2006 was just finished: LOCKED";
	EXPECT_EQ(stateOf(2007)->getStatus(), QuestStatus::LOCKED);

	hold(2001, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onQuestCompleted(player(), 2001);
	EXPECT_EQ(stateOf(2007)->getStatus(), QuestStatus::START);
}

TEST_F(WheresRaeThisTimeTest, TheLevelUpNeeds2100Too) {
	for (int32_t id : {2006, 2005, 2004, 2003, 2002, 2001})
		hold(id, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onLevelChanged(player());
	ASSERT_TRUE(stateOf(2007)) << "2100 missing, the others complete: LOCKED";
	EXPECT_EQ(stateOf(2007)->getStatus(), QuestStatus::LOCKED);
	hold(2100, QuestStatus::COMPLETE, 0);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(stateOf(2007)->getStatus(), QuestStatus::START);
}

// ---- TeleportService.teleportToNpc, the engine body the lane ported for 2007 (TeleportService.java:304-333) ---------------------------------

class TeleportToNpcTest : public ZoneQuestTest {};

TEST_F(TeleportToNpcTest, WithoutASpawnOfTheNpcItWarnsAndLeavesThePlayerWhereHeIs) {
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	services::teleport::TeleportService::teleportToNpc(player(), 999999);
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_FLOAT_EQ(player().getX(), 1000.0f);
	EXPECT_FLOAT_EQ(player().getY(), 1000.0f);
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(TeleportToNpcTest, FromAnotherMapItPutsThePlayerIntoTheMainInstanceOfTheNpcsOpenMap) {
	spawnActor(gameserver::model::Race::ASMODIANS, 8, POETA, 1000.0f, 1000.0f, 100.0f);
	services::teleport::TeleportService::teleportToNpc(player(), ULGORN); // no spawn on Poeta: SpawnsData finds Ishalgen's
	float radius = dataholders::DataManager::NPC_DATA->getNpcTemplate(ULGORN)->getBoundRadius()->getFront();
	double radian = utils::PositionUtil::convertHeadingToAngle(int8_t{23}) * 0.017453292519943295;
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_EQ(player().getInstanceId(), 1) << "World.getWorldMap(220010000).getMainWorldMapInstance()";
	EXPECT_FLOAT_EQ(player().getX(), 589.35f + static_cast<float>(std::cos(radian)) * (1.0f + radius));
	EXPECT_FLOAT_EQ(player().getY(), 2450.09f + static_cast<float>(std::sin(radian)) * (1.0f + radius));
	EXPECT_EQ(player().getHeading(), 83);
}

TEST_F(TeleportToNpcTest, OnThePlayersMapItTurnsAHeadingOf60Back) {
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	// Rae (203554) of 2007: spawns/Npcs/220010000_Ishalgen.xml:1335, 648.419, 920.773, 310.807, h 60
	services::teleport::TeleportService::teleportToNpc(player(), NPC_203554);
	float radius = dataholders::DataManager::NPC_DATA->getNpcTemplate(NPC_203554)->getBoundRadius()->getFront();
	double radian = utils::PositionUtil::convertHeadingToAngle(int8_t{60}) * 0.017453292519943295;
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_EQ(player().getInstanceId(), 1);
	EXPECT_FLOAT_EQ(player().getX(), 648.419f + static_cast<float>(std::cos(radian)) * (1.0f + radius));
	EXPECT_FLOAT_EQ(player().getY(), 920.773f + static_cast<float>(std::sin(radian)) * (1.0f + radius));
	EXPECT_FLOAT_EQ(player().getZ(), 310.807f + 0.5f);
	EXPECT_EQ(player().getHeading(), 0) << "(h & 0xFF) >= 60 ? h - 60 : h + 60, with h 60";
}

TEST_F(TeleportToNpcTest, AStaticObjectWithoutAnNpcTemplateCountsARadiusOf1) {
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	// the Loom of Pandaemonium (spawns/Statics/120010000_Pandaemonium.xml:6: 1185.222, 1468.409, 214.556, no h): an item template, no npc
	constexpr int32_t LOOM = 150000015;
	ASSERT_EQ(dataholders::DataManager::NPC_DATA->getNpcTemplate(LOOM), nullptr);
	services::teleport::TeleportService::teleportToNpc(player(), LOOM);
	EXPECT_EQ(player().getWorldId(), 120010000);
	EXPECT_EQ(player().getInstanceId(), 1) << "an open map: its main instance";
	EXPECT_FLOAT_EQ(player().getX(), 1185.222f + 2.0f) << "heading 0, 1 m plus the radius 1 of TeleportService.java:314";
	EXPECT_FLOAT_EQ(player().getY(), 1468.409f);
	EXPECT_EQ(player().getHeading(), 60);
}

// ---- 2136 The Lost Axe --------------------------------------------------------------------------------------------------------------------

inline constexpr int32_t AXE_OBJECT = 700146;
inline constexpr int32_t AXE_OWNER = 790009;
inline constexpr int32_t LOST_AXE_ITEM = 182203130;

class TheLostAxeTest : public IshalgenQuestTest {
protected:
	void SetUp() override {
		ZoneQuestTest::SetUp();
		if (!IsSkipped())
			setUpQuest(ishalgen::_2136TheLostAxe_questFactory(), 2136);
	}

	HandlerResult use(int32_t itemId) {
		give(itemId, 1);
		Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 2136, 0);
		envs.push_back(env);
		return handler->onItemUseEvent(*env, *player().getInventory().getFirstItemByItemId(itemId));
	}
};

TEST_F(TheLostAxeTest, RegisterNamesTheAxeItemAndTheTwoNpcs) {
	for (int32_t npcId : {AXE_OBJECT, AXE_OWNER})
		EXPECT_TRUE(QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().contains(2136)) << npcId;
	EXPECT_TRUE(QuestEngine::getInstance().isRegisteredQuestItem(LOST_AXE_ITEM));
}

TEST_F(TheLostAxeTest, TheAxeStartsTheQuestAndAnyOtherItemIsUnknown) {
	EXPECT_EQ(use(CHARMED_CUBE), HandlerResult::UNKNOWN);
	EXPECT_FALSE(stateOf(2136));
	EXPECT_EQ(use(LOST_AXE_ITEM), HandlerResult::SUCCESS);
	ASSERT_TRUE(stateOf(2136));
	EXPECT_EQ(stateOf(2136)->getStatus(), QuestStatus::START);
}

TEST_F(TheLostAxeTest, AcceptingStartsTheQuestAndAnythingElseClosesTheWindow) {
	EXPECT_FALSE(talk(nullptr, DA::QUEST_REFUSE_1));
	EXPECT_TRUE(wasSent(window(0, 0)));
	EXPECT_FALSE(stateOf(2136));
	EXPECT_TRUE(talk(nullptr, DA::QUEST_ACCEPT_1));
	ASSERT_TRUE(stateOf(2136));
	EXPECT_EQ(stateOf(2136)->getStatus(), QuestStatus::START);
	EXPECT_TRUE(wasSent(window(0, 0)));
}

TEST_F(TheLostAxeTest, TheAxeObjectPlaysMovie59AndSpawnsItsOwnerForFiveMinutes) {
	Ref<QuestState> qs = hold(2136, QuestStatus::START, 0);
	Npc& axe = spawnNpc(AXE_OBJECT);
	EXPECT_FALSE(talk(axe, DA::QUEST_SELECT));

	EXPECT_TRUE(talk(axe, DA::USE_OBJECT));
	EXPECT_EQ(varOf(2136), 1);
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, axe.getObjectId(), 2136, 59, true))));
	Ptr<Npc> owner = player().getWorldMapInstance()->getNpc(AXE_OWNER);
	ASSERT_TRUE(owner);
	spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(owner));
	EXPECT_FLOAT_EQ(owner->getX(), 1088.5f);
	EXPECT_FLOAT_EQ(owner->getY(), 2371.8f);
	EXPECT_FLOAT_EQ(owner->getZ(), 258.375f);
	EXPECT_EQ(owner->getHeading(), 87);
	EXPECT_FALSE(talk(axe, DA::USE_OBJECT)) << "var 1";
	executor().advance(std::chrono::milliseconds(300000));
	EXPECT_FALSE(owner->isSpawned()) << "spawnForFiveMinutes";
}

TEST_F(TheLostAxeTest, TheOwnerTakesTheAxeForGroup1OrLetsThePlayerKeepItForGroup0) {
	Ref<QuestState> qs = hold(2136, QuestStatus::START, 1);
	give(LOST_AXE_ITEM, 1);
	Npc& owner = spawnNpc(AXE_OWNER);
	EXPECT_TRUE(talk(owner, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(owner.getObjectId(), 1011, 2136)));
	EXPECT_TRUE(talk(owner, DA::SETPRO1));
	EXPECT_EQ(stateOf(2136)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(stateOf(2136)->getRewardGroup(), 1);
	EXPECT_EQ(held(LOST_AXE_ITEM), 0);
	EXPECT_TRUE(wasSent(dialogWindow(owner.getObjectId(), 6, 2136)));

	qs->setStatus(QuestStatus::START);
	give(LOST_AXE_ITEM, 1);
	EXPECT_TRUE(talk(owner, DA::SETPRO2));
	EXPECT_EQ(stateOf(2136)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(stateOf(2136)->getRewardGroup(), 0);
	EXPECT_EQ(held(LOST_AXE_ITEM), 0);
	EXPECT_TRUE(wasSent(dialogWindow(owner.getObjectId(), 5, 2136)));
}

TEST_F(TheLostAxeTest, InRewardTheOwnerFinishesTheQuestAndLeavesTenSecondsAfterEachTalk) {
	Ref<QuestState> qs = hold(2136, QuestStatus::REWARD, 1);
	qs->setRewardGroup(1);
	Npc& owner = spawnNpc(AXE_OWNER);
	talk(owner, DA::SELECTED_QUEST_NOREWARD);
	EXPECT_EQ(stateOf(2136)->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(held(182000558), 1) << "quest_data.xml:10354, group 1";
	EXPECT_EQ(held(100100494), 0);
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_TRUE(owner.isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(owner.isSpawned()) << "the Runnable of _2136TheLostAxe.java:61, 10000 ms";
}

} // namespace aion::gameserver::questEngine::handlers::zones::test
