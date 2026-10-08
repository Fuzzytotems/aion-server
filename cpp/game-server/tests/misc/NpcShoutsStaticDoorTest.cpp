// P5-14 NpcShoutsService and StaticDoorService (m5j-plan.md §18.1 stage 1 CP3, item S-07): an npc's shout to one player and to the players
// who see it, its range, the target parameter, the cooldown and the quest pattern's exception, the IDLE shout task on the deterministic clock;
// a door's key, its lock, the access levels, the instance handler's onOpenDoor and the GM state change. Real Players with recording
// connections from the party fixture (tests/team/P5-10b, by relative path, as tests/playersvc does), npcs and a door created as the spawn
// engine does.
//
// Expectations are derived by hand from NpcShoutsService.java:34-125 and StaticDoorService.java:30-91 (StaticDoorState.java: OPENED 1,
// CLICKABLE 2; AdminConfig.java:51-54: open_doors 6, door_info 9).

#include "../team/P5-10b/TeamTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcShoutData.bind.h"
#include "aion/gameserver/dataholders/NpcShoutData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/StaticDoor.h"
#include "aion/gameserver/model/templates/npcshout/NpcShout.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventType.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.bind.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/services/NpcShoutsService.h"
#include "aion/gameserver/services/StaticDoorService.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::StaticDoor;
using model::templates::npcshout::NpcShout;
using services::NpcShoutsService;
using services::StaticDoorService;

constexpr int32_t SHOUTER = 798001;
constexpr int32_t AGGRESSIVE_SHOUTER = 798002;
constexpr int32_t DOOR_ID = 77;
constexpr int32_t KEY = items::SPARKIE_CARAPACE_FRAGMENT;

// a minimal row (the npc_template attributes of PortalServiceTest's rows); 798002's aggro range of 20 makes its shout range 20
constexpr std::string_view NPC_TEMPLATES = R"xml(<npc_templates>
	<npc_template npc_id="798001" level="1" name="shouter" name_id="300001" height="1" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="dummy" sangle="0" attack_speed="2000" hpgauge="3">
		<stats maxHp="100" />
		<bound_radius front="0.25" side="0.35" upper="2" />
	</npc_template>
	<npc_template npc_id="798002" level="1" name="aggressive shouter" name_id="300002" height="1" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="dummy" sangle="0" attack_speed="2000" hpgauge="3" srange="20">
		<stats maxHp="100" />
		<bound_radius front="0.25" side="0.35" upper="2" />
	</npc_template>
</npc_templates>)xml";

/** shout rows: one plain, one with the target parameter, one with the quest pattern, the IDLE row of the shout task (5 s poll delay) */
constexpr std::string_view SHOUTS = R"xml(<npc_shouts><shout_group client_ai="">
	<shout_npcs npc_ids="798001"><shout string_id="1500001" when="SEE" param="hello"/><shout string_id="1500002" when="SEE" param="target"/>
	<shout string_id="1500003" when="SEE" pattern="quest"/><shout string_id="1500004" when="IDLE" poll_delay="5000"/></shout_npcs>
</shout_group></npc_shouts>)xml";

/** A spawn part at the case's coordinates (Java `new SpawnTemplate(group, x, y, z, h, 0, null, staticId)`) */
class ShoutSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	ShoutSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z, int32_t staticId)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, staticId) {}
};

/** An NpcAI whose pattern shouts are allowed or refused by the case (AITemplate refuses them all) */
class PatternAI final : public ai::NpcAI {
public:
	explicit PatternAI(Npc& owner) : NpcAI(owner) {}

	bool allowPattern = true;
	std::vector<std::string> patterns;

	bool onPatternShout(model::templates::npcshout::ShoutEventType event, std::string_view pattern, int32_t skillNumber) override {
		patterns.emplace_back(pattern);
		return allowPattern;
	}
};

/** An instance handler that records onOpenDoor */
class DoorInstanceHandler final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit DoorInstanceHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}
	static runtime::Ref<DoorInstanceHandler> create(world::WorldMapInstance& instance) { return runtime::makeRef<DoorInstanceHandler>(instance); }
	void onOpenDoor(int32_t door) override { openedDoors.push_back(door); }
	std::vector<int32_t> openedDoors;

protected:
	~DoorInstanceHandler() override = default;
};

/** Exposes the protected static KnownList::addPair (the knownlist update's pairing) */
struct Pairing : world::knownlist::KnownList {
	static void pair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { addPair(a, b); }
};

class NpcShoutsStaticDoorTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // this executable links no AI handlers: DummyNpcAI stands in
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), std::string(NPC_TEMPLATES)));
		dataholders::DataManager::NPC_SHOUT_DATA.publish(xml::bindString<dataholders::NpcShoutData>(contexts.emplace_back(), std::string(SHOUTS)));
		static const bool geo = [] {
			world::geo::GeoService::getInstance().init(); // geo data off: one empty GeoMap per test map (StaticDoor.setOpen sets the door state)
			return true;
		}();
		static_cast<void>(geo);
	}

	void TearDown() override {
		for (const runtime::Ref<Npc>& npc : npcs)
			npc->getController().cancelTask(model::TaskId::SHOUT);
		npcs.clear();
		doors.clear();
		spawnGroups.clear();
		doorMapInstance = nullptr;
		TeamTest::TearDown();
		dataholders::DataManager::NPC_SHOUT_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	/** an npc at (x, 100, 50) of the map instance, created as VisibleObjectSpawner.spawnNpc does, with the case's AI */
	Npc& npcAt(int32_t npcId, float x) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<ShoutSpawnTemplate>(*group, x, 100.0f, 50.0f, 0));
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn,
			dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		auto ai = std::make_unique<PatternAI>(*npc);
		patternAi = ai.get();
		npc->replaceAi(std::move(ai));
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	const NpcShout* shoutRow(int32_t stringId) {
		for (const NpcShout* shout : *dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(210010000, SHOUTER))
			if (shout->getStringId() == stringId)
				return shout;
		ADD_FAILURE() << "no shout " << stringId;
		return nullptr;
	}

	/** PacketSendUtility.sendMessage(player, msg): SM_MESSAGE(0, null, msg, GOLDEN_YELLOW) (PacketSendUtility.java:27-29) */
	static serverpackets::SM_MESSAGE message(std::string_view text) { return serverpackets::SM_MESSAGE(0, "", text, model::ChatType::GOLDEN_YELLOW); }

	static SM_SYSTEM_MESSAGE shoutPacket(Npc& npc, int32_t stringId, std::string param) {
		return SM_SYSTEM_MESSAGE(model::ChatType::NPC, runtime::Ptr<model::gameobjects::VisibleObject>(npc), stringId, std::vector<std::string>{std::move(param)});
	}

	/** a closed, clickable door with the static id DOOR_ID in a map instance whose handler records onOpenDoor; the player moves there */
	StaticDoor& doorFor(Member& m, int32_t keyId) {
		doorMapInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [this](world::WorldMapInstance& instance) {
			runtime::Ref<DoorInstanceHandler> handler = DoorInstanceHandler::create(instance);
			doorHandler = handler.get();
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(handler);
		});
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, 300001, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn =
			group->addSpawnTemplate(std::make_unique<ShoutSpawnTemplate>(*group, 100.0f, 100.0f, 50.0f, DOOR_ID));
		xml::LoadContext context;
		const model::templates::staticdoor::StaticDoorTemplate* doorTemplate =
			xml::bindString<model::templates::staticdoor::StaticDoorTemplate>(context,
				"<staticdoor id=\"" + std::to_string(DOOR_ID) + "\" keyid=\"" + std::to_string(keyId) + "\" state=\"2\"/>")
				.release(); // door templates are immortal static data
		runtime::Ref<StaticDoor> door =
			model::gameobjects::VisibleObject::create<StaticDoor>(std::make_unique<controllers::StaticObjectController>(), spawn, doorTemplate, 2);
		door->setKnownlist(std::make_unique<world::knownlist::KnownList>(*door));
		door->setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, doorMapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		door->getPosition()->setIsSpawned(true);
		doorMapInstance->addObject(*door);
		m.player().setPosition(
			world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, doorMapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		m.player().getPosition()->setIsSpawned(true);
		Pairing::pair(*door, m.player());
		spawnGroups.push_back(group);
		doors.push_back(door);
		m.clearSent();
		return *door;
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::deque<xml::LoadContext> contexts;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
	std::vector<runtime::Ref<StaticDoor>> doors;
	runtime::Ref<world::WorldMapInstance> doorMapInstance;
	DoorInstanceHandler* doorHandler = nullptr;
	PatternAI* patternAi = nullptr;
};

// ---- NpcShoutsService -----------------------------------------------------------------------------------------------------------------------

