#pragma once

// The fixture of the ascension quest handlers' unit cases (lane P6-Q asc-hand, chunk Q06): the four hand-ported handlers
// (handlers/quest/ascension: _1006Ascension, _2008Ascension, _1007ACeremonyinSanctum, _2009ACeremonyinPandaemonium) driven through the real
// QuestEngine and QuestService on tests/instance's ascension world (AscensionTestSupport.h: a DeterministicExecutor on a ManualClock, real
// Players with a real AionConnection, the real map rows of Poeta, Ishalgen, Karamatis and Ataxiar, the Karamatis B and Ataxiar B spawn files
// and a database for SpawnEngine.spawnInstance), plus the verbatim rows of AscensionQuestTestData.h: the four quests, the route's npcs and the
// quest and reward items.
//
// - The handlers are created through their AION_QUEST_HANDLER factories (the executable links the empty registries) and handed to
//   QuestEngine.addQuestHandler, which runs their register_ - as QuestEngine.init does with the registry's table. CustomConfig's
//   ENABLE_SIMPLE_2NDCLASS is off unless a case turns it on before registering (the flag is read by register_ only).
// - The quester is a Warrior of the given race and level (and exp), with an empty quest list and skill list, online and in the World, spawned
//   at the given place with a client of its own. The level is set before he is online, so no level change runs (PlayerCommonData.getPlayer).
// - The npcs a case talks to or kills are spawned with AbstractQuestHandler.spawn (SpawnEngine.newSingleTimeSpawn) into the map instance the
//   route has them in; the engine is reached the way the packets reach it: QuestEngine.onDialog (CM_DIALOG_SELECT), onKill, onDie,
//   onEnterWorld, onLevelChanged, onItemUseEvent.
// - Expected packets are Java's bytes where the packet's fields are the handler's (SM_QUEST_ACTION, SM_DIALOG_WINDOW, SM_PLAY_MOVIE,
//   SM_ASCENSION_MORPH, SM_EMOTION's id; ServerPacketsOpcodes.java: 37, 60, 105, 124, 182).

#include "AscensionQuestTestData.h"
#include "../instance/AscensionTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/MotionData.bind.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// the AION_QUEST_HANDLER factories of the four handlers (HandlerRegistry.h: external linkage in the handler's namespace)
namespace aion::gameserver::handlers::quest::ascension {
::aion::gameserver::handlers::QuestFactory _1006Ascension_questFactory;
::aion::gameserver::handlers::QuestFactory _2008Ascension_questFactory;
::aion::gameserver::handlers::QuestFactory _1007ACeremonyinSanctum_questFactory;
::aion::gameserver::handlers::QuestFactory _2009ACeremonyinPandaemonium_questFactory;
} // namespace aion::gameserver::handlers::quest::ascension

