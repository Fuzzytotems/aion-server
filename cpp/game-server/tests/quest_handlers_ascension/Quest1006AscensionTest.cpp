// _1006Ascension (lane P6-Q asc-hand, chunk Q06; Java: quest/ascension/_1006Ascension.java) on the real engine and the route's data rows
// (AscensionQuestTestSupport.h): the registration and CustomConfig.ENABLE_SIMPLE_2NDCLASS (:48-64), the start at level 9 and the lock at 7-8
// (defaultOnLevelChangedEvent, quest_data.xml:65), every dialog step of Pernos, Daminu and Belpartan with its var (:67-188), the item use
// outside its zone (:190-203), the Karamatis B instance (:98-104), the flight and the raiders 43 s later (:150-176), the kill counting and
// Orissan (:205-231), the class selection and setClass (:106-129, :233-239), the reward teleport, the finish with its exp and the Daeva
// status (:179-188, :280-288), the death and the enter world in and outside the instance (:241-265), and 1007 starting after 1006 completes.

#include "AscensionQuestTestSupport.h"

#include <gtest/gtest.h>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::handlers::quest::ascension::test {
namespace {

namespace DA = ::aion::gameserver::model::DialogAction;
using ::aion::gameserver::model::gameobjects::state::CreatureState;

constexpr int32_t QUEST = 1006;
// the Poeta spot of the quester: Pernos's own spawn (spawns/Npcs/210010000_Poeta.xml has him near the Akarios village)
constexpr float POETA_X = 245.0f, POETA_Y = 1639.0f, POETA_Z = 100.0f;
// the raiders' arena in Karamatis B: Orissan's spot (:218)
constexpr float ARENA_X = 226.7f, ARENA_Y = 251.5f, ARENA_Z = 205.5f;

class Quest1006Test : public AscensionQuestTest {
protected:
	/** An Elyos Warrior of level 9 in Poeta with 1006 held at `var` (START) and the four handlers registered */
	void startAt(int32_t var) {
		registerHandlers();
		spawnQuester(Race::ELYOS, 9, POETA, 1, POETA_X, POETA_Y, POETA_Z);
		hold(QUEST, QuestStatus::START, var);
	}

	/** Into Karamatis B the way SETPRO3 takes him (var 3 at Pernos), var 99 afterwards */
	void enterKaramatis() {
		startAt(3);
		holdEssence();
		Npc& pernos = npcBeside(PERNOS);
		ASSERT_TRUE(talk(pernos, QUEST, DA::SETPRO3));
		ASSERT_EQ(player().getWorldId(), KARAMATIS_B);
		ASSERT_EQ(varOf(QUEST), 99);
		levelReady();
	}

	void holdEssence() {
		ASSERT_TRUE(give(DAMINUS_ESSENCE));
	}

	/** Belpartan (205000), whom the shipped Karamatis spawn file puts into every Karamatis B instance */
	Npc& belpartan() {
		std::vector<Ptr<Npc>> found = npcsOf(BELPARTAN);
		EXPECT_EQ(found.size(), 1u);
		return *found.at(0);
	}

