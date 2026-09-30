// Q03's own cases of generated handlers (the review of 2026-09-29, docs/deviations/Q03.md "Tests"): what the golden harness cannot trace.
//
// - The gate impact of the chunk: gs.scenario.travel's Elyos Daeva (level 10, 1006 COMPLETE) arrives in Verteron, CM_LEVEL_READY runs
//   QuestEngine.onEnterWorld and PlayerController.updateNearbyQuests (CM_LEVEL_READY.java:93, :88); no handler of the chunk in the tree may start
//   a quest for him there or put its quest into his SM_NEARBY_QUESTS. That is what holds 14010 (its enter-world start) and 1131, 1146 and 1152
//   (their start npcs' markers, min level 11-12) back (Q03Handlers.h).
// - Hooks the oracle refuses (tools/oracle/questtrace: `unsupported`), written from their Java statement by statement: 14050's enter-world and
//   level-change start (WorldMapType.HEIRON), all of 1640 (TeleportService.teleportTo) and 1647 (player.getEquipment,
//   spawnForFiveMinutesInFrontOf), 14016's onDieEvent (qs.getQuestVars().getQuestVars()) and 1197's dialog (the instanceof pattern).
//
// The rows are the real ones (Q03QuestTestSupport.h realStaticRows): the quests, their reward and work items, the npcs the cases spawn and the
// whole tribe relation table; the World is the fixture's (Verteron, Heiron, Steel Rake, Reshanta).

#include "Q03Handlers.h"
#include "Q03QuestTestSupport.h"

#include <cmath>
#include <fstream>
#include <regex>
#include <set>

#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"
#include "aion/gameserver/services/QuestService.h"

namespace aion::gameserver::questEngine::handlers::q03::test {
namespace {

namespace DA = gameserver::model::DialogAction;
using network::aion::serverpackets::SM_PLAY_MOVIE;
using network::aion::serverpackets::SM_TELEPORT_LOC;

/** The factory of the chunk's handler of the quest (Q03Handlers.h) */
std::unique_ptr<AbstractQuestHandler> handlerOf(int32_t questId) {
	for (const Q03Handler& entry : q03Handlers()) {
		if (entry.questId == questId)
			return entry.factory();
	}
	throw std::runtime_error("no handler of the chunk for quest " + std::to_string(questId));
}

class GeneratedHookTest : public Q03QuestTest {
protected:
	/**
	 * The fixture's quest, npc, item and tribe rows replaced by the real ones: the quests (their reward, collect and work items come along),
	 * the npcs and the extra items; kinah always (a character holds it). The fixture's TearDown resets the holders.
	 */
	void useRows(const std::set<int32_t>& quests, const std::set<int32_t>& npcIds, std::set<int32_t> items) {
		std::string questRows = realStaticRows("quest_data/quest_data.xml", "quests", "quest", "id", quests);
		std::set<int32_t> named = itemIdsIn(questRows);
		items.insert(named.begin(), named.end());
		items.insert(182400001); // kinah
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), questRows));
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(),
			realStaticRows("items/item_templates.xml", "item_templates", "item_template", "id", items)));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(),
			realStaticRows("npcs/npc_templates.xml", "npc_templates", "npc_template", "npc_id", npcIds)));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(contexts.emplace_back(), wholeFile("tribe/tribe_relations.xml")));
	}

	static std::string wholeFile(const char* file) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data" / file, std::ios::binary);
		if (!in)
			throw std::runtime_error(std::string("cannot read ") + file);
		std::ostringstream out;
		out << in.rdbuf();
		std::string text = out.str();
		return text.substr(text.find("?>") + 2); // no XML declaration inside bindString's document
	}

	bool talk(Ptr<gameserver::model::gameobjects::VisibleObject> target, int32_t questId, int32_t action) {
		clearSent();
		return dialog(target, questId, action);
	}

	bool talk(Npc& npc, int32_t questId, int32_t action) { return talk(Ptr<gameserver::model::gameobjects::VisibleObject>(npc), questId, action); }

	/** Any SM_QUEST_ACTION sent since the last clearSent (the opcode header of one) */
	bool questActionSent() { return wasSentClass(questUpdate(0, 0, 0)); }

	static std::vector<uint8_t> window(int32_t objectId, int32_t page) { return dialogWindow(objectId, page, 0); }
};

