// _2008Ascension (lane P6-Q asc-hand, chunk Q06; Java: quest/ascension/_2008Ascension.java) on the real engine and the route's data rows
// (AscensionQuestTestSupport.h): the registration and CustomConfig.ENABLE_SIMPLE_2NDCLASS (:41-57), the start at level 9, Munin, Urd,
// Verdandi and Skuld with their cards and vars (:92-219), the movie and the cards taken back (:109-116), the Ataxiar B instance (:124-134),
// Hagen's flight and the guardian assassins 43 s later (:220-243), the kill counting and Hellion (:59-91), the class selection and setClass
// (:135-162, :277-283), the reward teleport, the finish and the Daeva status (:245-255, :296-304), the death and the enter world (:262-294),
// and 2009 starting after 2008 completes.

#include "AscensionQuestTestSupport.h"

#include <gtest/gtest.h>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::handlers::quest::ascension::test {
namespace {

namespace DA = ::aion::gameserver::model::DialogAction;
using ::aion::gameserver::model::gameobjects::state::CreatureState;

constexpr int32_t QUEST = 2008;
// the Ishalgen spot of the quester (Munin's village, _2008Ascension.java:253's return point)
constexpr float ISHALGEN_X = 386.0f, ISHALGEN_Y = 1894.0f, ISHALGEN_Z = 327.6f;
// the guardian assassins' arena in Ataxiar B: Hellion's spot (:75)
constexpr float ARENA_X = 301.0f, ARENA_Y = 259.0f, ARENA_Z = 205.5f;

class Quest2008Test : public AscensionQuestTest {
protected:
	void startAt(int32_t var, std::optional<int64_t> exp = std::nullopt) {
		registerHandlers();
		spawnQuester(Race::ASMODIANS, 9, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z, exp);
		hold(QUEST, QuestStatus::START, var);
	}

	int64_t held(int32_t itemId) { return player().getInventory().getItemCountByItemId(itemId); }

	/** Into Ataxiar B the way SETPRO5 takes him (var 4 at Munin), var 99 afterwards */
	void enterAtaxiar() {
		startAt(4);
		Npc& munin = npcBeside(MUNIN);
		ASSERT_TRUE(talk(munin, QUEST, DA::SETPRO5));
		ASSERT_EQ(player().getWorldId(), ATAXIAR_B);
		ASSERT_EQ(varOf(QUEST), 99);
		levelReady();
	}

	/** Hagen (205020), whom the shipped Ataxiar spawn file puts into every Ataxiar B instance */
	Npc& hagen() {
		std::vector<Ptr<Npc>> found = npcsOf(HAGEN);
		EXPECT_EQ(found.size(), 1u);
		return *found.at(0);
	}