	/** From var 99 at Belpartan to the four raiders, 43 s later */
	void summonRaiders() {
		ASSERT_TRUE(talk(belpartan(), QUEST, DA::QUEST_SELECT));
		ASSERT_EQ(varOf(QUEST), 50);
		landAt(ARENA_X, ARENA_Y, ARENA_Z);
		executor().advance(std::chrono::milliseconds(43000));
		ASSERT_EQ(varOf(QUEST), 51);
		ASSERT_EQ(npcsOf(RAIDER).size(), 4u);
	}
};

// ---- register (_1006Ascension.java:47-64) ---------------------------------------------------------------------------------------------------

TEST_F(Quest1006Test, RegistersItsNpcsItemAndEventsWhileTheSimpleClassChangeIsOff) {
	registerHandlers();
	QuestEngine& engine = QuestEngine::getInstance();
	ASSERT_TRUE(engine.isHaveHandler(QUEST));
	for (int32_t npcId : {PERNOS, DAMINU, BELPARTAN})
		EXPECT_TRUE(engine.getQuestNpc(npcId)->getOnTalkEvent().contains(QUEST)) << npcId;
	for (int32_t npcId : {RAIDER, ORISSAN})
		EXPECT_TRUE(engine.getQuestNpc(npcId)->getOnKillEvent().contains(QUEST)) << npcId;
	EXPECT_FALSE(engine.getQuestNpc(PERNOS)->getOnQuestStart().contains(QUEST)) << "a mission: no start npc";
}

TEST_F(Quest1006Test, RegistersNothingWhileTheSimpleClassChangeIsOn) {
	configs::main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.store(true);
	registerHandlers();
	QuestEngine& engine = QuestEngine::getInstance();
	for (int32_t npcId : {PERNOS, DAMINU, BELPARTAN, RAIDER, ORISSAN}) {
		EXPECT_FALSE(engine.getQuestNpc(npcId)->getOnTalkEvent().contains(QUEST)) << npcId;
		EXPECT_FALSE(engine.getQuestNpc(npcId)->getOnKillEvent().contains(QUEST)) << npcId;
	}
	EXPECT_FALSE(engine.getQuestNpc(PERNOS)->getOnTalkEvent().contains(1007)) << "1007 skips its registration as well";
	// no level-changed registration: level 9 starts nothing
	spawnQuester(Race::ELYOS, 9, POETA, 1, POETA_X, POETA_Y, POETA_Z);
	engine.onLevelChanged(player());
	EXPECT_FALSE(statusOf(QUEST).has_value());
	EXPECT_FALSE(statusOf(1007).has_value());
}

// ---- onLevelChangedEvent: defaultOnLevelChangedEvent (quest_data.xml:65: minlevel_permitted 9, a MISSION) ---------------------------------

TEST_F(Quest1006Test, LevelNineStartsTheMission) {
	registerHandlers();
	spawnQuester(Race::ELYOS, 9, POETA, 1, POETA_X, POETA_Y, POETA_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(QUEST), QuestStatus::START);
	EXPECT_EQ(varOf(QUEST), 0);
	EXPECT_TRUE(wasSent(questAdd(QUEST, START))) << "SM_QUEST_ACTION ADD";
	EXPECT_FALSE(statusOf(1007).has_value()) << "1007 waits for 1006";
}

TEST_F(Quest1006Test, LevelEightLocksTheMission) {
	registerHandlers();
	spawnQuester(Race::ELYOS, 8, POETA, 1, POETA_X, POETA_Y, POETA_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(QUEST), QuestStatus::LOCKED) << "level 8";
	EXPECT_TRUE(wasSent(questAdd(QUEST, LOCKED)));
}

TEST_F(Quest1006Test, LevelSevenLocksTheMission) {
	registerHandlers();
	spawnQuester(Race::ELYOS, 7, POETA, 1, POETA_X, POETA_Y, POETA_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(QUEST), QuestStatus::LOCKED);
}

TEST_F(Quest1006Test, LevelSixShowsNothing) {
	registerHandlers();
	spawnQuester(Race::ELYOS, 6, POETA, 1, POETA_X, POETA_Y, POETA_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_FALSE(statusOf(QUEST).has_value());
}

TEST_F(Quest1006Test, AnAsmodianIsNotOfferedTheElyosMission) {
	registerHandlers();
	spawnQuester(Race::ASMODIANS, 9, ISHALGEN, 1, 100.0f, 100.0f, 100.0f);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_FALSE(statusOf(QUEST).has_value());
	EXPECT_EQ(statusOf(2008), QuestStatus::START) << "2008 instead";
}

// ---- Pernos in Poeta (:79-97) ------------------------------------------------------------------------------------------------------------

TEST_F(Quest1006Test, PernosOpensThePagesOfVarsZeroThreeAndFiveOnly) {
	startAt(0);
	Npc& pernos = npcBeside(PERNOS);
	EXPECT_TRUE(talk(pernos, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), 1011, QUEST)));
	state(QUEST)->setQuestVar(3);
	EXPECT_TRUE(talk(pernos, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), 1693, QUEST)));
	state(QUEST)->setQuestVar(5);
	EXPECT_TRUE(talk(pernos, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), 2034, QUEST)));
	clearSent();
	state(QUEST)->setQuestVar(1);
	EXPECT_FALSE(talk(pernos, QUEST, DA::QUEST_SELECT)) << "var 1: no page";
	EXPECT_FALSE(sentOpcode(SM_DIALOG_WINDOW_OPCODE));
}