// ---- the travel gate's Elyos in Verteron -----------------------------------------------------------------------------------------------------

/** gs.scenario.travel's T1 arrival (TravelScenarioTest.cpp elyosRoute: teleport_location.xml:5) */
constexpr float ARRIVAL_X = 1640.76f;
constexpr float ARRIVAL_Y = 1500.32f;
constexpr float ARRIVAL_Z = 119.70999f;

/** The npc ids spawns/Npcs/210030000_Verteron.xml spawns (WorldMapInstance.addObject collects their quest starts into the map's quest ids) */
std::set<int32_t> verteronNpcIds() {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data/spawns/Npcs/210030000_Verteron.xml", std::ios::binary);
	std::ostringstream out;
	out << in.rdbuf();
	const std::string text = out.str();
	static const std::regex npcId(R"x(npc_id="(\d+)")x");
	std::set<int32_t> ids;
	for (std::sregex_iterator it(text.begin(), text.end(), npcId), end; it != end; ++it)
		ids.insert(std::stoi((*it)[1].str()));
	return ids;
}

class TravelArrivalTest : public GeneratedHookTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (IsSkipped())
			return;
		std::set<int32_t> quests{1006};
		for (const Q03Handler& entry : q03Handlers())
			quests.insert(entry.questId);
		for (const Q03HeldBack& held : Q03_HELD_BACK)
			quests.insert(held.questId);
		useRows(quests, {}, {});
		for (const Q03Handler& entry : q03Handlers())
			QuestEngine::getInstance().addQuestHandler(entry.factory());
		// the gate's seed: a level-10 Gladiator (TravelScenarioTest.cpp DAEVA_CLASS) with 1006 COMPLETE, here at T1's arrival
		spawnActor(gameserver::model::Race::ELYOS, 10, VERTERON, ARRIVAL_X, ARRIVAL_Y, ARRIVAL_Z);
		actor.commonData->setPlayerClass(gameserver::model::PlayerClass::GLADIATOR);
		hold(1006, QuestStatus::COMPLETE);
	}

	/** Whether the quest id is one of the chunk's (in the tree or held back) */
	static bool ofTheChunk(int32_t questId) {
		for (const Q03Handler& entry : q03Handlers())
			if (entry.questId == questId)
				return true;
		for (const Q03HeldBack& held : Q03_HELD_BACK)
			if (held.questId == questId)
				return true;
		return false;
	}
};

TEST_F(TravelArrivalTest, HisEnterWorldInVerteronStartsNoQuestOfTheChunk) {
	// CM_LEVEL_READY.java:93 on every map arrival: QuestEngine.onEnterWorld runs every registered onEnterWorldEvent (before the review, 14010's
	// `player.getWorldId() == VERTERON && !hasQuest -> QuestService.startQuest` started it here: the held-back handler)
	ASSERT_EQ(player().getWorldId(), VERTERON);
	clearSent();
	QuestEngine::getInstance().onEnterWorld(player());
	for (const Q03Handler& entry : q03Handlers())
		EXPECT_FALSE(stateOf(entry.questId)) << entry.javaClass << " started at the arrival";
	for (const Q03HeldBack& held : Q03_HELD_BACK)
		EXPECT_FALSE(stateOf(held.questId)) << held.javaClass << " started at the arrival";
	EXPECT_FALSE(questActionSent()) << "an SM_QUEST_ACTION at the arrival";
}

TEST_F(TravelArrivalTest, NoQuestOfTheChunkIsInHisNearbyQuestsInVerteron) {
	// PlayerController.updateNearbyQuests (CM_LEVEL_READY.java:88): the map's quest ids are the onQuestStart registrations of its spawned npcs
	// (WorldMapInstance.addObject), each shown when checkStartConditions(player, questId, false, 2, false, false, false) holds (a grey marker up
	// to two levels below the quest's minimum). Before the review 1131 (min 11), 1146 and 1152 (min 12) were in it
	int32_t chunkStarts = 0;
	int32_t reachable = 0;
	for (int32_t npcId : verteronNpcIds()) {
		for (int32_t questId : QuestEngine::getInstance().getQuestNpc(npcId)->getOnQuestStart()) {
			if (!ofTheChunk(questId))
				continue;
			chunkStarts++;
			EXPECT_FALSE(services::QuestService::checkStartConditions(player(), questId, false, 2, false, false, false))
				<< questId << " (start npc " << npcId << ") would be in his SM_NEARBY_QUESTS";
			// the check is not false for every quest of the chunk: with a wider level allowance the prerequisite-free ones hold
			if (services::QuestService::checkStartConditions(player(), questId, false, 10, false, false, false))
				reachable++;
		}
	}
	EXPECT_GT(chunkStarts, 10) << "the Verteron start npcs of the chunk (1141, 1149, 1156, ...)";
	EXPECT_GT(reachable, 0) << "a level allowance of 10 shows some of them: the check reads the quests";
}

