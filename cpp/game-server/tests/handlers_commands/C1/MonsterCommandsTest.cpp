// The stage-0 monster commands (m5j-plan.md §5.2): //spawn, //delete, //kill, //damage, //ai, //npcskill, //useskill, //say (data/handlers/admincommands)
// and K-10's guard prefix of SpawnsData.saveSpawn (SpawnsData.java:205-214), on a real Player with a real AionConnection (CommandTestSupport.h)
// and npcs built as tests/cm_ak/EconomyPacketTestSupport.h builds them. The texts are the Java literals.

#include "CommandTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GatherableData.bind.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/handlers/admincommands/Ai.h"
#include "aion/gameserver/handlers/admincommands/Damage.h"
#include "aion/gameserver/handlers/admincommands/DamagePercent.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/handlers/admincommands/Delete.h"
#include "aion/gameserver/handlers/admincommands/Kill.h"
#include "aion/gameserver/handlers/admincommands/NpcSkill.h"
#include "aion/gameserver/handlers/admincommands/Say.h"
#include "aion/gameserver/handlers/admincommands/SpawnNpc.h"
#include "aion/gameserver/handlers/admincommands/UseSkill.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::gameobjects::Npc;
using model::templates::spawns::SpawnGroup;
using model::templates::spawns::SpawnTemplate;
using serverpackets::SM_SYSTEM_MESSAGE;

/** One npc row of npc_templates.xml (tests/cm_ak/EconomyPacketTestSupport.h's minalinerk), ai "general" */
constexpr std::string_view MONSTER_NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<npc_template npc_id="798007" level="9" name="minalinerk" name_id="351126" height="1.16875" title_id="350377" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
	</npc_template>
</npc_templates>)xml";

/** a special spawn (Java: a SpawnTemplate subclass such as SiegeSpawnTemplate): saveSpawn and //delete refuse it */
class SpecialSpawnTemplate final : public SpawnTemplate {
public:
	SpecialSpawnTemplate(SpawnGroup& group, float x, float y, float z) : SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/**
 * A scratch working directory for SpawnsData.saveSpawn's write-back (Java writes ./data/static_data/spawns relative to the working directory;
 * the tests must never write into the tracked Java tree): created under the temp directory, made current, restored and removed at the end.
 */
struct ScratchDataRoot {
	std::filesystem::path dir;
	std::filesystem::path previous;

	ScratchDataRoot()
		: dir(std::filesystem::temp_directory_path() / ("aion_gs_spawn_writeback_" + std::to_string(std::random_device()()))),
		  previous(std::filesystem::current_path()) {
		std::filesystem::create_directories(dir);
		std::filesystem::current_path(dir);
	}
	~ScratchDataRoot() {
		std::filesystem::current_path(previous);
		std::error_code ignored;
		std::filesystem::remove_all(dir, ignored);
	}
	ScratchDataRoot(const ScratchDataRoot&) = delete;
	ScratchDataRoot& operator=(const ScratchDataRoot&) = delete;

