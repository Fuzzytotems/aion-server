// The stage-0 monster commands (m5j-plan.md §5.2): //spawn, //delete, //kill, //damage, //ai, //npcskill, //useskill (data/handlers/admincommands)
// and K-10's guard prefix of SpawnsData.saveSpawn (SpawnsData.java:205-214), on a real Player with a real AionConnection (CommandTestSupport.h)
// and npcs built as tests/cm_ak/EconomyPacketTestSupport.h builds them. The texts are the Java literals.

#include "CommandTestSupport.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
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
#include "aion/gameserver/handlers/admincommands/Delete.h"
#include "aion/gameserver/handlers/admincommands/Kill.h"
#include "aion/gameserver/handlers/admincommands/NpcSkill.h"
#include "aion/gameserver/handlers/admincommands/SpawnNpc.h"
#include "aion/gameserver/handlers/admincommands/UseSkill.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
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

TEST_F(MonsterCommandsTest, SaveSpawnRefusesWhatIsNoWorldSpawnAndReachesTheUnportedTailOtherwise) {
	Player& player = gm(730200);
	dataholders::SpawnsData& spawns = *dataholders::DataManager::SPAWNS_DATA;
	EXPECT_FALSE(spawns.saveSpawn(player, false)) << "no spawn template";
	EXPECT_FALSE(spawns.saveSpawn(npcAt(player, 101.0f, true, 300), false)) << "a special spawn (a SpawnTemplate subclass)";
	EXPECT_FALSE(spawns.saveSpawn(npcAt(player, 102.0f, false, 0), true)) << "a single time spawn (respawn time 0)";
	EXPECT_THROW(spawns.saveSpawn(npcAt(player, 103.0f, false, 300), false), runtime::UnportedException)
		<< "a world spawn reaches the file-I/O tail (m5j D7)";
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

// ---- //damage (Damage.java:26-113) --------------------------------------------------------------------------------------------------------

TEST_F(MonsterCommandsTest, DamageArms) {
	Player& admin = gm(730205);
	handlers::admincommands::Damage damage;
	const std::vector<uint8_t> syntax = message("syntax //damage (mp/fp/dp) <dmg | dmg%>\n<dmg> must be a number.\n"
												 "(mp/fp/dp) is optional, leave out to use HP damage\nin case of fp/dp, target must be player!");
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
	EXPECT_EQ(client()->sentBytes(), exactly({syntax})) << "the % group \"abc\" is no number";
	EXPECT_EQ(npc.getLifeStats()->getCurrentMp(), mp);
}

TEST_F(MonsterCommandsTest, DamageAPercentageOfTheHp) {
	Player& admin = gm(730211);
	Npc& npc = npcAt(admin, 101.0f);
	target(admin, npc);
	handlers::admincommands::Damage damage;
	const int32_t maxHp = npc.getLifeStats()->getMaxHp();
	EXPECT_TRUE(damage.process(admin, args({"50%"})));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - maxHp / 2) << "(int) (50 / 100f * maxHp)";
	EXPECT_TRUE(damage.process(admin, args({"10"})));
	EXPECT_EQ(npc.getLifeStats()->getCurrentHp(), maxHp - maxHp / 2 - maxHp / 10)
		<< "a value up to 100 is a percentage too (Java: dmg <= 100 -> isPercent; proposed correction)";
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

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