// ---- 14050 Orders from Heiron Fortress: onEnterWorldEvent, onLevelChangedEvent (_14050OrdersFromHeironFortress.java:55-67) ----------------

class OrdersFromHeironFortressTest : public GeneratedHookTest {
protected:
	void setUpAt(int32_t mapId, int32_t level) {
		useRows({14050}, {}, {});
		install(handlerOf(14050));
		spawnActor(gameserver::model::Race::ELYOS, level, mapId, 1500.0f, 2700.0f, 130.0f);
	}

	bool enterWorldHook() {
		Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 14050);
		envs.push_back(env);
		return handler->onEnterWorldEvent(*env);
	}
};

TEST_F(OrdersFromHeironFortressTest, AnEnterWorldInHeironStartsTheMission) {
	setUpAt(HEIRON, 36); // minlevel_permitted 36 (quest_data.xml:42219)
	clearSent();
	QuestEngine::getInstance().onEnterWorld(player());
	ASSERT_TRUE(stateOf(14050)) << "`player.getWorldId() == WorldMapType.HEIRON.getId() && !hasQuest(questId)`: QuestService.startQuest";
	EXPECT_EQ(stateOf(14050)->getStatus(), QuestStatus::START);
	EXPECT_EQ(varOf(14050), 0);
	EXPECT_TRUE(questActionSent());
	clearSent();
	EXPECT_FALSE(enterWorldHook()) << "hasQuest: nothing more";
	EXPECT_FALSE(questActionSent());
	EXPECT_EQ(stateOf(14050)->getStatus(), QuestStatus::START);
}

TEST_F(OrdersFromHeironFortressTest, TheHookAnswersTheStart) {
	setUpAt(HEIRON, 36);
	EXPECT_TRUE(enterWorldHook()) << "return QuestService.startQuest(env)";
	ASSERT_TRUE(stateOf(14050));
}

TEST_F(OrdersFromHeironFortressTest, OutsideHeironNothingStarts) {
	setUpAt(VERTERON, 36);
	clearSent();
	QuestEngine::getInstance().onEnterWorld(player());
	EXPECT_FALSE(stateOf(14050));
	EXPECT_FALSE(enterWorldHook());
	EXPECT_FALSE(questActionSent());
}

TEST_F(OrdersFromHeironFortressTest, BelowItsLevelTheStartFails) {
	setUpAt(HEIRON, 35);
	EXPECT_FALSE(enterWorldHook()) << "startQuest's checkStartConditions: minlevel 36";
	EXPECT_FALSE(stateOf(14050));
}

TEST_F(OrdersFromHeironFortressTest, ALevelChangeInHeironStartsItThroughTheEnterWorldHook) {
	setUpAt(HEIRON, 36);
	QuestEngine::getInstance().onLevelChanged(player()); // onLevelChangedEvent: onEnterWorldEvent(new QuestEnv(null, player, questId))
	ASSERT_TRUE(stateOf(14050));
	EXPECT_EQ(stateOf(14050)->getStatus(), QuestStatus::START);
}

// ---- 1640 Teleporter Repairs: onDialogEvent (_1640TeleporterRepairs.java:30-66) -------------------------------------------------------------

constexpr int32_t DESTROYED_TELEPORT_STATUE = 730033;
constexpr int32_t TELEPORTER_PART = 182201790; // collect_item of 1640 (quest_data.xml:5688)