	void write(const std::string& relative, std::string_view text) const {
		std::filesystem::create_directories((dir / relative).parent_path());
		std::ofstream out(dir / relative, std::ios::binary);
		out << text;
	}
	std::string read(const std::string& relative) const {
		std::ifstream in(dir / relative, std::ios::binary);
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
};

class MonsterCommandsTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // ai="general" has no linked handler here: a DummyAI
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, MONSTER_NPC_TEMPLATES_XML));
		dataholders::DataManager::GATHERABLE_DATA.publish(xml::bindString<dataholders::GatherableData>(context, "<gatherable_templates/>"));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, "<spawns/>"));
		// the enmity checks (isEnemy, the known list's see notifications) read the tribe relations: PC and GENERAL without relations
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context, R"(<tribe_relations><tribe name="PC"/><tribe name="GENERAL"/></tribe_relations>)"));
	}

	void TearDown() override {
		for (Player* player : stored)
			world::World::getInstance().removeObject(*player);
		for (const runtime::Ref<Npc>& npc : npcs)
			world::World::getInstance().removeObject(*npc); // false for the npcs the World never held
		for (PlayerFixture& fixture : players)
			fixture.player->setTarget(nullptr);
		stored.clear();
		npcs.clear(); // before the map instance their positions name
		spawnGroups.clear();
		CommandTest::TearDown();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::SPAWNS_DATA.resetForTests();
		dataholders::DataManager::GATHERABLE_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	/** a GM spawned in the fixture's Poeta map instance and stored in the World */
	Player& gm(int32_t objectId) {
		Player& player = connected(objectId, "Warden", 3);
		spawnInPoeta(player);
		world::World::getInstance().storeObject(player);
		stored.push_back(&player);
		return player;
	}

	/**
	 * The npc 798007 at (x, 100, 50), known to the player. `special` gives it a SpawnTemplate subclass; otherwise its template is a plain
	 * SpawnTemplate of a group with `respawnTime`.
	 */
	Npc& npcAt(Player& player, float x, bool special = false, int32_t respawnTime = 0) {
		runtime::Ref<SpawnGroup> group = SpawnGroup::create(210010000, 798007, respawnTime, nullptr);
		SpawnTemplate& spawn = special ? group->addSpawnTemplate(std::make_unique<SpecialSpawnTemplate>(*group, x, 100.0f, 50.0f))
										: *SpawnTemplate::create(*group, x, 100.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0);
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(
			std::make_unique<controllers::NpcController>(), spawn, dataholders::DataManager::NPC_DATA->getNpcTemplate(798007));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		static_cast<TestKnownList&>(player.getKnownList()).addForTest(*npc);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		client()->clearSent();
		return *npc;
	}

	void target(Player& player, model::gameobjects::VisibleObject& object) {
		player.setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(&object));
		client()->clearSent();
	}

	bool sent(const std::vector<uint8_t>& packet) {
		const std::vector<std::vector<uint8_t>> all = client()->sentBytes();
		return std::ranges::find(all, packet) != all.end();
	}

	std::vector<uint8_t> invalidTarget() { return serialized(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET(), client().con()); }

	xml::LoadContext context;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::vector<Player*> stored;
	std::vector<runtime::Ref<Npc>> npcs;
	std::vector<runtime::Ref<SpawnGroup>> spawnGroups;
};

// ---- K-10: SpawnsData.saveSpawn's guard (SpawnsData.java:205-214) ------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, SaveSpawnRefusesWhatIsNoWorldSpawn) {
	Player& player = gm(730200);
	dataholders::SpawnsData& spawns = *dataholders::DataManager::SPAWNS_DATA;
	ScratchDataRoot root; // nothing may be written: the guards return before any file I/O
	EXPECT_FALSE(spawns.saveSpawn(player, false)) << "no spawn template";
	EXPECT_FALSE(spawns.saveSpawn(npcAt(player, 101.0f, true, 300), false)) << "a special spawn (a SpawnTemplate subclass)";
	EXPECT_FALSE(spawns.saveSpawn(npcAt(player, 102.0f, false, 0), true)) << "a single time spawn (respawn time 0)";
	EXPECT_FALSE(std::filesystem::exists(root.dir / "data")) << "the guards touched the file system";
}

// ---- SpawnsData.saveSpawn's write-back (SpawnsData.java:216-273; m5j CP5, D14 = (a)) ------------------------------------------------------
// The working directory is a scratch data root (ScratchDataRoot); the expected files are JAXB's formatted output of Java's objects.