/** NpcShoutsService.java:64-94: a shout to a player in range reaches him alone; out of range (the minimum shout range 10) nothing */
TEST_F(NpcShoutsStaticDoorTest, AShoutToAPlayerNeedsHimInRange) {
	Member& near = addMember("Near", 105.0f);
	Member& far = addMember("Far", 125.0f);
	Npc& npc = npcAt(SHOUTER, 100.0f);
	NpcShoutsService& shouts = NpcShoutsService::getInstance();

	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500001), 0);
	EXPECT_EQ(near.count(shoutPacket(npc, 1500001, "hello")), 1);
	EXPECT_EQ(far.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0);

	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(far.player()), shoutRow(1500001), 0);
	EXPECT_EQ(far.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0) << "15 m: beyond the minimum shout range of 10";

	shouts.shout(nullptr, runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500001), 0);
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), nullptr, 0);
	EXPECT_EQ(near.count(opcodeOf<SM_SYSTEM_MESSAGE>), 1) << "no sender, no shout: nothing";
}

/** NpcTemplate.getMinimumShoutRange: the aggro range when it is above 10 */
TEST_F(NpcShoutsStaticDoorTest, AnAggressiveNpcShoutsAsFarAsItsAggroRange) {
	Member& far = addMember("Far", 115.0f);
	Npc& npc = npcAt(AGGRESSIVE_SHOUTER, 100.0f);
	NpcShoutsService::getInstance().shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(far.player()), shoutRow(1500001), 0);
	EXPECT_EQ(far.count(shoutPacket(npc, 1500001, "hello")), 1) << "15 m, aggro range 20";
}

/** NpcShoutsService.java:84-89: without a target the shout goes to the players who see the npc, within the shout range */
TEST_F(NpcShoutsStaticDoorTest, AShoutWithoutTargetReachesTheSeersInRange) {
	Member& near = addMember("Near", 105.0f);
	Member& far = addMember("Far", 125.0f);
	Member& unseen = addMember("Unseen", 101.0f);
	Npc& npc = npcAt(SHOUTER, 100.0f);
	Pairing::pair(npc, near.player());
	Pairing::pair(npc, far.player());
	clearAll();

	NpcShoutsService::getInstance().shout(runtime::Ptr<Npc>(npc), nullptr, shoutRow(1500001), 0);
	EXPECT_EQ(near.count(shoutPacket(npc, 1500001, "hello")), 1);
	EXPECT_EQ(far.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0) << "a seer beyond the range";
	EXPECT_EQ(unseen.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0) << "in range, but not in the npc's known list";
}

/** NpcShoutsService.java:79-81: the parameter "target" becomes the name of the npc's target's template */
TEST_F(NpcShoutsStaticDoorTest, TheTargetParameterNamesTheNpcsTarget) {
	Member& near = addMember("Near", 105.0f);
	Npc& npc = npcAt(SHOUTER, 100.0f);
	Npc& other = npcAt(AGGRESSIVE_SHOUTER, 102.0f);
	NpcShoutsService& shouts = NpcShoutsService::getInstance();
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500002), 0);
	EXPECT_EQ(near.count(shoutPacket(npc, 1500002, "target")), 1) << "no target: the parameter as written";
	npc.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(other));
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500002), 0);
	EXPECT_EQ(near.count(shoutPacket(npc, 1500002, "aggressive shouter")), 1);
	npc.setTarget(nullptr);
}

/** NpcShoutsService.java:49-56, :90-93: a cooldown blocks mayShout until it ends; 0 or removeShoutCooldown lifts it */
TEST_F(NpcShoutsStaticDoorTest, ACooldownBlocksTheNextShout) {
	Member& near = addMember("Near", 105.0f);
	Npc& npc = npcAt(SHOUTER, 100.0f);
	NpcShoutsService& shouts = NpcShoutsService::getInstance();
	EXPECT_TRUE(shouts.mayShout(npc));
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500001), 30);
	EXPECT_FALSE(shouts.mayShout(npc)) << "30 s";
	shouts.removeShoutCooldown(npc);
	EXPECT_TRUE(shouts.mayShout(npc));
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500001), 30);
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500001), 0);
	EXPECT_TRUE(shouts.mayShout(npc)) << "a shout without cooldown removes the cooldown";
}

