// _1007ACeremonyinSanctum and _2009ACeremonyinPandaemonium (lane P6-Q asc-hand, chunk Q06; Java: quest/ascension/_1007ACeremonyinSanctum.java,
// _2009ACeremonyinPandaemonium.java) on the real engine and the route's data rows (AscensionQuestTestSupport.h): the registration and
// CustomConfig.ENABLE_SIMPLE_2NDCLASS, the start after 1006 / 2008 completes (at the level change and at the completion,
// defaultOn*Event(.., 1006 / 2008)), the pages of Pernos / Munin, Leah / Heimdall and Jucleas / Balder with their movies and vars, the
// starting class's reward var and group, and the finish at the class's reward npc with the reward (a Daeva with the full level-9 bar reaches
// level 10 with its exp). SETPRO1 beams to Sanctum / Pandaemonium, which the ascension world of tests/instance does not have: the cases check
// the var, the quest update and the closed window, which come before the beam (SM_TELEPORT_LOC's constructor then throws on the missing map
// template, and QuestEngine.onDialog logs it); gs.scenario.ascension checks the beam's destination.

#include "AscensionQuestTestSupport.h"

#include <gtest/gtest.h>

#include "aion/gameserver/model/DialogAction.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::handlers::quest::ascension::test {
namespace {

namespace DA = ::aion::gameserver::model::DialogAction;

constexpr float POETA_X = 245.0f, POETA_Y = 1639.0f, POETA_Z = 100.0f;
constexpr float ISHALGEN_X = 386.0f, ISHALGEN_Y = 1894.0f, ISHALGEN_Z = 327.6f;
constexpr int32_t TEA_OF_REPOSE = 162001057;
constexpr int32_t PROPHECY_SWORD = 100000652; // 1007's first fighter reward (quest_data.xml)
constexpr int32_t KARMIC_SWORD = 100000640;   // 2009's

class CeremonyTest : public AscensionQuestTest {
protected:
	/** A level-9 Gladiator Daeva with a full bar whose ascension quest is COMPLETE, holding the ceremony at `var` (or nothing) */
	void daeva(Race race, std::optional<int32_t> ceremonyVar) {
		registerHandlers();
		if (race == Race::ELYOS)
			spawnQuester(race, 9, POETA, 1, POETA_X, POETA_Y, POETA_Z, LEVEL_10_EXP);
		else
			spawnQuester(race, 9, ISHALGEN, 1, ISHALGEN_X, ISHALGEN_Y, ISHALGEN_Z, LEVEL_10_EXP);
		actor.commonData->setPlayerClass(PlayerClass::GLADIATOR);
		hold(race == Race::ELYOS ? 1006 : 2008, QuestStatus::COMPLETE);
		ASSERT_TRUE(player().getCommonData()->updateDaeva());
		if (ceremonyVar)
			hold(race == Race::ELYOS ? 1007 : 2009, QuestStatus::START, *ceremonyVar);
		clearSent();
	}

	int64_t held(int32_t itemId) { return player().getInventory().getItemCountByItemId(itemId); }
};

/**
 * SETPRO3's table (_1007ACeremonyinSanctum.java:93-120, _2009ACeremonyinPandaemonium.java:103-130): a second class of each starting class,
 * the var and reward group it gets, and the reward npc of that var with its page (:129-169 / :137-177)
 */
struct CeremonyRow {
	PlayerClass playerClass;
	int32_t var;
	int32_t rewardGroup;
	int32_t elyosRewardNpc;
	int32_t asmodianRewardNpc;
	int32_t page;
};

constexpr CeremonyRow CEREMONY_ROWS[] = {{PlayerClass::GLADIATOR, 10, 0, 203758, 204080, 2034}, {PlayerClass::ASSASSIN, 20, 1, 203759, 204081, 2375},
	{PlayerClass::SORCERER, 30, 2, 203760, 204082, 2716}, {PlayerClass::CLERIC, 40, 3, 203761, 204083, 3057},
	{PlayerClass::GUNNER, 50, 4, 801212, 801220, 3398}, {PlayerClass::BARD, 60, 5, 801213, 801221, 3739}};

TEST_F(CeremonyTest, BothRegisterTheirNpcsWhileTheSimpleClassChangeIsOff) {
	registerHandlers();
	QuestEngine& engine = QuestEngine::getInstance();
	for (int32_t npcId : {PERNOS, LEAH, JUCLEAS, MACUS, 203759, 203760, 203761, 801212, 801213})
		EXPECT_TRUE(engine.getQuestNpc(npcId)->getOnTalkEvent().contains(1007)) << npcId;
	for (int32_t npcId : {MUNIN, HEIMDALL, BALDER, KALSTEN, 204081, 204082, 204083, 801220, 801221})
		EXPECT_TRUE(engine.getQuestNpc(npcId)->getOnTalkEvent().contains(2009)) << npcId;
}

TEST_F(CeremonyTest, ADaevaWhoseAscensionIsCompleteGetsTheCeremonyAtTheLevelChange) {
	daeva(Race::ELYOS, std::nullopt);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(1007), QuestStatus::START);
	EXPECT_TRUE(wasSent(questAdd(1007, START)));
}