class TeleporterRepairsTest : public GeneratedHookTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (IsSkipped())
			return;
		useRows({1640}, {DESTROYED_TELEPORT_STATUE}, {TELEPORTER_PART});
		install(handlerOf(1640));
		spawnActor(gameserver::model::Race::ELYOS, 43, HEIRON, 1500.0f, 2700.0f, 130.0f); // minlevel 43
	}
};

TEST_F(TeleporterRepairsTest, TheStatueOffersTheQuestAndSetpro1StartsItAndClosesTheWindow) {
	Npc& statue = spawnNpc(DESTROYED_TELEPORT_STATUE);
	EXPECT_TRUE(talk(statue, 1640, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(statue.getObjectId(), 1011, 1640)));
	EXPECT_FALSE(stateOf(1640));
	EXPECT_TRUE(talk(statue, 1640, DA::SETPRO1)) << "QuestService.startQuest(env); return closeDialogWindow(env)";
	ASSERT_TRUE(stateOf(1640));
	EXPECT_EQ(stateOf(1640)->getStatus(), QuestStatus::START);
	EXPECT_TRUE(wasSent(window(statue.getObjectId(), 0)));
}

TEST_F(TeleporterRepairsTest, AnyOtherActionIsTheStartDialog) {
	Npc& statue = spawnNpc(DESTROYED_TELEPORT_STATUE);
	EXPECT_TRUE(talk(statue, 1640, DA::QUEST_ACCEPT_1)) << "sendQuestStartDialog(env)";
	ASSERT_TRUE(stateOf(1640));
	EXPECT_EQ(stateOf(1640)->getStatus(), QuestStatus::START);
	EXPECT_TRUE(wasSent(dialogWindow(statue.getObjectId(), 1003, 1640)));
}

TEST_F(TeleporterRepairsTest, InStartTheStatueAsksForThePart) {
	hold(1640, QuestStatus::START);
	Npc& statue = spawnNpc(DESTROYED_TELEPORT_STATUE);
	EXPECT_TRUE(talk(statue, 1640, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(statue.getObjectId(), 1352, 1640)));
	EXPECT_TRUE(talk(statue, 1640, DA::SETPRO2)) << "no part: sendQuestDialog(env, 1353)";
	EXPECT_TRUE(wasSent(dialogWindow(statue.getObjectId(), 1353, 1640)));
	EXPECT_EQ(stateOf(1640)->getStatus(), QuestStatus::START);
	EXPECT_FALSE(talk(statue, 1640, DA::SETPRO1)) << "no such case in START";
}

TEST_F(TeleporterRepairsTest, WithThePartSetpro2FinishesTheQuestAndBeamsThePlayerAway) {
	hold(1640, QuestStatus::START);
	give(TELEPORTER_PART, 2);
	Npc& statue = spawnNpc(DESTROYED_TELEPORT_STATUE);
	int32_t instanceId = player().getInstanceId();
	int64_t expBefore = player().getCommonData()->getExp();
	EXPECT_TRUE(talk(statue, 1640, DA::SETPRO2));
	EXPECT_EQ(held(TELEPORTER_PART), 1) << "removeQuestItem(env, 182201790, 1)";
	EXPECT_EQ(stateOf(1640)->getStatus(), QuestStatus::COMPLETE) << "qs.setStatus(REWARD); QuestService.finishQuest(env)";
	EXPECT_EQ(stateOf(1640)->getCompleteCount(), 1);
	EXPECT_EQ(player().getCommonData()->getExp() - expBefore, 492677) << "quest_data.xml:5690";
	// TeleportService.teleportTo(player, HEIRON, 187.71689f, 2712.14870f, 141.91672f, (byte) 195, FADE_OUT_BEAM): the animation's SM_TELEPORT_LOC
	// (the spawn waits for CM_TELEPORT_ANIMATION_DONE)
	EXPECT_TRUE(wasSent(serializedFor(SM_TELEPORT_LOC(HEIRON, instanceId, 187.71689f, 2712.14870f, 141.91672f, static_cast<int8_t>(195),
		gameserver::model::animations::TeleportAnimation::FADE_OUT_BEAM))));
	EXPECT_TRUE(wasSent(window(statue.getObjectId(), 0))) << "closeDialogWindow";
}

// ---- 1647 Dressing Up For Bollvig: onDialogEvent, onMovieEndEvent (_1647DressingUpForBollvig.java:27-81) ----------------------------------