/** NpcShoutsService.java:71-72, :82-83: a pattern shout asks the AI first; the quest pattern to a player has no cooldown */
TEST_F(NpcShoutsStaticDoorTest, APatternShoutAsksTheAiAndTheQuestPatternHasNoCooldown) {
	Member& near = addMember("Near", 105.0f);
	Npc& npc = npcAt(SHOUTER, 100.0f);
	NpcShoutsService& shouts = NpcShoutsService::getInstance();
	patternAi->allowPattern = false;
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500003), 30);
	EXPECT_EQ(near.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0) << "the AI refused";
	EXPECT_EQ(patternAi->patterns, (std::vector<std::string>{"quest"}));
	EXPECT_TRUE(shouts.mayShout(npc));

	patternAi->allowPattern = true;
	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500003), 30);
	EXPECT_EQ(near.count(shoutPacket(npc, 1500003, "")), 1) << "no param: Java's writeS(null), the empty string";
	EXPECT_TRUE(shouts.mayShout(npc)) << "the quest pattern to a player drops the cooldown";

	shouts.shout(runtime::Ptr<Npc>(npc), runtime::Ptr<model::gameobjects::player::Player>(near.player()), shoutRow(1500001), 30);
	EXPECT_FALSE(shouts.mayShout(npc)) << "an ordinary shout keeps it";
	shouts.removeShoutCooldown(npc);
}

/** NpcShoutsService.java:58-62: an empty list shouts nothing; a list of one shouts it */
TEST_F(NpcShoutsStaticDoorTest, ARandomShoutOfOneIsThatShout) {
	Member& near = addMember("Near", 105.0f);
	Npc& npc = npcAt(SHOUTER, 100.0f);
	NpcShoutsService& shouts = NpcShoutsService::getInstance();
	shouts.shoutRandom(npc, runtime::Ptr<model::gameobjects::player::Player>(near.player()), {}, 0);
	EXPECT_EQ(near.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0);
	shouts.shoutRandom(npc, runtime::Ptr<model::gameobjects::player::Player>(near.player()), {shoutRow(1500001)}, 0);
	EXPECT_EQ(near.count(shoutPacket(npc, 1500001, "hello")), 1);
}

/**
 * NpcShoutsService.java:34-47, :121-125: the IDLE shouts become the SHOUT task, at once and then every poll delay (5 s, below the random
 * 180-360 s); it shouts only in an active region and while CAN_SHOUT (gameserver.npcshouts.enable and no cooldown). An npc without IDLE shouts
 * gets no task.
 */