TEST_F(Quest1006Test, Setpro1GivesTheBottleSetsVarOneAndBeamsToClionaLake) {
	startAt(0);
	Npc& pernos = npcBeside(PERNOS);
	EXPECT_TRUE(talk(pernos, QUEST, DA::SETPRO1));
	EXPECT_EQ(varOf(QUEST), 1);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(PERNOS_BOTTLE), 1);
	EXPECT_TRUE(wasSent(questUpdate(QUEST, START, 1)));
	EXPECT_FALSE(player().isSpawned()) << "FADE_OUT_BEAM: despawned until the animation is done";
	EXPECT_TRUE(wasSent(beamTo(POETA, 657.0f, 1071.0f, 99.375f, 72))) << ":95 the whole destination";
	animationDone();
	EXPECT_TRUE(player().isSpawned());
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), 657.0f);
	EXPECT_FLOAT_EQ(player().getY(), 1071.0f);
	EXPECT_FLOAT_EQ(player().getZ(), 99.375f);
	// a second SETPRO1 does not give a second bottle (:91: only while he holds none)
	ASSERT_TRUE(talk(pernos, QUEST, DA::SETPRO1));
	animationDone();
	EXPECT_EQ(player().getInventory().getItemCountByItemId(PERNOS_BOTTLE), 1);
}

TEST_F(Quest1006Test, TheBottleOutsideItsZoneDoesNothingAndAnswersSuccess) {
	startAt(1);
	ASSERT_TRUE(give(PERNOS_BOTTLE));
	Ptr<model::gameobjects::Item> bottle = player().getInventory().getFirstItemByItemId(PERNOS_BOTTLE);
	ASSERT_TRUE(bottle);
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), QUEST, 0);
	EXPECT_EQ(QuestEngine::getInstance().onItemUseEvent(*env, *bottle), questEngine::handlers::HandlerResult::SUCCESS) << ":202 `// ??`";
	EXPECT_EQ(varOf(QUEST), 1);
}

// ---- Daminu (:133-149) ---------------------------------------------------------------------------------------------------------------------

TEST_F(Quest1006Test, DaminuTalksOnlyToAHolderOfTheFilledBottleAtVarTwo) {
	startAt(2);
	Npc& daminu = npcBeside(DAMINU);
	EXPECT_FALSE(talk(daminu, QUEST, DA::QUEST_SELECT)) << "no filled bottle";
	ASSERT_TRUE(give(FILLED_BOTTLE));
	EXPECT_TRUE(talk(daminu, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(daminu.getObjectId(), 1352, QUEST)));
	// SELECT2_1: movie 14, then page 1353
	clearSent();
	EXPECT_TRUE(talk(daminu, QUEST, DA::SELECT2_1));
	EXPECT_TRUE(wasSent(questMovie(daminu.getObjectId(), QUEST, 14)));
	EXPECT_TRUE(wasSent(dialogWindow(daminu.getObjectId(), 1353, QUEST)));
	// SETPRO2: the filled bottle for the essence, var 3, beamed back to Pernos
	EXPECT_TRUE(talk(daminu, QUEST, DA::SETPRO2));
	EXPECT_EQ(varOf(QUEST), 3);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(FILLED_BOTTLE), 0);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(DAMINUS_ESSENCE), 1);
	EXPECT_TRUE(wasSent(beamTo(POETA, 246.0f, 1639.0f, 100.316f, 56))) << ":148 the whole destination";
	animationDone();
	EXPECT_FLOAT_EQ(player().getX(), 246.0f);
	EXPECT_FLOAT_EQ(player().getY(), 1639.0f);
	EXPECT_FLOAT_EQ(player().getZ(), 100.316f);
}

TEST_F(Quest1006Test, DaminusSetpro2OutsideVarTwoDoesNothing) {
	startAt(1);
	ASSERT_TRUE(give(FILLED_BOTTLE));
	Npc& daminu = npcBeside(DAMINU);
	EXPECT_FALSE(talk(daminu, QUEST, DA::SETPRO2));
	EXPECT_EQ(varOf(QUEST), 1);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(FILLED_BOTTLE), 1);
}

// ---- SETPRO3: Karamatis B (:98-104) ---------------------------------------------------------------------------------------------------------