constexpr int32_t ZETUS = 790019;
constexpr int32_t SUSPICIOUS_STONE_STATUE = 700272;
constexpr int32_t GRAVEKNIGHT = 204635;
constexpr int32_t MYANEES_FLUTE = 182201783; // 1647's work item (quest_data.xml:5761)
constexpr int32_t STENON_BLOUSE = 110100150;
constexpr int32_t STENON_SKIRT = 113100144;

class DressingUpForBollvigTest : public GeneratedHookTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (IsSkipped())
			return;
		useRows({1647}, {ZETUS, SUSPICIOUS_STONE_STATUE, GRAVEKNIGHT}, {MYANEES_FLUTE, STENON_BLOUSE, STENON_SKIRT});
		install(handlerOf(1647));
		spawnActor(gameserver::model::Race::ELYOS, 42, HEIRON, 1500.0f, 2700.0f, 130.0f); // minlevel 42
	}

	/** The Stenon clothes worn, loaded the way PlayerService.loadPlayer loads equipped items (Equipment.onLoadHandler; cloth needs skill 40) */
	void wearTheStenonClothes() {
		player().setSkillList(gameserver::model::skill::PlayerSkillList::create(
			{gameserver::model::skill::PlayerSkillEntry::create(40, 1, 0, gameserver::model::gameobjects::Persistable_PersistentState::UPDATED)}));
		int32_t objId = 420501;
		for (auto [itemId, slot] : {std::pair{STENON_BLOUSE, gameserver::model::items::ItemSlot::TORSO}, std::pair{STENON_SKIRT, gameserver::model::items::ItemSlot::PANTS}}) {
			Ref<gameserver::model::gameobjects::Item> item = gameserver::model::gameobjects::Item::create(objId++,
				dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId), 1, true, gameserver::model::items::getSlotIdMask(slot));
			player().getEquipment().onLoadHandler(*item);
			ASSERT_FALSE(player().getEquipment().getEquippedItemsByItemId(itemId).empty()) << itemId;
		}
	}
};

TEST_F(DressingUpForBollvigTest, ZetusOffersTheQuestAndTheAcceptGivesTheFlute) {
	Npc& zetus = spawnNpc(ZETUS);
	EXPECT_TRUE(talk(zetus, 1647, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(zetus.getObjectId(), 4762, 1647)));
	EXPECT_TRUE(talk(zetus, 1647, DA::QUEST_ACCEPT_1)) << "default: sendQuestStartDialog(env, 182201783, 1)";
	ASSERT_TRUE(stateOf(1647));
	EXPECT_EQ(stateOf(1647)->getStatus(), QuestStatus::START);
	EXPECT_EQ(held(MYANEES_FLUTE), 1);
	EXPECT_TRUE(wasSent(dialogWindow(zetus.getObjectId(), 1003, 1647)));
}

TEST_F(DressingUpForBollvigTest, TheStatueWantsTheStenonClothesAndTheFlute) {
	hold(1647, QuestStatus::START);
	Npc& statue = spawnNpc(SUSPICIOUS_STONE_STATUE);
	give(MYANEES_FLUTE, 1);
	EXPECT_FALSE(talk(statue, 1647, DA::USE_OBJECT)) << "no clothes";
	wearTheStenonClothes();
	EXPECT_FALSE(talk(statue, 1647, DA::QUEST_SELECT)) << "USE_OBJECT only";
	player().getInventory().decreaseByItemId(MYANEES_FLUTE, 1);
	EXPECT_FALSE(talk(statue, 1647, DA::USE_OBJECT)) << "no flute";
	EXPECT_EQ(stateOf(1647)->getStatus(), QuestStatus::START);
	give(MYANEES_FLUTE, 1);
	player().setTarget(Ptr<gameserver::model::gameobjects::VisibleObject>(statue));
	EXPECT_TRUE(talk(statue, 1647, DA::USE_OBJECT)) << "playQuestMovie(env, 199); return useQuestObject(env, 0, 0, true, false)";
	EXPECT_TRUE(wasSent(serializedFor(SM_PLAY_MOVIE(false, statue.getObjectId(), 1647, 199, true))));
	EXPECT_EQ(stateOf(1647)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1647), 0);
	EXPECT_TRUE(statue.isSpawned()) << "dieObject false";
	player().setTarget(nullptr);
}