	void summonAssassins() {
		ASSERT_TRUE(talk(hagen(), QUEST, DA::QUEST_SELECT));
		ASSERT_EQ(varOf(QUEST), 50);
		landAt(ARENA_X, ARENA_Y, ARENA_Z);
		executor().advance(std::chrono::milliseconds(43000));
		ASSERT_EQ(varOf(QUEST), 51);
		ASSERT_EQ(npcsOf(GUARDIAN_ASSASSIN).size(), 4u);
	}
};

TEST_F(Quest2008Test, RegistersItsNpcsAndEventsWhileTheSimpleClassChangeIsOff) {
	registerHandlers();
	QuestEngine& engine = QuestEngine::getInstance();
	ASSERT_TRUE(engine.isHaveHandler(QUEST));
	for (int32_t npcId : {MUNIN, URD, VERDANDI, SKULD, HAGEN})
		EXPECT_TRUE(engine.getQuestNpc(npcId)->getOnTalkEvent().contains(QUEST)) << npcId;
	for (int32_t npcId : {GUARDIAN_ASSASSIN, HELLION})
		EXPECT_TRUE(engine.getQuestNpc(npcId)->getOnKillEvent().contains(QUEST)) << npcId;
}

TEST_F(Quest2008Test, RegistersNothingWhileTheSimpleClassChangeIsOn) {
	configs::main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.store(true);
	registerHandlers();
	QuestEngine& engine = QuestEngine::getInstance();
	for (int32_t npcId : {MUNIN, URD, VERDANDI, SKULD, HAGEN, GUARDIAN_ASSASSIN, HELLION}) {
		EXPECT_FALSE(engine.getQuestNpc(npcId)->getOnTalkEvent().contains(QUEST)) << npcId;
		EXPECT_FALSE(engine.getQuestNpc(npcId)->getOnKillEvent().contains(QUEST)) << npcId;
	}
	EXPECT_FALSE(engine.getQuestNpc(MUNIN)->getOnTalkEvent().contains(2009));
	spawnQuester(Race::ASMODIANS, 9, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z);
	engine.onLevelChanged(player());
	EXPECT_FALSE(statusOf(QUEST).has_value());
}

TEST_F(Quest2008Test, LevelEightLocksTheMission) {
	registerHandlers();
	spawnQuester(Race::ASMODIANS, 8, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(QUEST), QuestStatus::LOCKED);
	EXPECT_FALSE(statusOf(1006).has_value()) << "no Elyos mission";
}

TEST_F(Quest2008Test, LevelSevenLocksTheMission) {
	registerHandlers();
	spawnQuester(Race::ASMODIANS, 7, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(QUEST), QuestStatus::LOCKED);
}

TEST_F(Quest2008Test, LevelSixShowsNothing) {
	registerHandlers();
	spawnQuester(Race::ASMODIANS, 6, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_FALSE(statusOf(QUEST).has_value());
}

TEST_F(Quest2008Test, LevelNineStartsTheMission) {
	registerHandlers();
	spawnQuester(Race::ASMODIANS, 9, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(QUEST), QuestStatus::START);
	EXPECT_TRUE(wasSent(questAdd(QUEST, START)));
}

// ---- Munin, Urd, Verdandi, Skuld (:101-219) -------------------------------------------------------------------------------------------------

TEST_F(Quest2008Test, MuninSendsHimToUrdWhoGivesTheCardOfThePast) {
	startAt(0);
	Npc& munin = npcBeside(MUNIN);
	EXPECT_TRUE(talk(munin, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 1011, QUEST)));
	EXPECT_TRUE(talk(munin, QUEST, DA::SETPRO1));
	EXPECT_EQ(varOf(QUEST), 1);
	EXPECT_TRUE(wasSent(beamTo(ISHALGEN, 585.5074f, 2416.0312f, 278.625f, 102))) << ":123 the whole destination";
	animationDone();
	EXPECT_FLOAT_EQ(player().getX(), 585.5074f);
	EXPECT_FLOAT_EQ(player().getY(), 2416.0312f);
	EXPECT_FLOAT_EQ(player().getZ(), 278.625f);
	Npc& urd = npcBeside(URD);
	EXPECT_TRUE(talk(urd, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(urd.getObjectId(), 1352, QUEST)));
	EXPECT_TRUE(talk(urd, QUEST, DA::SETPRO2));
	EXPECT_EQ(varOf(QUEST), 2);
	EXPECT_EQ(held(CARD_OF_THE_PAST), 1);
	EXPECT_TRUE(wasSent(beamTo(ISHALGEN, 940.74475f, 2295.5305f, 265.65674f, 46))) << ":176";
	animationDone();
	EXPECT_FLOAT_EQ(player().getX(), 940.74475f);
	EXPECT_FLOAT_EQ(player().getY(), 2295.5305f);
	EXPECT_FLOAT_EQ(player().getZ(), 265.65674f);
	Npc& verdandi = npcBeside(VERDANDI);
	EXPECT_FALSE(talk(urd, QUEST, DA::SETPRO2)) << "Urd: var 1 only";
	EXPECT_TRUE(talk(verdandi, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(verdandi.getObjectId(), 1693, QUEST)));
	EXPECT_TRUE(talk(verdandi, QUEST, DA::SETPRO3));
	EXPECT_EQ(varOf(QUEST), 3);
	EXPECT_EQ(held(CARD_OF_THE_PRESENT), 1);
	EXPECT_TRUE(wasSent(beamTo(ISHALGEN, 1111.5637f, 1719.2745f, 270.114256f, 114))) << ":193";
	animationDone();
	EXPECT_FLOAT_EQ(player().getX(), 1111.5637f);
	EXPECT_FLOAT_EQ(player().getY(), 1719.2745f);
	EXPECT_FLOAT_EQ(player().getZ(), 270.114256f);
	Npc& skuld = npcBeside(SKULD);
	EXPECT_TRUE(talk(skuld, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(skuld.getObjectId(), 2034, QUEST)));
	EXPECT_TRUE(talk(skuld, QUEST, DA::SETPRO4));
	EXPECT_EQ(varOf(QUEST), 4);
	EXPECT_EQ(held(CARD_OF_THE_FUTURE), 1);
	EXPECT_TRUE(wasSent(beamTo(ISHALGEN, 383.10248f, 1895.3093f, 327.625f, 59))) << ":210";
	animationDone();
	EXPECT_FLOAT_EQ(player().getX(), 383.10248f);
	EXPECT_FLOAT_EQ(player().getY(), 1895.3093f);
	EXPECT_FLOAT_EQ(player().getZ(), 327.625f);
	EXPECT_TRUE(wasSent(questUpdate(QUEST, START, 4)));
}

TEST_F(Quest2008Test, MuninAtVarFourPlaysMovie57AndTakesTheThreeCardsButAnswersFalse) {
	startAt(4);
	ASSERT_TRUE(give(CARD_OF_THE_PAST) && give(CARD_OF_THE_PRESENT) && give(CARD_OF_THE_FUTURE));
	Npc& munin = npcBeside(MUNIN);
	EXPECT_TRUE(talk(munin, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 2375, QUEST)));
	EXPECT_FALSE(talk(munin, QUEST, DA::SELECT5_1)) << ":116 returns false after the movie";
	EXPECT_TRUE(wasSent(questMovie(munin.getObjectId(), QUEST, 57)));
	EXPECT_EQ(held(CARD_OF_THE_PAST), 0);
	EXPECT_EQ(held(CARD_OF_THE_PRESENT), 0);
	EXPECT_EQ(held(CARD_OF_THE_FUTURE), 0);
	EXPECT_EQ(varOf(QUEST), 4);
}