/** //delete of a world spawn: the group is copied from the old template file (exact match), marked custom, the spot removed */
TEST_F(MonsterCommandsTest, SaveSpawnDeleteCopiesTheGroupFromTheTemplateFilesAndRemovesTheSpot) {
	Player& player = gm(730203);
	ScratchDataRoot root;
	root.write("data/static_data/spawns/Npcs/210010000_Poeta.xml", R"(<?xml version="1.0" encoding="utf-8"?>
<spawns>
	<spawn_map map_id="210010000">
		<!-- minalinerk elsewhere: a delete matches the group by its spot (exactMatch), not the first group of the npc -->
		<spawn npc_id="798007" respawn_time="60">
			<spot x="900.0" y="900.0" z="50.0" h="0"/>
		</spawn>
		<!-- minalinerk -->
		<spawn npc_id="798007" respawn_time="295">
			<spot x="103.0" y="100.0" z="50.0" h="0"/>
			<spot x="110.5" y="120.25" z="51.0" h="10" random_walk="5" static_id="77" walker_id="ROUTE" walker_index="2"/>
		</spawn>
		<spawn npc_id="798008" respawn_time="60">
			<spot x="1.0" y="2.0" z="3.0" h="4"/>
		</spawn>
	</spawn_map>
</spawns>
)");
	Npc& npc = npcAt(player, 103.0f, false, 295);
	EXPECT_TRUE(dataholders::DataManager::SPAWNS_DATA->saveSpawn(npc, true));
	EXPECT_EQ(root.read("data/static_data/spawns/Npcs/New/210010000_Poeta.xml"),
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<spawns>\n"
		"    <spawn_map map_id=\"210010000\">\n"
		"        <spawn npc_id=\"798007\" respawn_time=\"295\" difficult_id=\"0\" custom=\"true\">\n"
		"            <spot x=\"110.5\" y=\"120.25\" z=\"51.0\" h=\"10\" static_id=\"77\" random_walk=\"5\" walker_id=\"ROUTE\" walker_index=\"2\"/>\n"
		"        </spawn>\n"
		"    </spawn_map>\n"
		"</spawns>\n");
}

/** A moved world spawn (//spawnu and the like, delete false): the spot of the New file's group is replaced, the spawn template takes the
 * object's position, and addRegularSpawns makes the custom group the npc's world group */
TEST_F(MonsterCommandsTest, SaveSpawnMoveReplacesTheSpotInTheNewFile) {
	Player& player = gm(730204);
	ScratchDataRoot root;
	root.write("data/static_data/spawns/Npcs/New/210010000_Poeta.xml",
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<spawns>\n    <spawn_map map_id=\"210010000\">\n"
		"        <spawn npc_id=\"798007\" respawn_time=\"300\" difficult_id=\"0\" custom=\"true\">\n"
		"            <spot x=\"103.0\" y=\"100.0\" z=\"50.0\" h=\"0\"/>\n        </spawn>\n    </spawn_map>\n</spawns>\n");
	Npc& npc = npcAt(player, 103.0f, false, 300);
	npc.getPosition()->setXYZH(104.5f, 101.0f, 49.75f, int8_t{30});
	EXPECT_TRUE(dataholders::DataManager::SPAWNS_DATA->saveSpawn(npc, false));
	EXPECT_EQ(root.read("data/static_data/spawns/Npcs/New/210010000_Poeta.xml"),
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<spawns>\n"
		"    <spawn_map map_id=\"210010000\">\n"
		"        <spawn npc_id=\"798007\" respawn_time=\"300\" difficult_id=\"0\" custom=\"true\">\n"
		"            <spot x=\"104.5\" y=\"101.0\" z=\"49.75\" h=\"30\"/>\n"
		"        </spawn>\n"
		"    </spawn_map>\n"
		"</spawns>\n");
	EXPECT_EQ(npc.getSpawn()->getX(), 104.5f) << "the spawn template takes the saved spot";
	EXPECT_EQ(npc.getSpawn()->getHeading(), 30);
	bool found = false;
	for (const runtime::Ref<SpawnGroup>& group : dataholders::DataManager::SPAWNS_DATA->getSpawnsForNpc(210010000, 798007))
		for (const runtime::Ptr<SpawnTemplate>& spot : group->getSpawnTemplates().snapshot())
			found = found || (spot->getX() == 104.5f && spot->getY() == 101.0f);
	EXPECT_TRUE(found) << "addRegularSpawns of the written map";
}

