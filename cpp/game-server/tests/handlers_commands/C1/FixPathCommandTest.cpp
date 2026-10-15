// M5j stage 4 CP5 (m5j-plan.md §18.5, D7/D14 = (a)): //fixpath (FixPath.java) and the walker write-back it ends in (WalkerData.addTemplate,
// saveData). The command's arms before its route task run on a real Player (CommandTestSupport.h); the route task itself (teleports of the
// GM's client and the waits for its Z) needs a client that moves, so it is the in-game retest's. WalkerData.saveData writes in a scratch working
// directory; the expected file is JAXB's formatted output of Java's objects.

#include "CommandTestSupport.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/WalkerData.bind.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/handlers/admincommands/FixPath.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/spawnengine/WalkerGroupType.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::CustomPlayerState;
using model::templates::spawns::SpawnGroup;
using model::templates::spawns::SpawnTemplate;
using model::templates::walker::RouteStep;
using model::templates::walker::WalkerTemplate;

constexpr std::string_view NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" /></stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
	</npc_template>
</npc_templates>)xml";

constexpr std::string_view WALKER_TEMPLATES_XML = R"(<npc_walker>)"
	R"(<walker_template route_id="ROUTE_A"><routestep x="1" y="1" z="1"/><routestep x="2" y="2" z="2"/></walker_template>)"
	R"(</npc_walker>)";

class FixPathCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["fixpath"] = 3;
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // ai="general" has no linked handler here: a DummyAI
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML));
		dataholders::DataManager::WALKER_DATA.publish(xml::bindString<dataholders::WalkerData>(context, WALKER_TEMPLATES_XML));
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		for (PlayerFixture& fixture : players)
			fixture.player->setTarget(nullptr);
		stored.clear();
		npcs.clear();
		spawnGroups.clear();
		CommandTest::TearDown();
		dataholders::DataManager::WALKER_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	Player& gm(int32_t objectId, std::string_view name = "Warden") {
		Player& player = connected(objectId, name, 3);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	Npc& npcAt(Player& player, std::optional<std::string> walkerId) {
		runtime::Ref<SpawnGroup> group = SpawnGroup::create(210010000, 798007, 0, nullptr);
		SpawnTemplate& spawn = *SpawnTemplate::create(*group, 101.0f, 100.0f, 50.0f, int8_t{0}, 0, walkerId, 0);
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(
			std::make_unique<controllers::NpcController>(), spawn, dataholders::DataManager::NPC_DATA->getNpcTemplate(798007));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, 101.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(101.0f, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		player.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(npc.get()));
		return *npc;
	}

	xml::LoadContext context;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<Player*> stored;
	std::vector<runtime::Ref<Npc>> npcs;
	std::vector<runtime::Ref<SpawnGroup>> spawnGroups;
};

/** FixPath.java:38-90: the syntax, no route, the runner's lock and its cancel (before the route task runs) */
TEST_F(FixPathCommandTest, ArmsBeforeTheRouteTask) {
	Player& admin = gm(731301);
	handlers::admincommands::FixPath fixpath;
	EXPECT_TRUE(fixpath.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(fixpath.getSyntaxInfo())) << "no parameter and no npc target";
	client()->clearSent();
	EXPECT_TRUE(fixpath.process(admin, args({"ROUTE_X"})));
	EXPECT_EQ(client()->sentBytes(), info("Couldn't find route ID.")) << "an unknown route without an npc target";
	npcAt(admin, std::nullopt);
	client()->clearSent();
	EXPECT_TRUE(fixpath.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info("Couldn't find route ID.")) << "an npc target without a walker id";

	// the route given: the runner is set and the route task is queued (the test's executor does not run it)
	client()->clearSent();
	EXPECT_TRUE(fixpath.process(admin, args({"ROUTE_A", "0.5"})));
	EXPECT_EQ(client()->sentBytes(), info("Make sure you are on the correct map. If not use <cancel>!"));

	Player& other = gm(731302, "Other");
	client(1)->clearSent();
	EXPECT_TRUE(fixpath.process(other, args({"ROUTE_A"})));
	EXPECT_EQ(client(1)->sentBytes(), info("Someone is already running this command!", 1));

	client()->clearSent();
	EXPECT_TRUE(fixpath.process(admin, args({"go"})));
	EXPECT_EQ(client()->sentBytes(), info(fixpath.getSyntaxInfo())) << "the runner's other words";
	admin.setCustomState(CustomPlayerState::INVULNERABLE); // what the route task does first
	client()->clearSent();
	EXPECT_TRUE(fixpath.process(admin, args({"cancel"})));
	EXPECT_EQ(client()->sentBytes(), info("Canceled path fixing."));
	EXPECT_FALSE(admin.isInCustomState(CustomPlayerState::INVULNERABLE)) << "stop(): the GM was not invulnerable before";

	client(1)->clearSent();
	EXPECT_TRUE(fixpath.process(other, args({"NO_ROUTE"})));
	EXPECT_EQ(client(1)->sentBytes(), info("Couldn't find route ID.", 1)) << "the runner is free again";
}