namespace aion::gameserver::handlers::quest::ascension::test {

using namespace std::chrono_literals;
using ::aion::gameserver::model::PlayerClass;
using ::aion::gameserver::model::Race;
using ::aion::gameserver::model::gameobjects::Npc;
using ::aion::gameserver::model::gameobjects::VisibleObject;
using ::aion::gameserver::model::gameobjects::player::Player;
using ::aion::gameserver::network::test::PacketWriter;
using ::aion::gameserver::questEngine::QuestEngine;
using ::aion::gameserver::questEngine::handlers::AbstractQuestHandler;
using ::aion::gameserver::questEngine::model::QuestEnv;
using ::aion::gameserver::questEngine::model::QuestState;
using ::aion::gameserver::questEngine::model::QuestStatus;
using ::aion::gameserver::runtime::Ptr;
using ::aion::gameserver::runtime::Ref;
namespace cptest = ::aion::gameserver::network::aion::clientpackets::testing;
namespace itest = ::aion::gameserver::instance::test;

// maps (world_maps.xml) and the route's positions
inline constexpr int32_t POETA = itest::POETA;
inline constexpr int32_t ISHALGEN = itest::ISHALGEN;
inline constexpr int32_t KARAMATIS_B = itest::KARAMATIS_B;
inline constexpr int32_t ATAXIAR_B = itest::ATAXIAR_B;
inline constexpr int32_t SANCTUM = 110010000;
inline constexpr int32_t PANDAEMONIUM = 120010000;

// npcs (npc_templates.xml)
inline constexpr int32_t PERNOS = 790001;
inline constexpr int32_t DAMINU = 730008;
inline constexpr int32_t BELPARTAN = 205000;
inline constexpr int32_t RAIDER = 211042;
inline constexpr int32_t ORISSAN = 211043;
inline constexpr int32_t LEAH = 203725;
inline constexpr int32_t JUCLEAS = 203752;
inline constexpr int32_t MACUS = 203758; // 1007's reward npc of a Warrior (var 10)
inline constexpr int32_t MUNIN = 203550;
inline constexpr int32_t URD = 790003;
inline constexpr int32_t VERDANDI = 790002;
inline constexpr int32_t SKULD = 203546;
inline constexpr int32_t HAGEN = 205020;
inline constexpr int32_t GUARDIAN_ASSASSIN = 205040;
inline constexpr int32_t HELLION = 205041;
inline constexpr int32_t HEIMDALL = 204182;
inline constexpr int32_t BALDER = 204075;
inline constexpr int32_t KALSTEN = 204080; // 2009's reward npc of a Warrior (var 10)

// items (item_templates.xml)
inline constexpr int32_t PERNOS_BOTTLE = 182200007;
inline constexpr int32_t FILLED_BOTTLE = 182200008;
inline constexpr int32_t DAMINUS_ESSENCE = 182200009;
inline constexpr int32_t CARD_OF_THE_PAST = 182203009;
inline constexpr int32_t CARD_OF_THE_PRESENT = 182203010;
inline constexpr int32_t CARD_OF_THE_FUTURE = 182203011;

// player_experience_table.xml: the first exp of level 9 and of level 10 (a level-9 starting class with a full bar holds the second)
inline constexpr int64_t LEVEL_9_EXP = 82982;
inline constexpr int64_t LEVEL_10_EXP = 126069;
/** quest_data.xml:66 and :9300: <rewards exp="73200"/> */
inline constexpr int64_t ASCENSION_EXP = 73200;

// ServerPacketsOpcodes.java
inline constexpr int32_t SM_TELEPORT_LOC_OPCODE = 20;
inline constexpr int32_t SM_EMOTION_OPCODE = 37;
inline constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;
inline constexpr int32_t SM_PLAY_MOVIE_OPCODE = 105;
inline constexpr int32_t SM_QUEST_ACTION_OPCODE = 124;
inline constexpr int32_t SM_ASCENSION_MORPH_OPCODE = 182;

// QuestStatus.value() (QuestStatus.java:11-14)
inline constexpr int32_t START = 3;
inline constexpr int32_t REWARD = 4;
inline constexpr int32_t COMPLETE = 5;
inline constexpr int32_t LOCKED = 6;

/** One server packet as Java writes it: AionServerPacket.writeOP ([H op][C 0x44][H ~op], op = Crypt.encodeServerPacketOpcode) and the body */
inline std::vector<uint8_t> javaPacket(int32_t opcode, const PacketWriter& body) {
	int32_t op = (opcode + 207) ^ 0xDF;
	PacketWriter packet;
	packet.H(op).C(0x44).H(~op);
	packet.B(body.data);
	return packet.data;
}

/** SM_QUEST_ACTION(ADD = 1 / UPDATE = 2, qs) (SM_QUEST_ACTION.java:67-81): C type, D quest, C status, C 0, D vars | flags << 24, H 0 [, C 0] */
inline std::vector<uint8_t> questAction(int32_t type, int32_t questId, int32_t statusValue, int32_t vars) {
	PacketWriter body;
	body.C(type).D(questId).C(statusValue).C(0).D(vars).H(0);
	if (type == 1)
		body.C(0);
	return javaPacket(SM_QUEST_ACTION_OPCODE, body);
}

inline std::vector<uint8_t> questAdd(int32_t questId, int32_t statusValue, int32_t vars = 0) {
	return questAction(1, questId, statusValue, vars);
}

inline std::vector<uint8_t> questUpdate(int32_t questId, int32_t statusValue, int32_t vars) {
	return questAction(2, questId, statusValue, vars);
}

/** SM_DIALOG_WINDOW (SM_DIALOG_WINDOW.java:29-40), a page other than MAIL and TOWN_CHALLENGE_TASK: D target, H page, D quest, H 0, H 0 */
inline std::vector<uint8_t> dialogWindow(int32_t targetObjectId, int32_t dialogPageId, int32_t questId) {
	return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(targetObjectId).H(dialogPageId).D(questId).H(0).H(0));
}