TEST_F(DressingUpForBollvigTest, ZetusShowsTheRewardInReward) {
	hold(1647, QuestStatus::REWARD);
	Npc& zetus = spawnNpc(ZETUS);
	Npc& statue = spawnNpc(SUSPICIOUS_STONE_STATUE, 4.0f);
	EXPECT_FALSE(talk(statue, 1647, DA::USE_OBJECT)) << "REWARD: Zetus only";
	EXPECT_TRUE(talk(zetus, 1647, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(zetus.getObjectId(), 10002, 1647)));
	EXPECT_TRUE(talk(zetus, 1647, DA::SELECT_QUEST_REWARD)) << "default: sendQuestEndDialog(env), the reward page";
	EXPECT_TRUE(wasSent(dialogWindow(zetus.getObjectId(), 5, 1647)));
}

TEST_F(DressingUpForBollvigTest, Movie199SpawnsThreeGraveknightsAroundThePlayer) {
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 1647);
	envs.push_back(env);
	handler->onMovieEndEvent(*env, 198);
	EXPECT_TRUE(player().getWorldMapInstance()->getNpcs({GRAVEKNIGHT}).empty()) << "another movie: nothing";
	float x = player().getX(), y = player().getY(), z = player().getZ();
	handler->onMovieEndEvent(*env, 199);
	std::vector<Ptr<Npc>> knights = player().getWorldMapInstance()->getNpcs({GRAVEKNIGHT});
	for (const Ptr<Npc>& knight : knights)
		spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(knight));
	ASSERT_EQ(knights.size(), 3u) << "spawnForFiveMinutesInFrontOf and two spawnForFiveMinutes";
	int32_t atPlusMinus = 0, atMinusPlus = 0, inFront = 0;
	for (const Ptr<Npc>& knight : knights) {
		if (knight->getX() == x + 2 && knight->getY() == y - 2 && knight->getZ() == z)
			atPlusMinus++;
		else if (knight->getX() == x - 2 && knight->getY() == y + 2 && knight->getZ() == z)
			atMinusPlus++;
		else if (std::abs(std::hypot(knight->getX() - x, knight->getY() - y) - 2.0f) < 0.01f)
			inFront++;
	}
	EXPECT_EQ(atPlusMinus, 1) << "(x + 2, y - 2, z)";
	EXPECT_EQ(atMinusPlus, 1) << "(x - 2, y + 2, z)";
	EXPECT_EQ(inFront, 1) << "2 m in front of the player";
	executor().advance(std::chrono::minutes(5));
	for (const Ptr<Npc>& knight : knights)
		EXPECT_FALSE(knight->isSpawned()) << "five minutes";
}

// ---- 14016 A Gate Agape: onDieEvent (_14016AGateAgape.java:78-91) ---------------------------------------------------------------------------

constexpr int32_t DIMENSIONAL_KEY = 182215317; // 14016's collect item (quest_data.xml:41938)

class GateAgapeTest : public GeneratedHookTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (IsSkipped())
			return;
		useRows({14016}, {}, {DIMENSIONAL_KEY});
		install(handlerOf(14016));
		spawnActor(gameserver::model::Race::ELYOS, 20, VERTERON, ARRIVAL_X, ARRIVAL_Y, ARRIVAL_Z);
	}

	bool die() {
		Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0);
		envs.push_back(env);
		QuestEngine::getInstance().onDie(*env);
		Ref<QuestEnv> own = QuestEnv::create(nullptr, player(), 14016);
		envs.push_back(own);
		return handler->onDieEvent(*own);
	}
};

TEST_F(GateAgapeTest, DyingAtStep2TakesTheQuestBackToStep1AndOneKey) {
	hold(14016, QuestStatus::START, 2);
	give(DIMENSIONAL_KEY, 3);
	clearSent();
	Ref<QuestEnv> env = QuestEnv::create(nullptr, player(), 0);
	envs.push_back(env);
	QuestEngine::getInstance().onDie(*env); // registerOnDie
	EXPECT_EQ(varOf(14016), 1) << "changeQuestStep(env, 2, 1)";
	EXPECT_EQ(held(DIMENSIONAL_KEY), 2) << "removeQuestItem(env, 182215317, 1)";
	EXPECT_TRUE(wasSent(questUpdate(14016, START, 1)));
}

