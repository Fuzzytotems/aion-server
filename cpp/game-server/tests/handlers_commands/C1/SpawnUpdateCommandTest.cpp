// M5j stage 4 CP4 (m5j-plan.md §18.5, L-01): //spawnu (SpawnUpdate.java) on a real Player with a real AionConnection (CommandTestSupport.h),
// npcs built as MonsterCommandsTest builds them and a gatherable as GatheringTaskTest builds one. The texts are the Java literals.
//
// The spawns of the test map come from a <spawns> document (SpawnsData's afterUnmarshal adds them by world): npc 798007 with one spot of walker
// ROUTE_A, and - for the case of Java's NullPointerException - a second document with a spot without walker id. The walker templates are
// ROUTE_A (pool 1) and ROUTE_B (pool 2). SpawnsData.saveSpawn refuses every spawn here (respawn time 0, K-10's guard), so each arm ends with
// the "Could not save spawn" text; the write-back tail is D7.

#include "CommandTestSupport.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GatherableData.bind.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WalkerData.bind.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/handlers/admincommands/SpawnUpdate.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHERABLE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::Npc;
using model::templates::spawns::SpawnGroup;
using model::templates::spawns::SpawnTemplate;
using serverpackets::SM_SYSTEM_MESSAGE;

/** One npc row of npc_templates.xml (MonsterCommandsTest's minalinerk), ai "general" */
constexpr std::string_view NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
	</npc_template>
</npc_templates>)xml";

constexpr std::string_view GATHERABLE_TEMPLATES_XML = R"(<gatherable_templates>)"
	R"(<gatherable_template id="400001" name="Kukuru" nameId="701957" sourceType="VEGETABLE" harvestCount="3" skillLevel="20" harvestSkill="30002" successAdj="100" failureAdj="100" aerialAdj="100">)"
	R"(<materials><material rate="10000000" nameid="702021" itemid="152000001" name="Kukuru"/></materials>)"
	R"(</gatherable_template>)"
	R"(</gatherable_templates>)";

constexpr std::string_view WALKER_TEMPLATES_XML = R"(<npc_walker>)"
	R"(<walker_template route_id="ROUTE_A"><routestep x="1" y="1" z="1"/><routestep x="2" y="2" z="2"/></walker_template>)"
	R"(<walker_template route_id="ROUTE_B" pool="2"><routestep x="1" y="1" z="1"/><routestep x="2" y="2" z="2"/></walker_template>)"
	R"(</npc_walker>)";

/** One world spot of walker ROUTE_A in the test map */
constexpr std::string_view WALKING_SPAWNS_XML = R"(<spawns><spawn_map map_id="210010000"><spawn npc_id="798007" respawn_time="295">)"
	R"(<spot x="10" y="10" z="50" h="0" walker_id="ROUTE_A"/>)"
	R"(</spawn></spawn_map></spawns>)";

/** The same, plus a spot without walker id (Java: null) */
constexpr std::string_view MIXED_SPAWNS_XML = R"(<spawns><spawn_map map_id="210010000"><spawn npc_id="798007" respawn_time="295">)"
	R"(<spot x="10" y="10" z="50" h="0" walker_id="ROUTE_A"/><spot x="20" y="20" z="50" h="0"/>)"
	R"(</spawn></spawn_map></spawns>)";

class SpawnUpdateCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["spawnu"] = 3; // commands.properties
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // ai="general" has no linked handler here: a DummyAI
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML));
		dataholders::DataManager::GATHERABLE_DATA.publish(xml::bindString<dataholders::GatherableData>(context, GATHERABLE_TEMPLATES_XML));
		if (!dataholders::DataManager::HOUSE_DATA) // SM_NPC_INFO's house checks; never reset, as InfoCommandsTest publishes it
			dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(houseContext(), "<house_lands/>"));
		dataholders::DataManager::WALKER_DATA.publish(xml::bindString<dataholders::WalkerData>(context, WALKER_TEMPLATES_XML));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context, R"(<tribe_relations><tribe name="PC"/><tribe name="GENERAL"/></tribe_relations>)"));
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		for (PlayerFixture& fixture : players)
			fixture.player->setTarget(nullptr);
		stored.clear();
		npcs.clear(); // before the map instance their positions name
		gatherables.clear();
		spawnGroups.clear();
		CommandTest::TearDown();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::SPAWNS_DATA.resetForTests();
		dataholders::DataManager::WALKER_DATA.resetForTests();
		dataholders::DataManager::GATHERABLE_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	static xml::LoadContext& houseContext() {
		static xml::LoadContext houseLoadContext; // outlives every case: HOUSE_DATA is never reset
		return houseLoadContext;
	}

	void spawns(std::string_view xml) { dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, xml)); }

	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 3);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	/** The npc 798007 at (101, 100, 50) of a plain SpawnTemplate (respawn time 0), known to the player */
	Npc& npcAt(Player& player, std::optional<std::string> walkerId = std::nullopt) {
		runtime::Ref<SpawnGroup> group = SpawnGroup::create(210010000, 798007, 0, nullptr);
		SpawnTemplate& spawn = *SpawnTemplate::create(*group, 101.0f, 100.0f, 50.0f, int8_t{0}, 0, walkerId, 0);
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(
			std::make_unique<controllers::NpcController>(), spawn, dataholders::DataManager::NPC_DATA->getNpcTemplate(798007));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, 101.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(101.0f, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		static_cast<TestKnownList&>(player.getKnownList()).addForTest(*npc);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	model::gameobjects::Gatherable& gatherableAt() {
		runtime::Ref<SpawnGroup> group = SpawnGroup::create(210010000, 400001, 0, nullptr);
		SpawnTemplate& spawn = *SpawnTemplate::create(*group, 102.0f, 100.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0);
		runtime::Ref<model::gameobjects::Gatherable> gatherable =
			model::gameobjects::VisibleObject::create<model::gameobjects::Gatherable>(spawn, std::make_unique<controllers::GatherableController>());
		gatherable->setPosition(world::WorldPosition::create(210010000, 102.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(102.0f, 100.0f, 50.0f)));
		spawnGroups.push_back(group);
		gatherables.push_back(gatherable);
		return *gatherable;
	}

	void target(Player& player, model::gameobjects::VisibleObject& object) {
		player.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&object));
		client()->clearSent();
	}

	template <class P>
	std::vector<uint8_t> packet(P&& p) { return serialized(std::forward<P>(p), client().con()); }

	/** the packets of the given packet, then of sendInfo(admin, text) for each text */
	std::vector<std::vector<uint8_t>> sequence(std::vector<std::vector<uint8_t>> packets, std::initializer_list<std::string> texts) {
		for (const std::string& text : texts)
			for (std::vector<uint8_t>& p : info(text))
				packets.push_back(std::move(p));
		return packets;
	}

	static constexpr const char* NOT_SAVED =
		"Could not save spawn. Maybe it's a special or temporary spawn (siege, base, invasion, ...) which cannot be altered.";

	xml::LoadContext context;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<Player*> stored;
	std::vector<runtime::Ref<Npc>> npcs;
	std::vector<runtime::Ref<model::gameobjects::Gatherable>> gatherables;
	std::vector<runtime::Ref<SpawnGroup>> spawnGroups;
};

/** SpawnUpdate.java:35-56, :86-89: the syntax, a target that is neither npc nor gatherable, the "w" arm only for npcs, an unknown arm */
TEST_F(SpawnUpdateCommandTest, SyntaxAndTargets) {
	spawns(WALKING_SPAWNS_XML);
	Player& admin = gm(731101);
	handlers::admincommands::SpawnUpdate spawnu;
	EXPECT_TRUE(spawnu.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(spawnu.getSyntaxInfo()));
	const std::vector<std::vector<uint8_t>> invalidTarget{packet(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET())};
	for (std::vector<std::string> params : {args({"x"}), args({"w", "ROUTE_A"})}) {
		client()->clearSent();
		EXPECT_TRUE(spawnu.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), invalidTarget) << "no target: " << params[0];
	}
	target(admin, admin);
	EXPECT_TRUE(spawnu.process(admin, args({"w"})));
	EXPECT_EQ(client()->sentBytes(), invalidTarget) << "a player";
	model::gameobjects::Gatherable& gatherable = gatherableAt();
	target(admin, gatherable);
	EXPECT_TRUE(spawnu.process(admin, args({"W", "ROUTE_A"})));
	EXPECT_EQ(client()->sentBytes(), info(spawnu.getSyntaxInfo())) << "the walker arm is for npcs: a gatherable falls through to the unknown arm";
	Npc& npc = npcAt(admin);
	target(admin, npc);
	EXPECT_TRUE(spawnu.process(admin, args({"xy"})));
	EXPECT_EQ(client()->sentBytes(), info(spawnu.getSyntaxInfo()));
	EXPECT_EQ(npc.getX(), 101.0f) << "unchanged";
}

/** SpawnUpdate.java:58-101: each coordinate arm, from the GM's position or the value; SM_NPC_INFO, the text, the refused save */
TEST_F(SpawnUpdateCommandTest, CoordinatesOfAnNpc) {
	spawns(WALKING_SPAWNS_XML);
	Player& admin = gm(731102);
	admin.getPosition()->setXYZH(100.5f, 99.25f, 50.75f, int8_t{12});
	Npc& npc = npcAt(admin);
	target(admin, npc);
	handlers::admincommands::SpawnUpdate spawnu;

	EXPECT_TRUE(spawnu.process(admin, args({"X"})));
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_NPC_INFO(npc, admin))}, {"Updated Npc's coordinates to\nX:100.5 Y:100.0 Z:50.0 H:0.", NOT_SAVED}));
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"y", "120.5"})));
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_NPC_INFO(npc, admin))}, {"Updated Npc's coordinates to\nX:100.5 Y:120.5 Z:50.0 H:0.", NOT_SAVED}));
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"y"})));
	EXPECT_EQ(npc.getY(), 99.25f);
	EXPECT_TRUE(spawnu.process(admin, args({"z", "-3"})));
	EXPECT_EQ(npc.getZ(), -3.0f);
	EXPECT_TRUE(spawnu.process(admin, args({"z"})));
	EXPECT_EQ(npc.getZ(), 50.75f);
	EXPECT_TRUE(spawnu.process(admin, args({"h", "-7"})));
	EXPECT_EQ(npc.getHeading(), -7);
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"h"})));
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_NPC_INFO(npc, admin))}, {"Updated Npc's coordinates to\nX:100.5 Y:99.25 Z:50.75 H:12.", NOT_SAVED}));
	EXPECT_TRUE(spawnu.process(admin, args({"x", "7"})));
	EXPECT_EQ(npc.getX(), 7.0f);
	EXPECT_EQ(npc.getY(), 99.25f) << "the other coordinates stay (setXYZH with null)";

	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"h", "128"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number.")) << "Byte.parseByte's range";
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"x", "east"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"east\"")) << "Float.parseFloat";
	EXPECT_EQ(npc.getX(), 7.0f);
	EXPECT_EQ(npc.getHeading(), 12);
}