TEST_F(Quest1006Test, Setpro3CreatesKaramatisTakesTheEssenceAndSetsNinetyNine) {
	enterKaramatis();
	Ptr<world::WorldMapInstance> instance = player().getWorldMapInstance();
	ASSERT_TRUE(instance);
	EXPECT_GT(instance->getInstanceId(), 1) << "a new instance, not the map's first";
	EXPECT_TRUE(instance->isRegistered(player().getObjectId())) << "InstanceService.getNextAvailableInstance registers him";
	EXPECT_FLOAT_EQ(player().getX(), 52.0f);
	EXPECT_FLOAT_EQ(player().getY(), 174.0f);
	EXPECT_FLOAT_EQ(player().getZ(), 229.0f);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(DAMINUS_ESSENCE), 0);
	EXPECT_TRUE(wasSent(questUpdate(QUEST, START, 99)));
}

// ---- Belpartan and the raiders (:150-176, :205-231) ------------------------------------------------------------------------------------------

TEST_F(Quest1006Test, BelpartanFliesHimAndTheRaidersComeFortyThreeSecondsLater) {
	enterKaramatis();
	clearSent();
	ASSERT_TRUE(talk(belpartan(), QUEST, DA::QUEST_SELECT));
	EXPECT_EQ(varOf(QUEST), 50);
	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_FALSE(player().isInState(CreatureState::ACTIVE));
	ASSERT_TRUE(player().getFlightPath());
	EXPECT_EQ(player().getFlightPath()->getId(), 1001) << "setFlightTeleportId(1001)";
	EXPECT_TRUE(wasSent(cptest::serialized(network::aion::serverpackets::SM_EMOTION(player(), model::EmotionType::START_FLYTELEPORT, 1001, 0))));
	EXPECT_TRUE(wasSent(questUpdate(QUEST, START, 50)));
	EXPECT_TRUE(npcsOf(RAIDER).empty());
	landAt(ARENA_X, ARENA_Y, ARENA_Z);
	executor().advance(std::chrono::milliseconds(42999));
	EXPECT_EQ(varOf(QUEST), 50);
	EXPECT_TRUE(npcsOf(RAIDER).empty());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(varOf(QUEST), 51);
	std::vector<Ptr<Npc>> raiders = npcsOf(RAIDER);
	ASSERT_EQ(raiders.size(), 4u);
	for (const Ptr<Npc>& raider : raiders)
		EXPECT_EQ(raider->getAggroList().getHate(player()), 1000) << "addHate(player, 1000)";
	EXPECT_TRUE(player().getEffectController()->hasAbnormalEffect(281)) << ":157 applyEffectDirectly(281): Belpartan's Blessing";
	EXPECT_FALSE(player().getEffectController()->hasAbnormalEffect(257)) << "not 2008's Shield of Hagen";
}

TEST_F(Quest1006Test, BelpartanSaysNothingBeforeVarNinetyNine) {
	enterKaramatis();
	state(QUEST)->setQuestVar(50);
	EXPECT_FALSE(talk(belpartan(), QUEST, DA::QUEST_SELECT));
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_TRUE(npcsOf(RAIDER).empty());
}

TEST_F(Quest1006Test, FourRaidersCountFiftyOneToFiftyFourThenOrissanComes) {
	enterKaramatis();
	summonRaiders();
	std::vector<Ptr<Npc>> raiders = npcsOf(RAIDER);
	for (int32_t i = 0; i < 3; i++) {
		EXPECT_TRUE(kill(*raiders[i]));
		EXPECT_EQ(varOf(QUEST), 52 + i);
		EXPECT_FALSE(raiders[i]->isSpawned()) << "deleted";
	}
	EXPECT_TRUE(npcsOf(ORISSAN).empty());
	EXPECT_TRUE(kill(*raiders[3]));
	EXPECT_EQ(varOf(QUEST), 4);
	std::vector<Ptr<Npc>> orissan = npcsOf(ORISSAN);
	ASSERT_EQ(orissan.size(), 1u);
	EXPECT_EQ(orissan[0]->getAggroList().getHate(player()), 1000);
	EXPECT_FLOAT_EQ(orissan[0]->getX(), 226.7f);
	EXPECT_FLOAT_EQ(orissan[0]->getY(), 251.5f);
}