TEST_F(GateAgapeTest, TheHookAnswersTrueAtStep2Only) {
	EXPECT_FALSE(die()) << "no quest";
	Ref<QuestState> qs = hold(14016, QuestStatus::START, 3);
	give(DIMENSIONAL_KEY, 1);
	EXPECT_FALSE(die()) << "var 3";
	EXPECT_EQ(varOf(14016), 3);
	EXPECT_EQ(held(DIMENSIONAL_KEY), 1);
	qs->setStatus(QuestStatus::REWARD);
	qs->setQuestVar(2);
	EXPECT_FALSE(die()) << "REWARD";
	EXPECT_EQ(varOf(14016), 2);
	qs->setStatus(QuestStatus::START);
	Ref<QuestEnv> own = QuestEnv::create(nullptr, player(), 14016);
	envs.push_back(own);
	EXPECT_TRUE(handler->onDieEvent(*own));
	EXPECT_EQ(varOf(14016), 1);
	EXPECT_EQ(held(DIMENSIONAL_KEY), 0);
}

// ---- 1197 Krall Book: onDialogEvent (_1197KrallBook.java:31-62) ----------------------------------------------------------------------------

constexpr int32_t ORDER_OF_THE_DUKAKI_TRIBE = 700004;
constexpr int32_t LETO = 203129;
constexpr int32_t KRALL_BOOK = 182200558; // 1197's work item and quest item (quest_data.xml:1678)

class KrallBookTest : public GeneratedHookTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (IsSkipped())
			return;
		useRows({1197}, {ORDER_OF_THE_DUKAKI_TRIBE, LETO}, {KRALL_BOOK});
		install(handlerOf(1197));
		spawnActor(gameserver::model::Race::ELYOS, 14, VERTERON, ARRIVAL_X, ARRIVAL_Y, ARRIVAL_Z); // minlevel 14
	}
};

TEST_F(KrallBookTest, TheOrderGivesTheBookOnceAndGoesAway) {
	Npc& order = spawnNpc(ORDER_OF_THE_DUKAKI_TRIBE);
	EXPECT_TRUE(talk(order, 1197, DA::USE_OBJECT));
	EXPECT_EQ(held(KRALL_BOOK), 1) << "giveQuestItem(env, 182200558, 1)";
	EXPECT_FALSE(order.isSpawned()) << "npc.getController().deleteAndScheduleRespawn()";
	Npc& another = spawnNpc(ORDER_OF_THE_DUKAKI_TRIBE, 4.0f);
	EXPECT_TRUE(talk(another, 1197, DA::USE_OBJECT)) << "any talk at the order answers true";
	EXPECT_EQ(held(KRALL_BOOK), 1) << "the book is held: nothing given";
	EXPECT_TRUE(another.isSpawned());
}

TEST_F(KrallBookTest, WithTheQuestTheOrderGivesNothing) {
	hold(1197, QuestStatus::START);
	Npc& order = spawnNpc(ORDER_OF_THE_DUKAKI_TRIBE);
	EXPECT_TRUE(talk(order, 1197, DA::USE_OBJECT));
	EXPECT_EQ(held(KRALL_BOOK), 0) << "not startable";
	EXPECT_TRUE(order.isSpawned());
}

TEST_F(KrallBookTest, TheBooksAcceptWithoutAnNpcStartsTheQuest) {
	// the book's page 4 (onItemUseEvent) is answered without a target: `!(env.getVisibleObject() instanceof Npc npc)` and QUEST_ACCEPT_1
	EXPECT_FALSE(talk(nullptr, 1197, DA::QUEST_SELECT)) << "not QUEST_ACCEPT_1";
	EXPECT_FALSE(stateOf(1197));
	EXPECT_TRUE(talk(nullptr, 1197, DA::QUEST_ACCEPT_1));
	ASSERT_TRUE(stateOf(1197));
	EXPECT_EQ(stateOf(1197)->getStatus(), QuestStatus::START);
	EXPECT_TRUE(wasSent(window(0, 0))) << "new SM_DIALOG_WINDOW(0, 0)";
}