TEST_F(CeremonyTest, AnAsmodianDaevaGets2009AtTheLevelChange) {
	daeva(Race::ASMODIANS, std::nullopt);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_EQ(statusOf(2009), QuestStatus::START);
	EXPECT_FALSE(statusOf(1007).has_value());
}

TEST_F(CeremonyTest, WithoutTheAscensionTheCeremonyIsNotStarted) {
	registerHandlers();
	spawnQuester(Race::ELYOS, 9, POETA, 1, POETA_X, POETA_Y, POETA_Z);
	hold(1006, QuestStatus::START, 3);
	QuestEngine::getInstance().onLevelChanged(player());
	EXPECT_FALSE(statusOf(1007).has_value());
}

// ---- 1007 ----------------------------------------------------------------------------------------------------------------------------------

TEST_F(CeremonyTest, PernosOpens1011AtVarZeroAnd1013AtVarOne) {
	daeva(Race::ELYOS, 0);
	Npc& pernos = npcBeside(PERNOS);
	EXPECT_TRUE(talk(pernos, 1007, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), 1011, 1007)));
	state(1007)->setQuestVar(1);
	EXPECT_TRUE(talk(pernos, 1007, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(pernos.getObjectId(), 1013, 1007)));
	state(1007)->setQuestVar(2);
	EXPECT_FALSE(talk(pernos, 1007, DA::QUEST_SELECT));
	EXPECT_FALSE(talk(pernos, 1007, DA::SETPRO1)) << "var <= 1 only";
	EXPECT_EQ(varOf(1007), 2) << "no var change at var 2 (the beam's exception would answer false as well)";
	EXPECT_TRUE(player().isSpawned()) << "no beam";
}

TEST_F(CeremonyTest, PernosSetpro1AtVarZeroSetsVarOneClosesTheWindowAndBeams) {
	daeva(Race::ELYOS, 0);
	Npc& pernos = npcBeside(PERNOS);
	talk(pernos, 1007, DA::SETPRO1); // the beam to Sanctum throws in this world (see the file's header)
	EXPECT_EQ(varOf(1007), 1);
	EXPECT_TRUE(wasSent(questUpdate(1007, START, 1)));
	EXPECT_TRUE(wasSent(dialogClosed(pernos.getObjectId()))) << ":64 SM_DIALOG_WINDOW(npc, 0)";
	EXPECT_FALSE(player().isSpawned()) << "TeleportService.sendLoc despawned him for the beam";
}

TEST_F(CeremonyTest, PernosSetpro1AtVarOneBeamsAgain) {
	daeva(Race::ELYOS, 1);
	Npc& pernos = npcBeside(PERNOS);
	talk(pernos, 1007, DA::SETPRO1);
	EXPECT_EQ(varOf(1007), 1);
	EXPECT_TRUE(wasSent(questUpdate(1007, START, 1))) << ":61 var <= 1";
	EXPECT_TRUE(wasSent(dialogClosed(pernos.getObjectId())));
	EXPECT_FALSE(player().isSpawned());
}

TEST_F(CeremonyTest, LeahPlaysMovie92AndSetpro2GoesToVarTwo) {
	daeva(Race::ELYOS, 1);
	Npc& leah = npcBeside(LEAH);
	EXPECT_TRUE(talk(leah, 1007, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(leah.getObjectId(), 1352, 1007)));
	EXPECT_FALSE(talk(leah, 1007, DA::SELECT2_1)) << "return playQuestMovie(env, 92): false";
	EXPECT_TRUE(wasSent(questMovie(leah.getObjectId(), 1007, 92)));
	EXPECT_TRUE(talk(leah, 1007, DA::SETPRO2));
	EXPECT_EQ(varOf(1007), 2);
	EXPECT_TRUE(wasSent(questUpdate(1007, START, 2)));
}