TEST_F(Quest1006Test, OrissansDeathPlaysMovie151ClearsTheInstanceAndBringsPernos) {
	enterKaramatis();
	summonRaiders();
	for (const Ptr<Npc>& raider : npcsOf(RAIDER))
		kill(*raider);
	Ptr<Npc> orissan = npcsOf(ORISSAN).at(0);
	clearSent();
	EXPECT_TRUE(kill(*orissan)) << "QuestEngine.onKill: no exception (the handler's own false at :233 is not observable)";
	EXPECT_EQ(varOf(QUEST), 5);
	EXPECT_TRUE(wasSent(questMovie(orissan->getObjectId(), QUEST, 151)));
	EXPECT_TRUE(npcsOf(BELPARTAN).empty()) << "forEachNpc deletes every npc of the instance";
	std::vector<Ptr<Npc>> pernos = npcsOf(PERNOS);
	ASSERT_EQ(pernos.size(), 1u);
	EXPECT_FLOAT_EQ(pernos[0]->getX(), 220.6f);
	size_t npcCount = 0;
	player().getWorldMapInstance()->forEachNpc([&](Npc&) { npcCount++; });
	EXPECT_EQ(npcCount, 1u) << "only Pernos";
}

TEST_F(Quest1006Test, ARaiderKilledOutsideTheCountIsDeletedAndCountsNothing) {
	enterKaramatis();
	summonRaiders();
	state(QUEST)->setQuestVar(4);
	Ptr<Npc> raider = npcsOf(RAIDER).at(0);
	EXPECT_TRUE(kill(*raider));
	EXPECT_EQ(varOf(QUEST), 4);
	EXPECT_FALSE(raider->isSpawned());
}

// ---- the class selection (:106-129, :233-239) ------------------------------------------------------------------------------------------------

TEST_F(Quest1006Test, Setpro4ShowsTheWarriorsPageAndSetpro5MakesAGladiatorAndReward) {
	startAt(5);
	Npc& pernos = npcBeside(PERNOS);
	EXPECT_TRUE(talk(pernos, QUEST, DA::SETPRO4));
	EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), 2375, QUEST))) << "ClassChangeService.getClassSelectionDialogPageId(ELYOS, WARRIOR)";
	clearSent();
	EXPECT_TRUE(talk(pernos, QUEST, DA::SETPRO5));
	EXPECT_EQ(player().getPlayerClass(), PlayerClass::GLADIATOR);
	EXPECT_EQ(statusOf(QUEST), QuestStatus::REWARD);
	EXPECT_EQ(varOf(QUEST), 5) << "changeQuestStep(env, 5, 5, true)";
	EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), 5, QUEST)));
	EXPECT_FALSE(player().getCommonData()->isDaeva()) << "setClass(player, class): no daeva update";
}

TEST_F(Quest1006Test, Setpro6MakesATemplarAndAScoutsClassIsRefused) {
	startAt(5);
	Npc& pernos = npcBeside(PERNOS);
	EXPECT_FALSE(talk(pernos, QUEST, DA::SETPRO7)) << "an Assassin is no Warrior's class: setClass refuses";
	EXPECT_EQ(player().getPlayerClass(), PlayerClass::WARRIOR);
	EXPECT_EQ(statusOf(QUEST), QuestStatus::START);
	EXPECT_TRUE(talk(pernos, QUEST, DA::SETPRO6));
	EXPECT_EQ(player().getPlayerClass(), PlayerClass::TEMPLAR);
}

TEST_F(Quest1006Test, Setpro4ShowsThePageOfEveryStartingClass) {
	startAt(5);
	Npc& pernos = npcBeside(PERNOS);
	// ClassChangeService.getClassSelectionDialogPageId(ELYOS, class) (ClassChangeService.java)
	const std::pair<PlayerClass, int32_t> pages[] = {{PlayerClass::WARRIOR, 2375}, {PlayerClass::SCOUT, 2716}, {PlayerClass::MAGE, 3057},
		{PlayerClass::PRIEST, 3398}, {PlayerClass::ENGINEER, 3739}, {PlayerClass::ARTIST, 4080}};
	for (const auto& [startingClass, page] : pages) {
		actor.commonData->setPlayerClass(startingClass);
		clearSent();
		EXPECT_TRUE(talk(pernos, QUEST, DA::SETPRO4)) << static_cast<int>(startingClass);
		EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), page, QUEST))) << static_cast<int>(startingClass);
	}
}