TEST_F(Quest2008Test, MuninsSelect5_1OutsideVarFourPlaysNothingAndKeepsTheCards) {
	startAt(3);
	ASSERT_TRUE(give(CARD_OF_THE_PAST) && give(CARD_OF_THE_PRESENT) && give(CARD_OF_THE_FUTURE));
	Npc& munin = npcBeside(MUNIN);
	clearSent();
	EXPECT_FALSE(talk(munin, QUEST, DA::SELECT5_1));
	EXPECT_FALSE(sentOpcode(SM_PLAY_MOVIE_OPCODE)) << ":113 var == 4 only";
	EXPECT_EQ(held(CARD_OF_THE_PAST), 1);
	EXPECT_EQ(held(CARD_OF_THE_PRESENT), 1);
	EXPECT_EQ(held(CARD_OF_THE_FUTURE), 1);
}

TEST_F(Quest2008Test, Setpro5ClosesTheWindowAndOpensAtaxiarB) {
	startAt(4);
	Npc& munin = npcBeside(MUNIN);
	const int32_t muninId = munin.getObjectId();
	EXPECT_TRUE(talk(munin, QUEST, DA::SETPRO5));
	EXPECT_EQ(varOf(QUEST), 99);
	EXPECT_TRUE(wasSent(javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(muninId).H(0).D(0).H(0).H(0)))) << "SM_DIALOG_WINDOW(npc, 0)";
	EXPECT_EQ(player().getWorldId(), ATAXIAR_B);
	EXPECT_GT(player().getInstanceId(), 1);
	EXPECT_FLOAT_EQ(player().getX(), 457.65f);
	EXPECT_FLOAT_EQ(player().getY(), 426.8f);
	EXPECT_FLOAT_EQ(player().getZ(), 230.4f);
}