TEST_F(CeremonyTest, JucleasGivesAWarriorVarTenAndRewardGroupZeroAndMacusFinishesWithLevelTen) {
	daeva(Race::ELYOS, 2);
	Npc& jucleas = npcBeside(JUCLEAS);
	EXPECT_TRUE(talk(jucleas, 1007, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(jucleas.getObjectId(), 1693, 1007)));
	EXPECT_FALSE(talk(jucleas, 1007, DA::SELECT3_1));
	EXPECT_TRUE(wasSent(questMovie(jucleas.getObjectId(), 1007, 91)));
	EXPECT_TRUE(talk(jucleas, 1007, DA::SETPRO3));
	EXPECT_EQ(statusOf(1007), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1007), 10) << "a Gladiator's starting class is WARRIOR";
	EXPECT_EQ(state(1007)->getRewardGroup(), std::optional<int32_t>(0));
	// the reward npc of the other classes has nothing for him
	Npc& eumelos = npcBeside(203759);
	EXPECT_FALSE(talk(eumelos, 1007, DA::USE_OBJECT));
	Npc& macus = npcBeside(MACUS);
	clearSent();
	EXPECT_TRUE(talk(macus, 1007, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(dialogWindow(macus.getObjectId(), 2034, 1007)));
	EXPECT_TRUE(talk(macus, 1007, DA::SELECTED_QUEST_REWARD1));
	EXPECT_EQ(statusOf(1007), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getCommonData()->getExp(), LEVEL_10_EXP + 13125) << "quest_data.xml: <rewards exp=\"13125\">";
	EXPECT_EQ(player().getLevel(), 10) << "the ceremony's exp lifts the Daeva to level 10";
	EXPECT_EQ(held(TEA_OF_REPOSE), 5);
	EXPECT_EQ(player().getInventory().getKinah(), 250000);
	EXPECT_EQ(held(PROPHECY_SWORD), 1) << "the first fighter reward";
}

TEST_F(CeremonyTest, JucleasGivesEveryClassItsVarAndRewardGroupAndOnlyItsRewardNpcAnswers) {
	daeva(Race::ELYOS, 2);
	Npc& jucleas = npcBeside(JUCLEAS);
	std::vector<Npc*> rewardNpcs;
	for (const CeremonyRow& row : CEREMONY_ROWS)
		rewardNpcs.push_back(&npcBeside(row.elyosRewardNpc));
	for (const CeremonyRow& row : CEREMONY_ROWS) {
		actor.commonData->setPlayerClass(row.playerClass);
		state(1007)->setStatus(QuestStatus::START);
		state(1007)->setQuestVar(2);
		EXPECT_TRUE(talk(jucleas, 1007, DA::SETPRO3)) << row.var;
		EXPECT_EQ(statusOf(1007), QuestStatus::REWARD) << row.var;
		EXPECT_EQ(varOf(1007), row.var);
		EXPECT_EQ(state(1007)->getRewardGroup(), std::optional<int32_t>(row.rewardGroup)) << row.var;
		for (size_t i = 0; i < std::size(CEREMONY_ROWS); i++) {
			const bool own = CEREMONY_ROWS[i].var == row.var;
			clearSent();
			EXPECT_EQ(talk(*rewardNpcs[i], 1007, DA::USE_OBJECT), own) << row.var << " at " << CEREMONY_ROWS[i].elyosRewardNpc;
			if (own)
				EXPECT_TRUE(wasSent(dialogWindow(rewardNpcs[i]->getObjectId(), row.page, 1007))) << row.var;
		}
	}
}

// ---- 2009 ----------------------------------------------------------------------------------------------------------------------------------

TEST_F(CeremonyTest, MuninSetpro1AtVarZeroSetsVarOneClosesTheWindowAndBeams) {
	daeva(Race::ASMODIANS, 0);
	Npc& munin = npcBeside(MUNIN);
	talk(munin, 2009, DA::SETPRO1); // the beam to Pandaemonium throws in this world (see the file's header)
	EXPECT_EQ(varOf(2009), 1);
	EXPECT_TRUE(wasSent(questUpdate(2009, START, 1)));
	EXPECT_TRUE(wasSent(dialogClosed(munin.getObjectId()))) << ":70 SM_DIALOG_WINDOW(npc, 0)";
	EXPECT_FALSE(player().isSpawned());
}

TEST_F(CeremonyTest, MuninSetpro1AtVarOneBeamsAgainAndNotAtVarTwo) {
	daeva(Race::ASMODIANS, 1);
	Npc& munin = npcBeside(MUNIN);
	talk(munin, 2009, DA::SETPRO1);
	EXPECT_EQ(varOf(2009), 1);
	EXPECT_TRUE(wasSent(questUpdate(2009, START, 1))) << ":67 var <= 1";
	EXPECT_TRUE(wasSent(dialogClosed(munin.getObjectId())));
	EXPECT_FALSE(player().isSpawned());
}

