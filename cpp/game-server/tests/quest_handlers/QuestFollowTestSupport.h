#pragma once

// P5-06a/P5-06b, M5d E-07 (m5d-plan.md §7): the fixture of the quest escort tests - questEngine/task (tests/quest/QuestTasksTest.cpp) and
// AbstractQuestHandler's follow family (tests/quest_handlers/AbstractQuestHandlerFollowTest.cpp) - on the handler-base fixture
// (QuestHandlerTestSupport.h: the quester at (100, 100, 50) in Poeta's instance 1, a TestClient recording his packets, the ManualClock).
//
// - The escort npc is spawned into that instance (AbstractQuestHandler.spawn, as AbstractQuestHandlerSpawnTest does), so the World holds it and
//   its controller's delete takes it out again. The unit tests load no geo data (GeoService.init builds empty geo maps).
// - Nothing moves by itself here: a case moves the npc (and the player) by setting their coordinates, which is all the check task reads.
// - FollowingProbeAI stands in for FollowingNpcAI (data/handlers/ai/FollowingNpcAI.java, AI "following", the AI of every follower of the 14
//   escort quests), which has no C++ file yet (m5c-plan.md D2: the capital-economy milestone): it records the FOLLOW_ME and STOP_FOLLOW_ME it
//   handles and handles them as FollowingNpcAI does (FollowEventHandler.follow / stopFollow), and carries the registry entry "following" that
//   AbstractAI.getName answers. Without it an npc keeps its template's AI, a DummyNpcAI here (gameserver.dev.missing_ai_handlers=warn), whose
//   getName is "noname" and which ignores both events.
// - EscortHandler is a fabricated handler registered with the engine for a real quest of the fixture (as ProbeHandler is): it registers for the
//   reach and lost target events (QuestEngine.registerAddOnReachTargetEvent / registerAddOnLostTargetEvent) and records each one it gets. With
//   `endSteps` it answers them as the Java escorts do, through defaultFollowEndEvent: _1149MissingPoppy.java:84-92, the follow started at step 0
//   -> 1, reach: step 1 to REWARD with movie 12, lost: step 1 back to 0. It also notes whether the player still had his QUEST_FOLLOW task when the
//   event came, and runs the case's `atEvent` then, so a case can see what the stop had done before the event (FollowingNpcCheckTask.java:49-57:
//   onSuccess and onFail stop first).
// - The spawn row of the npc id overload is the shipped data's (spawns/Npcs/210010000_Poeta.xml:877-880, mires), published by the case that
//   needs it and reset in the TearDown; so are the kill bounties a player's death reads (none). The tribe relations Npc.getType and SM_NPC_INFO
//   read are published for every case, and SM_NPC_INFO's town comes from a packet lookup (no database here).

#include "QuestHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/FollowEventHandler.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/KillBountyData.bind.h"
#include "aion/gameserver/dataholders/KillBountyData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {

/** spawns/Npcs/210010000_Poeta.xml:877-880, verbatim: mires, the target of the npc id overload */
inline constexpr std::string_view FOLLOW_SPAWNS_XML = R"xml(<spawns>
	<spawn_map map_id="210010000">
		<spawn npc_id="203057" respawn_time="295">
			<spot x="1141" y="1032" z="128.875" h="3"/>
		</spawn>
	</spawn_map>
</spawns>)xml";

/**
 * tribe_relations.xml reduced to what the cases ask (the rows of tests/playersvc/PvpDeathRewardTest.cpp): the relation type of the striped kerub
 * (MONSTER) to the quester, which Npc.getType and SM_NPC_INFO read
 */
inline constexpr const char* FOLLOW_TRIBE_RELATIONS_XML = R"(<tribe_relations>)"
														  R"(<tribe name="PC"/><tribe name="PC_DARK"/>)"
														  R"(<tribe name="MONSTER"><hostile>PC</hostile><hostile>PC_DARK</hostile></tribe>)"
														  R"(</tribe_relations>)";