/** SM_PLAY_MOVIE (SM_PLAY_MOVIE.java:27-35) of AbstractQuestHandler.playQuestMovie: C 0, D target, D quest, D movie, C 0, C 0 (can skip) */
inline std::vector<uint8_t> questMovie(int32_t targetObjectId, int32_t questId, int32_t movieId) {
	return javaPacket(SM_PLAY_MOVIE_OPCODE, PacketWriter().C(0).D(targetObjectId).D(questId).D(movieId).C(0).C(0));
}

/** SM_ASCENSION_MORPH (SM_ASCENSION_MORPH.java:20-23): C inascension, C 0 */
inline std::vector<uint8_t> ascensionMorph(int32_t inascension) {
	return javaPacket(SM_ASCENSION_MORPH_OPCODE, PacketWriter().C(inascension).C(0));
}

/**
 * SM_TELEPORT_LOC (SM_TELEPORT_LOC.java:31-40) of a beam teleport (TeleportService.sendLoc, TeleportAnimation.FADE_OUT_BEAM = 1): C animation,
 * D map, D map (an open map; the instance id for an instance map), F x, F y, F z, C heading - the whole destination the handler passes
 */
inline std::vector<uint8_t> beamTo(int32_t mapId, float x, float y, float z, int32_t heading) {
	return javaPacket(SM_TELEPORT_LOC_OPCODE, PacketWriter().C(1).D(mapId).D(mapId).F(x).F(y).F(z).C(heading));
}

/** SM_DIALOG_WINDOW(targetObjectId, 0) (SM_DIALOG_WINDOW.java:29-40): the window closed */
inline std::vector<uint8_t> dialogClosed(int32_t targetObjectId) {
	return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(targetObjectId).H(0).D(0).H(0).H(0));
}