/** FixPath.java:51 with the runner and no parameter (an npc target): Java's "cancel".equals(params[0]) is an index error, ChatCommand's
 * error text (proposed correction J-S4CP5-1) */
TEST_F(FixPathCommandTest, TheRunnerWithoutAParameterIsJavasIndexError) {
	Player& admin = gm(731303);
	handlers::admincommands::FixPath fixpath;
	npcAt(admin, std::string("ROUTE_A"));
	client()->clearSent();
	EXPECT_TRUE(fixpath.process(admin, args({})));
	EXPECT_TRUE(client()->sentBytes().empty()) << "the npc's route: no answer before the task";
	client()->clearSent();
	EXPECT_TRUE(fixpath.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info("<Error while executing command>"));
	EXPECT_TRUE(fixpath.process(admin, args({"cancel"})));
}

/** WalkerData.java:52-75: addTemplate and saveData write ./data/static_data/npc_walker/generated_npc_walker_<route>.xml as JAXB does, then
 * clear the list (a second save writes an empty <npc_walker>) */
TEST(WalkerDataSaveTest, SaveDataWritesTheRouteAsJaxbDoes) {
	const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("aion_gs_walker_writeback_" + std::to_string(std::random_device()()));
	const std::filesystem::path previous = std::filesystem::current_path();
	std::filesystem::create_directories(dir / "data/static_data/npc_walker");
	std::filesystem::current_path(dir);
	auto read = [&dir](const char* name) {
		std::ifstream in(dir / "data/static_data/npc_walker" / name, std::ios::binary);
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	};
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		dataholders::WalkerData data;
		auto route = std::make_unique<WalkerTemplate>("RT_1");
		std::vector<std::unique_ptr<RouteStep>> steps;
		steps.push_back(std::make_unique<RouteStep>(1.5f, 2.25f, 100.125f, 0));
		steps.push_back(std::make_unique<RouteStep>(3.0f, 4.0f, 99.0f, 5));
		route->setRouteSteps(std::move(steps));
		route->setLoopType(WalkerTemplate::LoopType::WALK_BACK);
		route->setPool(2);
		route->setType(spawnengine::WalkerGroupType::SQUARE);
		route->setRows(std::vector<int32_t>{1, 1});
		data.addTemplate(std::move(route));
		data.saveData("RT_1");
		data.saveData("RT_2");
	}
	const std::string first = read("generated_npc_walker_RT_1.xml");
	const std::string second = read("generated_npc_walker_RT_2.xml");
	std::filesystem::current_path(previous);
	std::error_code ignored;
	std::filesystem::remove_all(dir, ignored);
	EXPECT_EQ(first,
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<npc_walker>\n"
		"    <walker_template route_id=\"RT_1\" pool=\"2\" formation=\"SQUARE\" loop_type=\"WALK_BACK\">\n"
		"        <routestep x=\"1.5\" y=\"2.25\" z=\"100.125\" rest_time=\"0\"/>\n"
		"        <routestep x=\"3.0\" y=\"4.0\" z=\"99.0\" rest_time=\"5\"/>\n"
		"    </walker_template>\n"
		"</npc_walker>\n")
		<< "setRows sets the transient int[]: no rows attribute";
	EXPECT_EQ(second, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<npc_walker/>\n") << "the list was cleared (null): JAXB self-closes the empty root";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