TEST_F(Quest1006Test, EverySetproOfTheClassSelectionMakesItsClass) {
	startAt(5);
	Npc& pernos = npcBeside(PERNOS);
	// :110-129: SETPRO5-15, each for the starting class whose second class it is (ClassChangeService.setClass validates the pair)
	struct Row {
		PlayerClass startingClass;
		int32_t action;
		PlayerClass newClass;
	};
	const Row rows[] = {{PlayerClass::WARRIOR, DA::SETPRO5, PlayerClass::GLADIATOR}, {PlayerClass::WARRIOR, DA::SETPRO6, PlayerClass::TEMPLAR},
		{PlayerClass::SCOUT, DA::SETPRO7, PlayerClass::ASSASSIN}, {PlayerClass::SCOUT, DA::SETPRO8, PlayerClass::RANGER},
		{PlayerClass::MAGE, DA::SETPRO9, PlayerClass::SORCERER}, {PlayerClass::MAGE, DA::SETPRO10, PlayerClass::SPIRIT_MASTER},
		{PlayerClass::PRIEST, DA::SETPRO11, PlayerClass::CLERIC}, {PlayerClass::PRIEST, DA::SETPRO12, PlayerClass::CHANTER},
		{PlayerClass::ENGINEER, DA::SETPRO13, PlayerClass::GUNNER}, {PlayerClass::ARTIST, DA::SETPRO14, PlayerClass::BARD},
		{PlayerClass::ENGINEER, DA::SETPRO15, PlayerClass::RIDER}};
	for (const Row& row : rows) {
		actor.commonData->setPlayerClass(row.startingClass);
		state(QUEST)->setStatus(QuestStatus::START);
		state(QUEST)->setQuestVar(5);
		EXPECT_TRUE(talk(pernos, QUEST, row.action)) << row.action;
		EXPECT_EQ(player().getPlayerClass(), row.newClass) << row.action;
		EXPECT_EQ(statusOf(QUEST), QuestStatus::REWARD) << row.action;
	}
}

TEST_F(Quest1006Test, NoClassBeforeVarFive) {
	startAt(4);
	Npc& pernos = npcBeside(PERNOS);
	EXPECT_FALSE(talk(pernos, QUEST, DA::SETPRO4));
	EXPECT_FALSE(talk(pernos, QUEST, DA::SETPRO5));
	EXPECT_EQ(player().getPlayerClass(), PlayerClass::WARRIOR);
}

// ---- the reward (:179-188, :280-288), from Karamatis with a full level-9 bar ---------------------------------------------------------------

TEST_F(Quest1006Test, TheRewardBeamsHimToPoetaFinishesTheQuestMakesADaevaAndStarts1007) {
	registerHandlers();
	spawnQuester(Race::ELYOS, 9, POETA, 1, POETA_X, POETA_Y, POETA_Z, LEVEL_10_EXP);
	ASSERT_EQ(player().getLevel(), 9) << "a starting class is held below level 10";
	ASSERT_EQ(player().getCommonData()->getExp(), LEVEL_10_EXP) << "the full bar";
	hold(QUEST, QuestStatus::START, 3);
	holdEssence();
	Npc& pernos = npcBeside(PERNOS);
	ASSERT_TRUE(talk(pernos, QUEST, DA::SETPRO3));
	ASSERT_EQ(player().getWorldId(), KARAMATIS_B);
	levelReady();
	state(QUEST)->setQuestVar(5);
	Npc& pernosInside = npcBeside(PERNOS);
	ASSERT_TRUE(talk(pernosInside, QUEST, DA::SETPRO5));
	ASSERT_EQ(statusOf(QUEST), QuestStatus::REWARD);
	clearSent();

	EXPECT_TRUE(talk(pernosInside, QUEST, DA::SELECTED_QUEST_NOREWARD));
	EXPECT_EQ(statusOf(QUEST), QuestStatus::COMPLETE);
	EXPECT_TRUE(wasSent(beamTo(POETA, 245.14868f, 1639.1372f, 100.35713f, 60))) << ":185 the whole destination";
	animationDone();
	EXPECT_EQ(player().getWorldId(), POETA) << ":184-185: out of Karamatis";
	EXPECT_FLOAT_EQ(player().getX(), 245.14868f);
	EXPECT_FLOAT_EQ(player().getY(), 1639.1372f);
	EXPECT_FLOAT_EQ(player().getZ(), 100.35713f);
	EXPECT_TRUE(player().getCommonData()->isDaeva()) << "onQuestCompletedEvent: updateDaeva";
	// Java's order (QuestService.finishQuest:103-113): the reward's exp is added while he is no Daeva yet, so PlayerCommonData.setExp holds him at
	// level 9 with the full bar and the 73,200 exp are lost; the Daeva status comes after it (docs/deviations/Q06.md, kept)
	EXPECT_EQ(player().getCommonData()->getExp(), LEVEL_10_EXP);
	EXPECT_EQ(player().getLevel(), 9);
	// the next exp he earns as a Daeva lifts him to level 10
	player().getCommonData()->addExp(1, model::gameobjects::player::Rates::XP_QUEST);
	EXPECT_EQ(player().getLevel(), 10);
	// 1007 starts (1007.onQuestCompletedEvent: defaultOnQuestCompletedEvent(env, 1006))
	EXPECT_EQ(statusOf(1007), QuestStatus::START);
	EXPECT_TRUE(wasSent(questAdd(1007, START)));
}