TEST_F(CeremonyTest, MuninSetpro1AtVarTwoDoesNothing) {
	daeva(Race::ASMODIANS, 2);
	Npc& munin = npcBeside(MUNIN);
	EXPECT_FALSE(talk(munin, 2009, DA::SETPRO1));
	EXPECT_EQ(varOf(2009), 2);
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(CeremonyTest, BalderGivesEveryClassItsVarAndRewardGroupAndOnlyItsRewardNpcAnswers) {
	daeva(Race::ASMODIANS, 2);
	Npc& balder = npcBeside(BALDER);
	std::vector<Npc*> rewardNpcs;
	for (const CeremonyRow& row : CEREMONY_ROWS)
		rewardNpcs.push_back(&npcBeside(row.asmodianRewardNpc));
	for (const CeremonyRow& row : CEREMONY_ROWS) {
		actor.commonData->setPlayerClass(row.playerClass);
		state(2009)->setStatus(QuestStatus::START);
		state(2009)->setQuestVar(2);
		EXPECT_TRUE(talk(balder, 2009, DA::SETPRO3)) << row.var;
		EXPECT_EQ(statusOf(2009), QuestStatus::REWARD) << row.var;
		EXPECT_EQ(varOf(2009), row.var);
		EXPECT_EQ(state(2009)->getRewardGroup(), std::optional<int32_t>(row.rewardGroup)) << row.var;
		for (size_t i = 0; i < std::size(CEREMONY_ROWS); i++) {
			const bool own = CEREMONY_ROWS[i].var == row.var;
			clearSent();
			EXPECT_EQ(talk(*rewardNpcs[i], 2009, DA::USE_OBJECT), own) << row.var << " at " << CEREMONY_ROWS[i].asmodianRewardNpc;
			if (own)
				EXPECT_TRUE(wasSent(dialogWindow(rewardNpcs[i]->getObjectId(), row.page, 2009))) << row.var;
		}
	}
}

TEST_F(CeremonyTest, The2009ChainMuninHeimdallBalderKalsten) {
	daeva(Race::ASMODIANS, 0);
	Npc& munin = npcBeside(MUNIN);
	EXPECT_TRUE(talk(munin, 2009, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 1011, 2009)));
	state(2009)->setQuestVar(1);
	EXPECT_TRUE(talk(munin, 2009, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(munin.getObjectId(), 1013, 2009)));
	Npc& heimdall = npcBeside(HEIMDALL);
	EXPECT_TRUE(talk(heimdall, 2009, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(heimdall.getObjectId(), 1352, 2009)));
	EXPECT_FALSE(talk(heimdall, 2009, DA::SELECT2_1));
	EXPECT_TRUE(wasSent(questMovie(heimdall.getObjectId(), 2009, 121)));
	EXPECT_TRUE(talk(heimdall, 2009, DA::SETPRO2));
	EXPECT_EQ(varOf(2009), 2);
	Npc& balder = npcBeside(BALDER);
	EXPECT_TRUE(talk(balder, 2009, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(balder.getObjectId(), 1693, 2009)));
	EXPECT_FALSE(talk(balder, 2009, DA::SELECT3_1));
	EXPECT_TRUE(wasSent(questMovie(balder.getObjectId(), 2009, 122)));
	EXPECT_TRUE(talk(balder, 2009, DA::SETPRO3));
	EXPECT_EQ(statusOf(2009), QuestStatus::REWARD);
	EXPECT_EQ(varOf(2009), 10);
	Npc& kalsten = npcBeside(KALSTEN);
	EXPECT_TRUE(talk(kalsten, 2009, DA::USE_OBJECT));
	EXPECT_TRUE(wasSent(dialogWindow(kalsten.getObjectId(), 2034, 2009)));
	EXPECT_TRUE(talk(kalsten, 2009, DA::SELECTED_QUEST_REWARD1));
	EXPECT_EQ(statusOf(2009), QuestStatus::COMPLETE);
	EXPECT_EQ(player().getLevel(), 10);
	EXPECT_EQ(held(KARMIC_SWORD), 1);
}

TEST_F(CeremonyTest, HeimdallsMovieNeedsVarOne) {
	daeva(Race::ASMODIANS, 2);
	Npc& heimdall = npcBeside(HEIMDALL);
	clearSent();
	EXPECT_FALSE(talk(heimdall, 2009, DA::SELECT2_1));
	EXPECT_FALSE(sentOpcode(SM_PLAY_MOVIE_OPCODE));
}

} // namespace
} // namespace aion::gameserver::handlers::quest::ascension::test