/** The registry entry AIEngine.newAI would store for FollowingNpcAI (@AIName("following"), FollowingNpcAI.java:13); no factory is called */
inline const ::aion::gameserver::handlers::AIHandlerEntry FOLLOWING_ENTRY{"following", "ai.FollowingNpcAI", nullptr, "(test stand-in)"};

/** One creature event the probe handled */
struct FollowAiEvent {
	ai::event::AIEventType type;
	int32_t creatureObjectId;
};

/** FollowingNpcAI's follow handling (FollowingNpcAI.java:20-23, 46-49) with a record of the events it handled */
class FollowingProbeAI final : public ai::NpcAI {
public:
	explicit FollowingProbeAI(gameserver::model::gameobjects::Npc& owner) : NpcAI(owner) {}

	std::vector<FollowAiEvent> events;

protected:
	void handleFollowMe(gameserver::model::gameobjects::Creature& creature) override {
		events.push_back({ai::event::AIEventType::FOLLOW_ME, creature.getObjectId()});
		ai::handler::FollowEventHandler::follow(*this, creature);
	}

	void handleStopFollowMe(gameserver::model::gameobjects::Creature& creature) override {
		events.push_back({ai::event::AIEventType::STOP_FOLLOW_ME, creature.getObjectId()});
		ai::handler::FollowEventHandler::stopFollow(*this, creature);
	}
};

/** One reach or lost target event an EscortHandler got */
struct TargetEvent {
	bool reached;
	int32_t handlerQuestId;
	int32_t envQuestId;
	const QuestEnv* env;
	/** The player still had his QUEST_FOLLOW task when the event came */
	bool followTask;
};

/** A fabricated escort handler for a real quest (see the header comment) */
class EscortHandler final : public AbstractQuestHandler {
public:
	EscortHandler(int32_t questId, std::vector<TargetEvent>& events, const std::function<void()>& atEvent, bool endSteps)
		: AbstractQuestHandler(questId), events(events), atEvent(atEvent), endSteps(endSteps) {}

	void register_() override {
		qe.registerAddOnReachTargetEvent(questId);
		qe.registerAddOnLostTargetEvent(questId);
	}

	bool onNpcReachTargetEvent(QuestEnv& env) override {
		record(true, env);
		return endSteps && defaultFollowEndEvent(env, 1, 1, true, 12); // reward
	}

	bool onNpcLostTargetEvent(QuestEnv& env) override {
		record(false, env);
		return endSteps && defaultFollowEndEvent(env, 1, 0, false); // 0
	}

private:
	void record(bool reached, QuestEnv& env) {
		events.push_back({reached, questId, env.getQuestId(), &env,
			env.getPlayer()->getController().hasTask(gameserver::model::TaskId::QUEST_FOLLOW)});
		if (atEvent)
			atEvent();
	}

	std::vector<TargetEvent>& events;
	const std::function<void()>& atEvent;
	const bool endSteps;
};

class QuestFollowTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		// empty geo maps for the published world maps (GeoDataConfig.GEO_ENABLE is false), as AbstractQuestHandlerSpawnTest
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init();
			return true;
		}();
		static_cast<void>(geoInitialised);
		xml::LoadContext context;
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, FOLLOW_TRIBE_RELATIONS_XML));
		// SM_NPC_INFO writes the town of the npc's position (TownService.getTownIdByPosition), and TownService loads its towns from the database,
		// which this executable does not open: Poeta has no town, so the answer is 0 (SM_NPC_INFO.java:127)
		lookups.townIdByPosition = [](gameserver::model::gameobjects::Creature&) { return 0; };
		network::aion::serverpackets::detail::setPacketLookupsForTests(&lookups);
	}

	void TearDown() override {
		atEvent = nullptr; // it may hold references to the case's locals
		for (const Ref<gameserver::model::gameobjects::VisibleObject>& object : spawned) {
			if (object->isSpawned())
				object->getController().delete_();
		}
		spawned.clear();
		network::aion::serverpackets::detail::setPacketLookupsForTests(nullptr);
		if (spawnsPublished)
			dataholders::DataManager::SPAWNS_DATA.resetForTests();
		if (killBountiesPublished)
			dataholders::DataManager::KILL_BOUNTY_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		QuestHandlerTest::TearDown();
	}

	static world::WorldMapInstance& poeta() { return *world::World::getInstance().getWorldMap(210010000)->getMainWorldMapInstance(); }

	/** An npc of the template spawned into Poeta's instance 1 at the coordinates (kept for the TearDown) */
	gameserver::model::gameobjects::Npc& spawnNpc(int32_t templateId, float x, float y, float z) {
		Ptr<gameserver::model::gameobjects::VisibleObject> object = AbstractQuestHandler::spawn(templateId, poeta(), x, y, z, int8_t{0});
		EXPECT_TRUE(object);
		Ptr<gameserver::model::gameobjects::Npc> npc = runtime::as<gameserver::model::gameobjects::Npc>(object);
		EXPECT_TRUE(npc);
		spawned.emplace_back(*object);
		return *npc;
	}

	/**
	 * Gives the npc a FollowingProbeAI, idle as a spawned npc's AI is (the spawn's SPAWNED event moves the AI to IDLE). Unnamed, it has no registry
	 * entry, so getName answers "noname" as a DummyNpcAI's does: the task then deletes the npc, and the probe still records the events
	 */
	static FollowingProbeAI& followingAi(gameserver::model::gameobjects::Npc& npc, bool named = true) {
		auto ai = std::make_unique<FollowingProbeAI>(npc);
		FollowingProbeAI& result = *ai;
		if (named)
			ai->setRegistryEntry(&FOLLOWING_ENTRY);
		npc.replaceAi(std::move(ai));
		result.setStateIfNot(ai::AIState::IDLE);
		return result;
	}

	/** Registers an EscortHandler for `questId` with the engine (addQuestHandler calls its register_); the engine keeps it (Immortal, RT-11) */
	EscortHandler& escort(int32_t questId, bool endSteps = false) {
		auto handler = std::make_unique<EscortHandler>(questId, targetEvents, atEvent, endSteps);
		EscortHandler& result = *handler;
		QuestEngine::getInstance().addQuestHandler(std::move(handler));
		return result;
	}

	void publishFollowSpawns() {
		xml::LoadContext context;
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, FOLLOW_SPAWNS_XML));
		spawnsPublished = true;
	}

	/** The death of a player (PlayerController.onDie) reaches PvpService, whose constructor reads the kill bounties: none here */
	void publishKillBounties() {
		xml::LoadContext context;
		dataholders::DataManager::KILL_BOUNTY_DATA.publish(xml::bindString<dataholders::KillBountyData>(context, "<kill_bounties/>"));
		killBountiesPublished = true;
	}

	/** Moves the object to the coordinates (its region stays: the check task reads the coordinates, the map and the instance only) */
	static void moveTo(gameserver::model::gameobjects::VisibleObject& object, float x, float y, float z) {
		object.getPosition()->setXYZH(x, y, z, std::nullopt);
	}

	bool hasFollowTask() { return player().getController().hasTask(gameserver::model::TaskId::QUEST_FOLLOW); }

	/** Advances the ManualClock by `ms` milliseconds */
	void advance(int64_t ms) { executor->advance(std::chrono::milliseconds(ms)); }

	std::vector<TargetEvent> targetEvents;
	/** Run by the EscortHandler at each event, after it recorded the event (a case sets it to see the follower at that moment) */
	std::function<void()> atEvent;
	std::vector<Ref<gameserver::model::gameobjects::VisibleObject>> spawned;
	network::aion::serverpackets::detail::PacketLookupsForTests lookups{};
	bool spawnsPublished = false;
	bool killBountiesPublished = false;
};

} // namespace aion::gameserver::questEngine::handlers::test