TEST_F(Quest1006Test, AQuesterBelowAFullBarReachesTheCapWithTheRewardExp) {
	registerHandlers();
	spawnQuester(Race::ELYOS, 9, POETA, 1, POETA_X, POETA_Y, POETA_Z, LEVEL_9_EXP);
	hold(QUEST, QuestStatus::START, 5);
	Npc& pernos = npcBeside(PERNOS);
	ASSERT_TRUE(talk(pernos, QUEST, DA::SETPRO5));
	EXPECT_TRUE(talk(pernos, QUEST, DA::SELECTED_QUEST_NOREWARD));
	EXPECT_EQ(statusOf(QUEST), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getWorldId(), POETA) << "not in Karamatis: no teleport";
	EXPECT_EQ(player().getCommonData()->getExp(), LEVEL_10_EXP) << "82,982 + 73,200 capped at the full level-9 bar";
	EXPECT_EQ(player().getLevel(), 9);
	EXPECT_TRUE(player().getCommonData()->isDaeva());
}

// ---- onDieEvent and onEnterWorldEvent (:241-265) -----------------------------------------------------------------------------------------------

TEST_F(Quest1006Test, DyingInKaramatisSetsVarBackToThree) {
	enterKaramatis();
	summonRaiders();
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0, 0);
	QuestEngine::getInstance().onDie(*env);
	EXPECT_EQ(varOf(QUEST), 3);
}

TEST_F(Quest1006Test, DyingInKaramatisDuringOrissansFightSetsVarFourBackToThreeButKeepsThree) {
	enterKaramatis();
	state(QUEST)->setQuestVar(4); // Orissan's fight (:223)
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0, 0);
	QuestEngine::getInstance().onDie(*env);
	EXPECT_EQ(varOf(QUEST), 3) << ":250 var > 3";
	clearSent();
	QuestEngine::getInstance().onDie(*env);
	EXPECT_EQ(varOf(QUEST), 3);
	EXPECT_FALSE(sentOpcode(SM_QUEST_ACTION_OPCODE)) << "var 3: no step change";
}

TEST_F(Quest1006Test, DyingOutsideKaramatisKeepsTheVar) {
	startAt(51);
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0, 0);
	QuestEngine::getInstance().onDie(*env);
	EXPECT_EQ(varOf(QUEST), 51);
}

TEST_F(Quest1006Test, EnteringTheWorldInKaramatisMorphsHim) {
	enterKaramatis();
	clearSent();
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_TRUE(wasSent(ascensionMorph(1)));
	EXPECT_EQ(varOf(QUEST), 99);
}

TEST_F(Quest1006Test, EnteringTheWorldOutsideAfterVarThreeGoesBackToThreeExceptAtFive) {
	startAt(51);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(QUEST), 3);
	EXPECT_FALSE(sentOpcode(SM_ASCENSION_MORPH_OPCODE));
	state(QUEST)->setQuestVar(5);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(QUEST), 5) << "5 is the class selection";
	state(QUEST)->setQuestVar(2);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(QUEST), 2);
}

} // namespace
} // namespace aion::gameserver::handlers::quest::ascension::test