TEST_F(Quest2008Test, Setpro5BeforeVarFourDoesNothing) {
	startAt(3);
	Npc& munin = npcBeside(MUNIN);
	EXPECT_FALSE(talk(munin, QUEST, DA::SETPRO5));
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_EQ(varOf(QUEST), 3);
}

// ---- Hagen and the guardian assassins (:220-243, :59-91) --------------------------------------------------------------------------------------

TEST_F(Quest2008Test, HagenFliesHimAndTheAssassinsComeFortyThreeSecondsLater) {
	enterAtaxiar();
	ASSERT_TRUE(talk(hagen(), QUEST, DA::QUEST_SELECT));
	EXPECT_EQ(varOf(QUEST), 50);
	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_FALSE(player().isInState(CreatureState::ACTIVE));
	ASSERT_TRUE(player().getFlightPath());
	EXPECT_EQ(player().getFlightPath()->getId(), 3001);
	EXPECT_TRUE(wasSent(cptest::serialized(network::aion::serverpackets::SM_EMOTION(player(), model::EmotionType::START_FLYTELEPORT, 3001, 0))));
	landAt(ARENA_X, ARENA_Y, ARENA_Z);
	executor().advance(std::chrono::milliseconds(42999));
	EXPECT_TRUE(npcsOf(GUARDIAN_ASSASSIN).empty());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(varOf(QUEST), 51);
	std::vector<Ptr<Npc>> assassins = npcsOf(GUARDIAN_ASSASSIN);
	ASSERT_EQ(assassins.size(), 4u);
	for (const Ptr<Npc>& assassin : assassins)
		EXPECT_EQ(assassin->getAggroList().getHate(player()), 1000);
	EXPECT_TRUE(player().getEffectController()->hasAbnormalEffect(257)) << ":219 applyEffectDirectly(257): Shield of Hagen";
	EXPECT_FALSE(player().getEffectController()->hasAbnormalEffect(281)) << "not 1006's Belpartan's Blessing";
}

TEST_F(Quest2008Test, FourAssassinsCountToFiftyFourThenHellionComesAndHisDeathBringsMunin) {
	enterAtaxiar();
	summonAssassins();
	std::vector<Ptr<Npc>> assassins = npcsOf(GUARDIAN_ASSASSIN);
	for (int32_t i = 0; i < 3; i++) {
		EXPECT_TRUE(kill(*assassins[i]));
		EXPECT_EQ(varOf(QUEST), 52 + i);
		EXPECT_FALSE(assassins[i]->isSpawned());
	}
	EXPECT_TRUE(kill(*assassins[3]));
	EXPECT_EQ(varOf(QUEST), 5);
	std::vector<Ptr<Npc>> hellion = npcsOf(HELLION);
	ASSERT_EQ(hellion.size(), 1u);
	EXPECT_EQ(hellion[0]->getAggroList().getHate(player()), 1000);
	EXPECT_FLOAT_EQ(hellion[0]->getX(), 301.0f);
	clearSent();
	EXPECT_TRUE(kill(*hellion[0])) << ":90 returns true";
	EXPECT_EQ(varOf(QUEST), 6);
	EXPECT_TRUE(wasSent(questMovie(hellion[0]->getObjectId(), QUEST, 152)));
	EXPECT_TRUE(npcsOf(HAGEN).empty());
	std::vector<Ptr<Npc>> munin = npcsOf(MUNIN);
	ASSERT_EQ(munin.size(), 1u);
	EXPECT_FLOAT_EQ(munin[0]->getX(), 301.92999f);
}