/** A spawn in no file: a new group (npc id, respawn time, no handler) with the spot; a second npc of another id adds its own group */
TEST_F(MonsterCommandsTest, SaveSpawnOfANewSpawnCreatesItsGroup) {
	Player& player = gm(730205);
	ScratchDataRoot root;
	std::filesystem::create_directories(root.dir / "data/static_data/spawns/Npcs");
	Npc& npc = npcAt(player, 103.25f, false, 120);
	EXPECT_TRUE(dataholders::DataManager::SPAWNS_DATA->saveSpawn(npc, false));
	EXPECT_EQ(root.read("data/static_data/spawns/Npcs/New/210010000_Poeta.xml"),
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<spawns>\n"
		"    <spawn_map map_id=\"210010000\">\n"
		"        <spawn npc_id=\"798007\" respawn_time=\"120\" difficult_id=\"0\" custom=\"true\">\n"
		"            <spot x=\"103.25\" y=\"100.0\" z=\"50.0\" h=\"0\"/>\n"
		"        </spawn>\n"
		"    </spawn_map>\n"
		"</spawns>\n");
	// a delete of a spot that is not in the group: the group stays, nothing removed (Java: oldSpotIndex -1 and delete)
	Npc& other = npcAt(player, 105.0f, false, 120);
	EXPECT_TRUE(dataholders::DataManager::SPAWNS_DATA->saveSpawn(other, true));
	EXPECT_NE(root.read("data/static_data/spawns/Npcs/New/210010000_Poeta.xml").find("x=\"103.25\""), std::string::npos);
}

/** Java: xml.getParentFile().mkdir() creates one level: without the folder the write fails, "Could not save XML file!" and false */
TEST_F(MonsterCommandsTest, SaveSpawnWithoutTheFolderFails) {
	Player& player = gm(730206);
	ScratchDataRoot root;
	network::test::LogCapture capture({"com.aionemu.gameserver.dataholders.SpawnsData"});
	Npc& npc = npcAt(player, 103.0f, false, 300);
	EXPECT_FALSE(dataholders::DataManager::SPAWNS_DATA->saveSpawn(npc, false));
	EXPECT_FALSE(std::filesystem::exists(root.dir / "data/static_data/spawns/Npcs/New"));
	EXPECT_TRUE(capture.contains("Could not save XML file!"));
	EXPECT_EQ(npc.getSpawn()->getX(), 103.0f) << "the template keeps its position";
}

/** //delete's answer for a saved removal (Delete.java:68-70) */
TEST_F(MonsterCommandsTest, DeleteOfAWorldSpawnRemovesItPermanently) {
	Player& admin = gm(730207);
	ScratchDataRoot root;
	std::filesystem::create_directories(root.dir / "data/static_data/spawns/Npcs");
	Npc& npc = npcAt(admin, 101.0f, false, 300);
	target(admin, npc);
	handlers::admincommands::Delete del;
	EXPECT_TRUE(del.process(admin, args({})));
	EXPECT_TRUE(sent(info("Spawn removed permanently. Npc will not spawn on server start anymore.")[0]));
	EXPECT_TRUE(std::filesystem::exists(root.dir / "data/static_data/spawns/Npcs/New/210010000_Poeta.xml"));
}