TEST_F(NpcShoutsStaticDoorTest, TheIdleShoutsRunAsTheShoutTask) {
	Member& near = addMember("Near", 105.0f);
	Npc& npc = npcAt(SHOUTER, 100.0f);
	Npc& silent = npcAt(AGGRESSIVE_SHOUTER, 100.0f);
	Pairing::pair(npc, near.player());
	clearAll();
	const bool previous = configs::main::AIConfig::SHOUTS_ENABLE.load();
	configs::main::AIConfig::SHOUTS_ENABLE.store(true);

	NpcShoutsService::getInstance().registerShoutTask(silent);
	EXPECT_FALSE(silent.getController().hasTask(model::TaskId::SHOUT)) << "no IDLE shout";

	NpcShoutsService::getInstance().registerShoutTask(npc);
	EXPECT_TRUE(npc.getController().hasTask(model::TaskId::SHOUT));
	executor->advance(std::chrono::milliseconds(0));
	EXPECT_EQ(near.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0) << "the region is inactive";

	mapInstance->getRegion(100.0f, 100.0f, 50.0f)->add(near.player()); // a player in the region activates it (MapRegion.java:89-91)
	executor->advance(std::chrono::milliseconds(5'000));
	EXPECT_EQ(near.count(shoutPacket(npc, 1500004, "")), 1);
	executor->advance(std::chrono::milliseconds(4'999));
	EXPECT_EQ(near.count(shoutPacket(npc, 1500004, "")), 1) << "not before the poll delay";
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(near.count(shoutPacket(npc, 1500004, "")), 2);

	configs::main::AIConfig::SHOUTS_ENABLE.store(false);
	executor->advance(std::chrono::milliseconds(5'000));
	EXPECT_EQ(near.count(shoutPacket(npc, 1500004, "")), 2) << "CAN_SHOUT answers no";
	configs::main::AIConfig::SHOUTS_ENABLE.store(previous);
	mapInstance->getRegion(100.0f, 100.0f, 50.0f)->remove(near.player());
}

// ---- StaticDoorService ----------------------------------------------------------------------------------------------------------------------

/** StaticDoorService.java:70-91: a locked door takes its key from the inventory and stays unlocked; it opens and tells the instance handler */
TEST_F(NpcShoutsStaticDoorTest, ALockedDoorTakesItsKey) {
	ConfigScope<int8_t> openDoors(configs::administration::AdminConfig::INSTANCE_OPEN_DOORS, 6);
	ConfigScope<int8_t> doorInfo(configs::administration::AdminConfig::INSTANCE_DOOR_INFO, 9);
	Member& m = addMember("Alpha");
	StaticDoor& door = doorFor(m, KEY);
	ASSERT_TRUE(door.isLocked());

	StaticDoorService::getInstance().openStaticDoor(m.player(), DOOR_ID);
	EXPECT_FALSE(door.isOpen()) << "no key";
	EXPECT_EQ(m.count(SM_SYSTEM_MESSAGE::STR_CANNOT_OPEN_DOOR_NEED_KEY_ITEM()), 1);
	EXPECT_TRUE(doorHandler->openedDoors.empty());

	runtime::Ref<model::gameobjects::Item> key = items::loadedItem(720901, KEY, 2, model::items::storage::StorageType::CUBE);
	m.player().getInventory().onLoadHandler(*key);
	storedItems.push_back(key);
	m.clearSent();
	StaticDoorService::getInstance().openStaticDoor(m.player(), DOOR_ID);
	EXPECT_TRUE(door.isOpen());
	EXPECT_FALSE(door.isLocked());
	EXPECT_EQ(m.player().getInventory().getItemCountByItemId(KEY), 1) << "one key used";
	EXPECT_EQ(m.count(serverpackets::SM_EMOTION(DOOR_ID, model::EmotionType::OPEN_DOOR, 0x9)), 1) << "StaticDoor.setOpen's broadcast";
	EXPECT_EQ(doorHandler->openedDoors, (std::vector<int32_t>{DOOR_ID}));

	StaticDoorService::getInstance().openStaticDoor(m.player(), DOOR_ID);
	EXPECT_EQ(doorHandler->openedDoors.size(), 1u) << "an open door does not open again";
}

/** StaticDoorService.java:74-75, :78-80: keys 0 and 1 (free, never); the open-doors access level opens any door, door_info tells the ids */
TEST_F(NpcShoutsStaticDoorTest, TheKeyRulesAndTheAccessLevels) {
	ConfigScope<int8_t> openDoors(configs::administration::AdminConfig::INSTANCE_OPEN_DOORS, 6);
	ConfigScope<int8_t> doorInfo(configs::administration::AdminConfig::INSTANCE_DOOR_INFO, 9);
	Member& m = addMember("Alpha");
	StaticDoor& never = doorFor(m, 1);
	StaticDoorService::getInstance().openStaticDoor(m.player(), DOOR_ID);
	EXPECT_FALSE(never.isOpen()) << "key id 1: never";
	EXPECT_EQ(m.count(opcodeOf<SM_SYSTEM_MESSAGE>), 0) << "no key message";
	EXPECT_EQ(m.count(opcodeOf<serverpackets::SM_MESSAGE>), 0) << "no door info below access level 9";

	ConfigScope<int8_t> everyone(configs::administration::AdminConfig::INSTANCE_OPEN_DOORS, 0);
	ConfigScope<int8_t> info(configs::administration::AdminConfig::INSTANCE_DOOR_INFO, 0);
	StaticDoorService::getInstance().openStaticDoor(m.player(), DOOR_ID);
	EXPECT_TRUE(never.isOpen()) << "the open-doors access level";
	EXPECT_EQ(m.count(message("Door ID: 77, key ID: 1")), 1) << "the door_info access level";
}

/** StaticDoorService.java:58-68: no object of the static id, or one that is not a door: a warning and nothing else */
TEST_F(NpcShoutsStaticDoorTest, AMissingDoorIsLogged) {
	Member& m = addMember("Alpha");
	doorFor(m, 0);
	network::test::LogCapture capture({"com.aionemu.gameserver.services.StaticDoorService"});
	StaticDoorService::getInstance().openStaticDoor(m.player(), DOOR_ID + 1);
	EXPECT_EQ(capture.count("Door (ID: 78) is missing near"), 1) << capture.dump();
	EXPECT_TRUE(doorHandler->openedDoors.empty());
}

/** StaticDoorService.java:50-56: the GM state change and its EnumSet.toString message */
TEST_F(NpcShoutsStaticDoorTest, TheStateChangeTellsTheStates) {
	Member& m = addMember("Alpha");
	StaticDoor& door = doorFor(m, 0);
	StaticDoorService::getInstance().changeStaticDoorState(m.player(), DOOR_ID, true, 0x13);
	EXPECT_TRUE(door.isOpen()) << "state & 0xF = 3: OPENED and CLICKABLE";
	EXPECT_EQ(m.count(serverpackets::SM_EMOTION(DOOR_ID, model::EmotionType::OPEN_DOOR, 3)), 1);
	EXPECT_EQ(m.count(message("Door states now are: [OPENED, CLICKABLE]")), 1);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