// ---- the class selection and the reward (:135-162, :245-255, :277-283, :296-304) ---------------------------------------------------------------

TEST_F(Quest2008Test, Setpro6ShowsTheAsmodianWarriorsPageAndSetpro7MakesAGladiator) {
	startAt(6);
	Npc& munin = npcBeside(MUNIN);
	EXPECT_TRUE(talk(munin, QUEST, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 2716, QUEST)));
	EXPECT_TRUE(talk(munin, QUEST, DA::SETPRO6));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 3057, QUEST))) << "getClassSelectionDialogPageId(ASMODIANS, WARRIOR)";
	EXPECT_FALSE(talk(munin, QUEST, DA::SETPRO9)) << "an Assassin: refused";
	EXPECT_TRUE(talk(munin, QUEST, DA::SETPRO7));
	EXPECT_EQ(player().getPlayerClass(), PlayerClass::GLADIATOR);
	EXPECT_EQ(statusOf(QUEST), QuestStatus::REWARD);
	EXPECT_EQ(varOf(QUEST), 6);
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 5, QUEST)));
}

TEST_F(Quest2008Test, Setpro6ShowsThePageOfEveryStartingClass) {
	startAt(6);
	Npc& munin = npcBeside(MUNIN);
	// ClassChangeService.getClassSelectionDialogPageId(ASMODIANS, class) (ClassChangeService.java)
	const std::pair<PlayerClass, int32_t> pages[] = {{PlayerClass::WARRIOR, 3057}, {PlayerClass::SCOUT, 3398}, {PlayerClass::MAGE, 3739},
		{PlayerClass::PRIEST, 4080}, {PlayerClass::ENGINEER, 3569}, {PlayerClass::ARTIST, 3910}};
	for (const auto& [startingClass, page] : pages) {
		actor.commonData->setPlayerClass(startingClass);
		clearSent();
		EXPECT_TRUE(talk(munin, QUEST, DA::SETPRO6)) << static_cast<int>(startingClass);
		EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), page, QUEST))) << static_cast<int>(startingClass);
	}
}

TEST_F(Quest2008Test, EverySetproOfTheClassSelectionMakesItsClass) {
	startAt(6);
	Npc& munin = npcBeside(MUNIN);
	// :141-162: SETPRO7-17 (CHANTER before CLERIC, the reverse of 1006), each for the starting class whose second class it is
	struct Row {
		PlayerClass startingClass;
		int32_t action;
		PlayerClass newClass;
	};
	const Row rows[] = {{PlayerClass::WARRIOR, DA::SETPRO7, PlayerClass::GLADIATOR}, {PlayerClass::WARRIOR, DA::SETPRO8, PlayerClass::TEMPLAR},
		{PlayerClass::SCOUT, DA::SETPRO9, PlayerClass::ASSASSIN}, {PlayerClass::SCOUT, DA::SETPRO10, PlayerClass::RANGER},
		{PlayerClass::MAGE, DA::SETPRO11, PlayerClass::SORCERER}, {PlayerClass::MAGE, DA::SETPRO12, PlayerClass::SPIRIT_MASTER},
		{PlayerClass::PRIEST, DA::SETPRO13, PlayerClass::CHANTER}, {PlayerClass::PRIEST, DA::SETPRO14, PlayerClass::CLERIC},
		{PlayerClass::ENGINEER, DA::SETPRO15, PlayerClass::GUNNER}, {PlayerClass::ARTIST, DA::SETPRO16, PlayerClass::BARD},
		{PlayerClass::ENGINEER, DA::SETPRO17, PlayerClass::RIDER}};
	for (const Row& row : rows) {
		actor.commonData->setPlayerClass(row.startingClass);
		state(QUEST)->setStatus(QuestStatus::START);
		state(QUEST)->setQuestVar(6);
		EXPECT_TRUE(talk(munin, QUEST, row.action)) << row.action;
		EXPECT_EQ(player().getPlayerClass(), row.newClass) << row.action;
		EXPECT_EQ(statusOf(QUEST), QuestStatus::REWARD) << row.action;
	}
}