TEST_F(KrallBookTest, LetoTakesOneBookAndSetsTheReward) {
	Npc& leto = spawnNpc(LETO);
	EXPECT_FALSE(talk(leto, 1197, DA::QUEST_SELECT)) << "no quest";
	Ref<QuestState> qs = hold(1197, QuestStatus::START);
	EXPECT_TRUE(talk(leto, 1197, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(leto.getObjectId(), 2375, 1197)));
	give(KRALL_BOOK, 2);
	EXPECT_TRUE(talk(leto, 1197, DA::SELECT_QUEST_REWARD));
	EXPECT_EQ(held(KRALL_BOOK), 1) << "removeQuestItem(env, 182200558, 1)";
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(varOf(1197), 1) << "qs.setQuestVar(1)";
	EXPECT_TRUE(wasSent(questUpdate(1197, REWARD, 1)));
	EXPECT_TRUE(wasSent(dialogWindow(leto.getObjectId(), 5, 1197))) << "sendQuestEndDialog: the reward page";
}

// ---- 1612 Lepharist Secrets: the object-use steps (_1612LepharistSecrets.java:57-72) -------------------------------------------------------

constexpr int32_t LEPHARIST_RESEARCH_DATA = 700352;

class LepharistSecretsTest : public GeneratedHookTest {
protected:
	void SetUp() override {
		Q03QuestTest::SetUp();
		if (IsSkipped())
			return;
		useRows({1612}, {LEPHARIST_RESEARCH_DATA}, {});
		install(handlerOf(1612));
		spawnActor(gameserver::model::Race::ELYOS, 39, HEIRON, 1500.0f, 2700.0f, 130.0f); // minlevel 39
	}

	/** USE_OBJECT at a fresh research data object the player targets (useQuestObject's dieObject kills the target) */
	bool useData(Npc*& used) {
		Npc& data = spawnNpc(LEPHARIST_RESEARCH_DATA);
		used = &data;
		player().setTarget(Ptr<gameserver::model::gameobjects::VisibleObject>(data));
		bool answer = talk(data, 1612, DA::USE_OBJECT);
		player().setTarget(nullptr);
		return answer;
	}
};

TEST_F(LepharistSecretsTest, EachResearchDataTakesOneStepAndDiesAndTheFourthSetsTheReward) {
	// the golden replay does not model dieObject (knownNotReproducible 1612 onDialogEvent#8-#11): useQuestObject(env, var, var + 1, false, true)
	// at vars 0-2 and useQuestObject(env, 3, 3, true, true) at var 3, each killing the target (AbstractQuestHandler.java:911-916)
	hold(1612, QuestStatus::START, 0);
	for (int32_t var = 0; var < 3; var++) {
		Npc* data = nullptr;
		EXPECT_TRUE(useData(data)) << "var " << var;
		EXPECT_EQ(varOf(1612), var + 1);
		EXPECT_EQ(stateOf(1612)->getStatus(), QuestStatus::START);
		EXPECT_TRUE(data->isDead()) << "var " << var << ": npc.getController().die(player)";
	}
	Npc* last = nullptr;
	EXPECT_TRUE(useData(last));
	EXPECT_EQ(stateOf(1612)->getStatus(), QuestStatus::REWARD) << "var 3: the reward";
	EXPECT_EQ(varOf(1612), 3);
	EXPECT_TRUE(last->isDead());
}

TEST_F(LepharistSecretsTest, AnUntargetedObjectOrAnotherActionDoesNothing) {
	hold(1612, QuestStatus::START, 1);
	Npc& data = spawnNpc(LEPHARIST_RESEARCH_DATA);
	EXPECT_FALSE(talk(data, 1612, DA::USE_OBJECT)) << "the player's target is not the object: useQuestObject answers false";
	EXPECT_EQ(varOf(1612), 1);
	EXPECT_FALSE(data.isDead());
	player().setTarget(Ptr<gameserver::model::gameobjects::VisibleObject>(data));
	EXPECT_FALSE(talk(data, 1612, DA::QUEST_SELECT)) << "USE_OBJECT only";
	player().setTarget(nullptr);
	EXPECT_EQ(varOf(1612), 1);
	EXPECT_FALSE(data.isDead());
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::q03::test