// ---- //spawn (SpawnNpc.java:36-66) -------------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, SpawnRefusals) {
	Player& admin = gm(730201);
	handlers::admincommands::SpawnNpc spawn;
	EXPECT_TRUE(spawn.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(spawn.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(spawn.process(admin, args({"1"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid NPC ID.")) << "neither an npc nor a gatherable (and no 9-digit item ID)";
	client()->clearSent();
	EXPECT_TRUE(spawn.process(admin, args({"798007", "-1"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid static ID."));
	client()->clearSent();
	EXPECT_TRUE(spawn.process(admin, args({"798007", "0", "-5"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid respawn time."));
	client()->clearSent();
	EXPECT_TRUE(spawn.process(admin, args({"x"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"x\""));
}

// ---- //delete (Delete.java:30-73) ---------------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, DeleteRefusals) {
	Player& admin = gm(730202);
	handlers::admincommands::Delete del;
	EXPECT_TRUE(del.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(del.getSyntaxInfo())) << "no target";

	target(admin, admin);
	client()->clearSent();
	EXPECT_TRUE(del.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({invalidTarget()})) << "a player";

	Npc& special = npcAt(admin, 101.0f, true);
	target(admin, special);
	client()->clearSent();
	EXPECT_TRUE(del.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info("Can't delete special spawns (spawn type: SpecialSpawn).")) << "the class name without \"Template\"";

	client()->clearSent();
	EXPECT_TRUE(del.process(admin, args({"0.5"})));
	EXPECT_EQ(client()->sentBytes(), info("Deleted 0 objects.")) << "the special npc is 1 m away; a range delete stays quiet about refusals";
}

// ---- //kill (Kill.java:30-96) -------------------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, KillArgumentsAndAPlayerTarget) {
	Player& admin = gm(730203);
	handlers::admincommands::Kill kill;
	EXPECT_TRUE(kill.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(kill.getSyntaxInfo())) << "no target, no parameter";
	client()->clearSent();
	EXPECT_TRUE(kill.process(admin, args({"1", "2", "3"})));
	EXPECT_EQ(client()->sentBytes(), info(kill.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(kill.process(admin, args({"-1"})));
	EXPECT_EQ(client()->sentBytes(), info("The given range must be larger than 0."));
	client()->clearSent();
	EXPECT_TRUE(kill.process(admin, args({"all", "enemy"})));
	EXPECT_EQ(client()->sentBytes(), info("0 NPC(s) were killed.")) << "nothing known";
	client()->clearSent();
	EXPECT_TRUE(kill.process(admin, args({"all", "x"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"x\"")) << "the npc ID filter";
}

TEST_F(MonsterCommandsTest, KillSkipsTheNpcsAFilterLeavesOut) {
	Player& admin = gm(730204);
	npcAt(admin, 104.0f);
	handlers::admincommands::Kill kill;
	EXPECT_TRUE(kill.process(admin, args({"2"})));
	EXPECT_EQ(client()->sentBytes(), info("0 NPC(s) were killed.")) << "4 m away, the range is 2.999";
	client()->clearSent();
	EXPECT_TRUE(kill.process(admin, args({"all", "1234"})));
	EXPECT_EQ(client()->sentBytes(), info("0 NPC(s) were killed.")) << "another npc ID";
	client()->clearSent();
	EXPECT_TRUE(kill.process(admin, args({"all", "enemy"})));
	EXPECT_EQ(client()->sentBytes(), info("0 NPC(s) were killed.")) << "a GENERAL tribe npc is no enemy";
}

TEST_F(MonsterCommandsTest, KillATargetedNpc) {
	Player& admin = gm(730209);
	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	handlers::admincommands::Kill kill;
	EXPECT_TRUE(kill.process(admin, args({})));
	EXPECT_TRUE(npc.isDead());
	EXPECT_TRUE(sent(info("Killed npc: " + utils::ChatUtil::path(npc, true))[0]));
	client()->clearSent();
	EXPECT_TRUE(kill.process(admin, args({})));
	EXPECT_TRUE(sent(info("Couldn't kill npc: " + utils::ChatUtil::path(npc, true))[0])) << "already dead";
}

TEST_F(MonsterCommandsTest, DeleteATargetedWorldNpc) {
	Player& admin = gm(730210);
	Npc& npc = npcAt(admin, 101.0f);
	world::World::getInstance().storeObject(npc); // Java: SpawnEngine stores a spawned object; World.removeObject despawns only those
	target(admin, npc);
	handlers::admincommands::Delete del;
	EXPECT_TRUE(del.process(admin, args({})));
	EXPECT_EQ(world::World::getInstance().findVisibleObject(npc.getObjectId()), nullptr);
	EXPECT_FALSE(npc.isSpawned());
	EXPECT_TRUE(client()->sentBytes().empty() || !sent(invalidTarget())) << "respawn time 0: saveSpawn refuses quietly";
}

// ---- //damage (Damage.java:26-113) and //damagepercent (C++-dev: the split, docs/deviations/C1.md) ------------------------------------------

const char* const DAMAGE_SYNTAX = "syntax //damage (mp/fp/dp) <dmg>\n<dmg> must be a number of points (for a percentage use //damagepercent).\n"
								  "(mp/fp/dp) is optional, leave out to use HP damage\nin case of fp/dp, target must be player!";
const char* const DAMAGE_PERCENT_SYNTAX = "syntax //damagepercent (mp/fp/dp) <percent | percent%>\n<percent> must be a number from 0 to 100 "
										  "(of the max HP/MP/FP, DP: of 4000).\n(mp/fp/dp) is optional, leave out to use HP damage\nin case of "
										  "fp/dp, target must be player!";
const char* const DAMAGE_PERCENT_REFUSAL = "//damage takes absolute points. Use //damagepercent [mp|fp|dp] <percent> for a percentage.";

TEST_F(MonsterCommandsTest, DamageArms) {
	Player& admin = gm(730205);
	handlers::admincommands::Damage damage;
	const std::vector<uint8_t> syntax = message(DAMAGE_SYNTAX);
	EXPECT_TRUE(damage.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	EXPECT_TRUE(damage.process(admin, args({"10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("No target selected")}));

	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	EXPECT_TRUE(damage.process(admin, args({"fp", "10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "fp needs a player";
	client()->clearSent();
	EXPECT_TRUE(damage.process(admin, args({"mp"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "mp needs a value";
	client()->clearSent();
	EXPECT_TRUE(damage.process(admin, args({"mp", "abc"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "catch (Exception): the syntax";
	client()->clearSent();
	const int32_t mp = npc.getLifeStats()->getCurrentMp();
	EXPECT_TRUE(damage.process(admin, args({"MP", "abc%"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message(DAMAGE_PERCENT_REFUSAL)})) << "C++-dev: a % value is //damagepercent's";
	EXPECT_EQ(npc.getLifeStats()->getCurrentMp(), mp);
}

/** C++-dev: //damage is always points - 50 is 50 HP, not half (Java: dmg <= 100 was a percentage) - and refuses a % value */
TEST_F(MonsterCommandsTest, DamageIsAlwaysPoints) {
	Player& admin = gm(730211);
	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	handlers::admincommands::Damage damage;
	const int32_t maxHp = npc.getLifeStats()->getMaxHp();
	ASSERT_GT(maxHp, 200);
	EXPECT_TRUE(damage.process(admin, args({"50"})));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - 50) << "50 points";
	EXPECT_TRUE(damage.process(admin, args({"150"})));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - 200);
	client()->clearSent();
	EXPECT_TRUE(damage.process(admin, args({"50%"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message(DAMAGE_PERCENT_REFUSAL)}));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - 200) << "refused: no damage";
}

/** C++-dev: //damagepercent takes 0 to 100 percent of the max, with or without %, and refuses anything else with its syntax */
TEST_F(MonsterCommandsTest, DamagePercentTakesAPercentageOfTheMax) {
	Player& admin = gm(730212);
	handlers::admincommands::DamagePercent damagePercent;
	const std::vector<uint8_t> syntax = message(DAMAGE_PERCENT_SYNTAX);
	EXPECT_TRUE(damagePercent.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax}));
	client()->clearSent();
	EXPECT_TRUE(damagePercent.process(admin, args({"10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("No target selected")}));

	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	const int32_t maxHp = npc.getLifeStats()->getMaxHp();
	EXPECT_TRUE(damagePercent.process(admin, args({"50"})));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - maxHp / 2) << "(int) (50 / 100f * maxHp)";
	EXPECT_TRUE(damagePercent.process(admin, args({"10%"})));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - maxHp / 2 - maxHp / 10) << "a trailing % is accepted";
	EXPECT_TRUE(damagePercent.process(admin, args({"0"})));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - maxHp / 2 - maxHp / 10) << "0 is in range";
	const int32_t hp = npc.getLifeStats()->getCurrentHp();
	for (std::vector<std::string> params : {args({"101"}), args({"-1"}), args({"101%"}), args({"abc"}), args({"5%%"}), args({"mp"})}) {
		client()->clearSent();
		EXPECT_TRUE(damagePercent.process(admin, params));
		EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << params[0];
	}
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), hp) << "refused: no damage";
	client()->clearSent();
	EXPECT_TRUE(damagePercent.process(admin, args({"fp", "10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "fp needs a player";
	client()->clearSent();
	EXPECT_TRUE(damagePercent.process(admin, args({"dp", "10"})));
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "dp needs a player";
	EXPECT_EQ(damagePercent.getLevel(), handlers::admincommands::Damage().getLevel()) << "//damage's access level (commands.properties: damage)";
}

/** C++-dev: dp and fp on a player target - DP as a percentage of 4000, clamped at the player's DP (Java's DP clamp) */
TEST_F(MonsterCommandsTest, DamagePercentOnAPlayersDpAndFp) {
	Player& admin = gm(730213);
	admin.getCommonData()->setPlayerClass(model::PlayerClass::GLADIATOR); // setDp ignores a starting class
	admin.getCommonData()->setDp(3000);
	const int32_t dp = admin.getCommonData()->getDp();
	ASSERT_GT(dp, 0);
	target(admin, admin);
	handlers::admincommands::DamagePercent damagePercent;
	EXPECT_TRUE(damagePercent.process(admin, args({"dp", "25"})));
	EXPECT_EQ(admin.getCommonData()->getDp(), dp - std::min(dp, 1000)) << "25% of 4000, at most the DP";
	EXPECT_TRUE(damagePercent.process(admin, args({"DP", "100%"})));
	EXPECT_EQ(admin.getCommonData()->getDp(), 0) << "the clamp";
	{ // the flight time stat (max FP) of a real character: 60 s
		using model::stats::calc::functions::RcStatFunction;
		using model::stats::calc::functions::StatSetFunction;
		std::vector<runtime::Ref<model::stats::calc::functions::IStatFunction>> functions{
			RcStatFunction<StatSetFunction>::create(model::stats::container::StatEnum::FLY_TIME, 60)};
		std::vector<runtime::Ptr<model::stats::calc::functions::IStatFunction>> borrowed(functions.begin(), functions.end());
		admin.getGameStats()->addEffect(nullptr, borrowed);
	}
	const int32_t maxFp = admin.getLifeStats()->getMaxFp();
	ASSERT_GT(maxFp, 10);
	admin.getLifeStats()->setCurrentFp(maxFp);
	ASSERT_EQ(admin.getLifeStats()->getCurrentFp(), maxFp);
	EXPECT_TRUE(damagePercent.process(admin, args({"fp", "20"})));
	EXPECT_EQ(admin.getLifeStats()->getCurrentFp(), maxFp - static_cast<int32_t>(20 / 100.0f * static_cast<float>(maxFp))) << "(int) (20 / 100f * maxFp)";

	handlers::admincommands::Damage damage;
	admin.getCommonData()->setDp(3000);
	const int32_t points = admin.getCommonData()->getDp();
	EXPECT_TRUE(damage.process(admin, args({"dp", "50"})));
	EXPECT_EQ(admin.getCommonData()->getDp(), points - std::min(points, 50)) << "//damage dp 50 is 50 points";
	EXPECT_TRUE(damage.process(admin, args({"dp", "100000"})));
	EXPECT_EQ(admin.getCommonData()->getDp(), 0) << "//damage's DP clamp (Java): at most the DP";
}

// ---- //ai (Ai.java:37-128) ----------------------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, AiArms) {
	Player& admin = gm(730206);
	handlers::admincommands::Ai ai;
	const bool eventDebug = configs::main::AIConfig::EVENT_DEBUG.load();
	EXPECT_TRUE(ai.process(admin, args({"EventLog"})));
	EXPECT_EQ(client()->sentBytes(), info(std::string("New eventlog value: ") + (!eventDebug ? "true" : "false")));
	configs::main::AIConfig::EVENT_DEBUG.store(eventDebug);

	client()->clearSent();
	EXPECT_TRUE(ai.process(admin, args({"info"})));
	EXPECT_EQ(client()->sentBytes(), exactly({invalidTarget()})) << "no target";
	target(admin, admin);
	client()->clearSent();
	EXPECT_TRUE(ai.process(admin, args({"info"})));
	EXPECT_EQ(client()->sentBytes(), exactly({invalidTarget()})) << "a player";

	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	EXPECT_TRUE(ai.process(admin, args({"info"})));
	EXPECT_EQ(client()->sentBytes(), info("[AI info]\n\tName: " + npc.getAi().getName() + "\n\tState: " +
										  std::string(xml::enumName(npc.getAi().getState())) + "\n\tSubstate: " +
										  std::string(xml::enumName(npc.getAi().getSubState()))));

	client()->clearSent();
	const bool logging = npc.getAi().isLogging();
	EXPECT_TRUE(ai.process(admin, args({"log"})));
	EXPECT_EQ(npc.getAi().isLogging(), !logging);
	EXPECT_EQ(client()->sentBytes(), info(std::string("New log value: ") + (!logging ? "true" : "false")));

	client()->clearSent();
	EXPECT_TRUE(ai.process(admin, args({"set"})));
	EXPECT_EQ(client()->sentBytes(), info(ai.getSyntaxInfo())) << "set without a name";
	client()->clearSent();
	EXPECT_TRUE(ai.process(admin, args({"event2", "attacked"})));
	EXPECT_EQ(client()->sentBytes(), info("Please provide a valid creature object ID"));

	// the owner's correction of 2026-10-05: a visible object that is no creature (Java: ClassCastException, the error line)
	runtime::Ref<model::gameobjects::StaticObject> object = model::gameobjects::VisibleObject::create<model::gameobjects::StaticObject>(
		std::make_unique<controllers::StaticObjectController>(), *npc.getSpawn(), nullptr);
	world::World::getInstance().storeObject(*object);
	client()->clearSent();
	EXPECT_TRUE(ai.process(admin, args({"event2", "attacked", std::to_string(object->getObjectId())})));
	EXPECT_EQ(client()->sentBytes(), info("Please provide a valid creature object ID"));
	world::World::getInstance().removeObject(*object);
	client()->clearSent();
	EXPECT_TRUE(ai.process(admin, args({"state", "dizzy"})));
	EXPECT_NE(client()->sentBytes(), info(ai.getSyntaxInfo()));
	EXPECT_FALSE(client()->sentBytes().empty()) << "AIState.valueOf: \"Invalid ai state.\" and the constants";
}

// ---- //npcskill (NpcSkill.java:23-71) -----------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, NpcSkillWithoutSkills) {
	Player& admin = gm(730207);
	handlers::admincommands::NpcSkill npcSkill;
	EXPECT_TRUE(npcSkill.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("You should select a valid target first!")}));
	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	EXPECT_TRUE(npcSkill.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), exactly({message("This npc does not have any skills.")})) << "the fixture's empty NPC skill data";
}

// ---- //useskill (UseSkill.java:31-91) -----------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, UseSkillRefusals) {
	Player& admin = gm(730208);
	handlers::admincommands::UseSkill useSkill;
	EXPECT_TRUE(useSkill.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(useSkill.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(useSkill.process(admin, args({"abc"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid skill ID or level."));
	client()->clearSent();
	EXPECT_TRUE(useSkill.process(admin, args({"1234"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid skill ID.")) << "the fixture's empty skill data";
	client()->clearSent();
	EXPECT_TRUE(useSkill.process(admin, args({"ME"})));
	EXPECT_EQ(client()->sentBytes(), info(useSkill.getSyntaxInfo()))
		<< "the owner's correction of 2026-10-05: no skill ID after the target mode is the syntax (Java: params[1] out of bounds)";
}

// ---- //say (Say.java:22-35) ----------------------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, SayLetsTheNpcTargetSpeak) {
	std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
	levels["say"] = 3; // the GM tools' level (CommandTest restores the levels)
	configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
	Player& admin = gm(730209);
	handlers::admincommands::Say say;
	EXPECT_TRUE(say.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(say.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(say.process(admin, args({"hello"})));
	EXPECT_EQ(client()->sentBytes(), exactly({invalidTarget()})) << "no target";
	target(admin, admin);
	EXPECT_TRUE(say.process(admin, args({"hello"})));
	EXPECT_EQ(client()->sentBytes(), exactly({invalidTarget()})) << "a player target";

	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	EXPECT_TRUE(say.process(admin, args({"hello", "there"})));
	EXPECT_EQ(client()->sentBytes(), exactly({serialized(SM_MESSAGE(npc, "hello there", model::ChatType::NORMAL), client().con())}))
		<< "broadcast to the GM himself (toSelf)";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