TEST_F(Quest2008Test, TheRewardInAtaxiarBeamsHimToIshalgenMakesADaevaAndStarts2009) {
	registerHandlers();
	spawnQuester(Race::ASMODIANS, 9, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z, LEVEL_10_EXP);
	hold(QUEST, QuestStatus::START, 4);
	Npc& munin = npcBeside(MUNIN);
	ASSERT_TRUE(talk(munin, QUEST, DA::SETPRO5));
	ASSERT_EQ(player().getWorldId(), ATAXIAR_B);
	levelReady();
	state(QUEST)->setQuestVar(6);
	Npc& muninInside = npcBeside(MUNIN);
	ASSERT_TRUE(talk(muninInside, QUEST, DA::SETPRO8));
	ASSERT_EQ(player().getPlayerClass(), PlayerClass::TEMPLAR);
	clearSent();
	EXPECT_TRUE(talk(muninInside, QUEST, DA::SELECTED_QUEST_NOREWARD));
	EXPECT_EQ(statusOf(QUEST), QuestStatus::COMPLETE);
	EXPECT_TRUE(wasSent(beamTo(ISHALGEN, 386.03476f, 1893.9309f, 327.62283f, 59))) << ":248 the whole destination";
	animationDone();
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_FLOAT_EQ(player().getX(), 386.03476f);
	EXPECT_FLOAT_EQ(player().getY(), 1893.9309f);
	EXPECT_FLOAT_EQ(player().getZ(), 327.62283f);
	EXPECT_TRUE(player().getCommonData()->isDaeva());
	EXPECT_EQ(player().getCommonData()->getExp(), LEVEL_10_EXP) << "the capped reward (docs/deviations/Q06.md)";
	EXPECT_EQ(statusOf(2009), QuestStatus::START);
	EXPECT_TRUE(wasSent(questAdd(2009, START)));
}

// ---- onDieEvent and onEnterWorldEvent (:262-294) ---------------------------------------------------------------------------------------------

TEST_F(Quest2008Test, DyingInAtaxiarSetsVarBackToFour) {
	enterAtaxiar();
	summonAssassins();
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0, 0);
	QuestEngine::getInstance().onDie(*env);
	EXPECT_EQ(varOf(QUEST), 4);
}

TEST_F(Quest2008Test, DyingOutsideAtaxiarKeepsTheVar) {
	startAt(51);
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0, 0);
	QuestEngine::getInstance().onDie(*env);
	EXPECT_EQ(varOf(QUEST), 51) << "only in 320020000";
}

TEST_F(Quest2008Test, EnteringTheWorldInAtaxiarMorphsHim) {
	enterAtaxiar();
	clearSent();
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_TRUE(wasSent(ascensionMorph(1)));
	EXPECT_EQ(varOf(QUEST), 99);
}

TEST_F(Quest2008Test, EnteringTheWorldOutsideAtaxiarResetsToFourExceptAtSix) {
	startAt(52);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(QUEST), 4);
	state(QUEST)->setQuestVar(5);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(QUEST), 4) << "var 5 (Hellion's fight) outside Ataxiar: back to 4 (var > 4)";
	state(QUEST)->setQuestVar(6);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(QUEST), 6);
	state(QUEST)->setQuestVar(3);
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_EQ(varOf(QUEST), 3);
}

} // namespace
} // namespace aion::gameserver::handlers::quest::ascension::test