/** SpawnUpdate.java:58-69: xyz and xyzh take the GM's position (and heading); a gatherable gets SM_GATHERABLE_INFO */
TEST_F(SpawnUpdateCommandTest, XyzAndXyzhOfAGatherable) {
	spawns(WALKING_SPAWNS_XML);
	Player& admin = gm(731103);
	admin.getPosition()->setXYZH(98.0f, 97.0f, 51.0f, int8_t{30});
	model::gameobjects::Gatherable& gatherable = gatherableAt();
	target(admin, gatherable);
	handlers::admincommands::SpawnUpdate spawnu;

	EXPECT_TRUE(spawnu.process(admin, args({"xyz"})));
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_GATHERABLE_INFO(gatherable))}, {"Updated Gatherable's coordinates to\nX:98.0 Y:97.0 Z:51.0 H:0.", NOT_SAVED}));
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"XYZH"})));
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_GATHERABLE_INFO(gatherable))}, {"Updated Gatherable's coordinates to\nX:98.0 Y:97.0 Z:51.0 H:30.", NOT_SAVED}));
}

/** SpawnUpdate.java:112-142: no walker to remove, an unknown template, an assignment, a full pool, the removal */
TEST_F(SpawnUpdateCommandTest, WalkerIds) {
	spawns(WALKING_SPAWNS_XML);
	Player& admin = gm(731104);
	Npc& npc = npcAt(admin);
	target(admin, npc);
	handlers::admincommands::SpawnUpdate spawnu;

	EXPECT_TRUE(spawnu.process(admin, args({"w"})));
	EXPECT_EQ(client()->sentBytes(), info("Npc has no walker_id that could be removed."));
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"w", "ROUTE_B", "extra"})));
	EXPECT_EQ(client()->sentBytes(), info("Npc has no walker_id that could be removed.")) << "three parameters: walkerId null";
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"w", "route_c"})));
	EXPECT_EQ(client()->sentBytes(), info("No such template exists in npc_walker.xml."));
	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"w", "route_a"})));
	EXPECT_EQ(client()->sentBytes(), info("Can not assign, walker pool reached the limit.")) << "ROUTE_A (pool 1) has its one world spot";
	EXPECT_FALSE(npc.getSpawn()->getWalkerId());

	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"w", "route_b"})));
	EXPECT_EQ(npc.getSpawn()->getWalkerId(), std::optional<std::string>("ROUTE_B")) << "upper case";
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_DELETE(npc)), packet(serverpackets::SM_NPC_INFO(npc, admin))},
		{"Updated walker_id from null to ROUTE_B for NPC 798007.", "Could not save spawn."}));

	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"w", "ROUTE_B"})));
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_DELETE(npc)), packet(serverpackets::SM_NPC_INFO(npc, admin))},
		{"Updated walker_id from ROUTE_B to ROUTE_B for NPC 798007.", "Could not save spawn."})) << "the npc's own spot is no world spot";

	client()->clearSent();
	EXPECT_TRUE(spawnu.process(admin, args({"w"})));
	EXPECT_FALSE(npc.getSpawn()->getWalkerId());
	EXPECT_EQ(client()->sentBytes(), sequence({packet(serverpackets::SM_DELETE(npc)), packet(serverpackets::SM_NPC_INFO(npc, admin))},
		{"Removed walker_id ROUTE_B for NPC 798007.", "Could not save spawn."}));
}

/** SpawnUpdate.java:128: Java's s.getWalkerId().equals(walkerId) throws a NullPointerException for a world spot without walker id (proposed
 * correction J-S4CP4-1); the command ends in the error text and the walker id is unchanged */
TEST_F(SpawnUpdateCommandTest, ASpotWithoutWalkerIdFailsLikeJava) {
	spawns(MIXED_SPAWNS_XML);
	Player& admin = gm(731105);
	Npc& npc = npcAt(admin);
	target(admin, npc);
	handlers::admincommands::SpawnUpdate spawnu;
	EXPECT_TRUE(spawnu.process(admin, args({"w", "ROUTE_B"})));
	EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>"));
	EXPECT_FALSE(npc.getSpawn()->getWalkerId());
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