class AscensionQuestTest : public itest::AscensionWorldTest {
protected:
	void SetUp() override {
		itest::AscensionWorldTest::SetUp();
		if (IsSkipped() || HasFatalFailure())
			return;
		savedSimpleSecondClass = configs::main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.load();
		configs::main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.store(false);
		// the npcs of the instance spawn files and the route's own
		std::string npcRows = itest::npcTemplatesXml();
		npcRows.erase(npcRows.rfind("</npc_templates>"));
		npcRows += questNpcTemplateRows();
		npcRows += "</npc_templates>\n";
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(questContext, npcRows));
		// the tribes of the route's npcs, beside the ones of the instance npcs and the players (KnownList asks them whenever the two meet)
		std::string tribeRows = itest::tribeRelationsXml();
		tribeRows.erase(tribeRows.rfind("</tribe_relations>"));
		tribeRows += questTribeRelationRows();
		tribeRows += "</tribe_relations>\n";
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(questContext, tribeRows));
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(questContext, questDataXml()));
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(questContext, itemTemplatesXml()));
		// what an item added to the cube asks (ItemPacketTestSupport.h's holders): no cleanup entries, no motions, no item sets
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		dataholders::DataManager::MOTION_DATA.publish(xml::bindString<dataholders::MotionData>(questContext, "<motion_times/>"));
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
		// ClassChangeService.setClass -> SkillLearnService.learnNewSkills reads the skill tree: none here (ClassChangeServiceTest pins it)
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(questContext, "<skill_tree/>"));
		// the two flight skills (281, 257) in place of InWorldPacketTest's empty skill data, which its TearDown resets
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(questContext, skillTemplatesXml()));
		// the quest reward rates at Java's defaults (RatesConfig.java)
		savedQuestKinahRates = *configs::main::RatesConfig::QUEST_KINAH_RATES.get();
		savedXpQuestRates = *configs::main::RatesConfig::XP_QUEST_RATES.get();
		configs::main::RatesConfig::QUEST_KINAH_RATES.set({1.0f});
		configs::main::RatesConfig::XP_QUEST_RATES.set({1.0f});
		// QuestEngine::clear cancels the daily message in the cron service (QuestEngine.java:119)
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
		questPrepared = true;
	}

	void TearDown() override {
		if (questPrepared) {
			// the npcs the cases spawned into the open maps' first instances, which World keeps for the whole process (the base fixture
			// destroys the instances a case created, and their npcs with them)
			for (int32_t mapId : {POETA, ISHALGEN}) {
				std::vector<Ptr<Npc>> spawned;
				world::World::getInstance().getWorldMap(mapId)->getWorldMapInstance(1)->forEachNpc([&](Npc& npc) { spawned.push_back(Ptr<Npc>(npc)); });
				for (const Ptr<Npc>& npc : spawned)
					npc->getController().delete_();
			}
			npcs.clear();
			QuestEngine::getInstance().clear(); // the handlers themselves are Immortal (RT-11)
			services::cron::CronService::resetForTests();
		}
		itest::AscensionWorldTest::TearDown();
		if (questPrepared) {
			dataholders::DataManager::QUEST_DATA.resetForTests();
			dataholders::DataManager::ITEM_DATA.resetForTests();
			dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
			dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
			dataholders::DataManager::MOTION_DATA.resetForTests();
			dataholders::DataManager::ITEM_SET_DATA.resetForTests();
			configs::main::RatesConfig::QUEST_KINAH_RATES.set(savedQuestKinahRates);
			configs::main::RatesConfig::XP_QUEST_RATES.set(savedXpQuestRates);
			configs::main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.store(savedSimpleSecondClass);
		}
	}

	/** QuestEngine.init's registration of the four handlers (their register_ reads ENABLE_SIMPLE_2NDCLASS) */
	static void registerHandlers() {
		QuestEngine& engine = QuestEngine::getInstance();
		engine.addQuestHandler(_1006Ascension_questFactory());
		engine.addQuestHandler(_2008Ascension_questFactory());
		engine.addQuestHandler(_1007ACeremonyinSanctum_questFactory());
		engine.addQuestHandler(_2009ACeremonyinPandaemonium_questFactory());
	}

	/**
	 * The quester: a Warrior of `race` at `level` (or at `exp` if given), with an empty quest list and skill list, online and in the World, spawned
	 * at the given place with a client of his own; his queue is cleared
	 */
	void spawnQuester(Race race, int32_t level, int32_t mapId, int32_t instanceId, float x, float y, float z, std::optional<int64_t> exp = std::nullopt) {
		actor = cptest::makePlayer(420001, 9411, "Ascender", race);
		actor.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*actor.player));
		actor.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
		actor.player->setSkillList(model::skill::PlayerSkillList::create({}));
		// the map change of a beam teleport asks his npc factions (PlayerNpcFactionsDAO: none)
		actor.player->setNpcFactions(std::make_unique<model::gameobjects::player::npcFaction::NpcFactions>(*actor.player));
		actor.commonData->setLevel(level); // before he is online: no level change
		if (exp)
			actor.commonData->setExp(*exp);
		actor.commonData->setOnline(true);
		world::World& world = world::World::getInstance();
		world.storeObject(*actor.player);
		ASSERT_TRUE(world.setPosition(Ptr<VisibleObject>(*actor.player), mapId, instanceId, x, y, z, int8_t{0}));
		world.spawn(Ptr<VisibleObject>(*actor.player));
		ASSERT_TRUE(actor.player->isSpawned());
		client = std::make_unique<cptest::TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	Player& player() { return *actor.player; }

	/**
	 * Where the flight teleport (1001 / 3001, CM_MOVE of the flying client) ends: the arena of the raiders. The npcs an instance spawns know a
	 * player only within their visibility (KnownList), so the cases stand him there before the raiders come, as the flight would.
	 */
	void landAt(float x, float y, float z) { world::World::getInstance().updatePosition(player(), x, y, z, int8_t{0}); }

	/** CM_LEVEL_READY's spawn (World.spawn): a teleport into another map waits for it, which the unit cases stand in for */
	void levelReady() {
		if (!player().isSpawned())
			world::World::getInstance().spawn(Ptr<VisibleObject>(player()));
		ASSERT_TRUE(player().isSpawned());
	}

	/** CM_TELEPORT_ANIMATION_DONE.runImpl: a beam teleport (TeleportService.sendLoc) ends when the client says its animation is done */
	void animationDone() {
		Ptr<runtime::Future> task = player().getController().getAndRemoveTask(model::TaskId::TELEPORT);
		ASSERT_TRUE(task) << "no teleport is waiting for its animation";
		task->run();
		task->get();
	}

	void clearSent() { (*client)->clearSent(); }

	/** An npc of the template spawned 2 m beside the quester, in his map instance (AbstractQuestHandler.spawn, a single-time spawn) */
	Npc& npcBeside(int32_t npcId) {
		Ptr<VisibleObject> spawned =
			AbstractQuestHandler::spawn(npcId, *player().getWorldMapInstance(), player().getX() + 2.0f, player().getY(), player().getZ(), int8_t{0});
		Ptr<Npc> npc = runtime::cast<Npc>(spawned);
		EXPECT_TRUE(npc) << npcId;
		npcs.push_back(Ref<Npc>(*npc));
		return *npc;
	}

	/** A quest state as PlayerQuestListDAO loads it (stored), in the quester's list */
	Ref<QuestState> hold(int32_t questId, QuestStatus status, int32_t vars = 0) {
		int32_t completeCount = status == QuestStatus::COMPLETE ? 1 : 0;
		Ref<QuestState> qs = QuestState::create(questId, status, vars, 0, completeCount, std::nullopt, std::nullopt, std::nullopt);
		qs->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATED);
		player().getQuestStateList()->addQuest(questId, *qs);
		return qs;
	}

	/** An item put into his cube the way a quest gives it (ItemService.addItem) */
	bool give(int32_t itemId) { return services::item::ItemService::addItem(player(), itemId, 1) == 0; }

	Ptr<QuestState> state(int32_t questId) { return player().getQuestStateList()->getQuestState(questId); }

	int32_t varOf(int32_t questId) {
		Ptr<QuestState> qs = state(questId);
		return qs ? qs->getQuestVars()->getQuestVars() : -1;
	}

	std::optional<QuestStatus> statusOf(int32_t questId) {
		Ptr<QuestState> qs = state(questId);
		return qs ? std::optional(qs->getStatus()) : std::nullopt;
	}

	/** CM_DIALOG_SELECT's quest arm: QuestEngine.onDialog with the npc, the quest and the action */
	bool talk(VisibleObject* target, int32_t questId, int32_t dialogActionId) {
		Ref<QuestEnv> env = QuestEnv::create(target ? Ptr<VisibleObject>(*target) : Ptr<VisibleObject>(), player(), questId, dialogActionId);
		return QuestEngine::getInstance().onDialog(*env);
	}

	bool talk(Npc& npc, int32_t questId, int32_t dialogActionId) { return talk(static_cast<VisibleObject*>(&npc), questId, dialogActionId); }

	/** NpcController.onDie's reward arm: QuestEngine.onKill with the npc as the target */
	bool kill(Npc& npc) {
		Ref<QuestEnv> env = QuestEnv::create(Ptr<VisibleObject>(npc), player(), 0);
		return QuestEngine::getInstance().onKill(*env);
	}

	/** The npcs of the quester's map instance with the template id */
	std::vector<Ptr<Npc>> npcsOf(int32_t npcId) {
		std::vector<Ptr<Npc>> found;
		player().getWorldMapInstance()->forEachNpc([&](Npc& npc) {
			if (npc.getNpcId() == npcId)
				found.push_back(Ptr<Npc>(npc));
		});
		return found;
	}

	size_t countSent(const std::vector<uint8_t>& packet) {
		std::vector<std::vector<uint8_t>> bytes = sent();
		return static_cast<size_t>(std::count(bytes.begin(), bytes.end(), packet));
	}

	bool sentOpcode(int32_t opcode) {
		std::vector<uint8_t> header = javaPacket(opcode, PacketWriter());
		return wasSentClass(header);
	}

	bool questPrepared = false;
	bool savedSimpleSecondClass = false;
	xml::LoadContext questContext;
	std::vector<Ref<Npc>> npcs;
	std::vector<float> savedQuestKinahRates;
	std::vector<float> savedXpQuestRates;
};

} // namespace aion::gameserver::handlers::quest::ascension::test
