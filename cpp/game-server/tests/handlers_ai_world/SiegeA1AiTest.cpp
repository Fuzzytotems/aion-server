// A1, lane B, the siege slice (docs/design/a1-laneb-plan.md): the siege AIs of handlers/ai/siege that phase 5 left - cp1: DredgionCommanderAI,
// FortressGateAI, IncarnateAI, MercenaryAI, MineAI, SiegeTeleporterAI, SpringAI - on npcs of the AI tests' Poeta map, each AI installed on its
// npc and driven through its events, as tests/handlers_ai_core/SiegeAiHandlersTest.cpp drives the leased ones.
//
// Not covered here (no player connection, no skill rows, no World registration of the test map): the dialog and question windows of
// FortressGateAI and MercenaryAI, the gate passage, the skills of MineAI and SpringAI, IncarnateAI's claw spawn and the mercenary spawn.
// The gate (gs.scenario.m5i) spawns the siege npcs of fortress 1131 with these AIs.

#include "../ai/AiWorldTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"
#include "aion/gameserver/handlers/ai/siege/DredgionCommanderAI.h"
#include "aion/gameserver/handlers/ai/siege/EmpoweredAgent.h"
#include "aion/gameserver/handlers/ai/siege/EnragedAgent.h"
#include "aion/gameserver/handlers/ai/siege/GateRepairAI.h"
#include "aion/gameserver/handlers/ai/siege/SiegeCannonAI.h"
#include "aion/gameserver/handlers/ai/siege/SiegeWeaponAI.h"
#include "aion/gameserver/handlers/ai/siege/FortressGateAI.h"
#include "aion/gameserver/handlers/ai/siege/IncarnateAI.h"
#include "aion/gameserver/handlers/ai/siege/MercenaryAI.h"
#include "aion/gameserver/handlers/ai/siege/MineAI.h"
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.h"
#include "aion/gameserver/handlers/ai/siege/SiegeTeleporterAI.h"
#include "aion/gameserver/handlers/ai/siege/SpringAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/controllers/SiegeWeaponController.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory).
namespace aion::gameserver::handlers::ai::siege {
::aion::gameserver::handlers::AIFactory DredgionCommanderAI_aiFactory;
::aion::gameserver::handlers::AIFactory FortressGateAI_aiFactory;
::aion::gameserver::handlers::AIFactory IncarnateAI_aiFactory;
::aion::gameserver::handlers::AIFactory MercenaryAI_aiFactory;
::aion::gameserver::handlers::AIFactory MineAI_aiFactory;
::aion::gameserver::handlers::AIFactory SiegeTeleporterAI_aiFactory;
::aion::gameserver::handlers::AIFactory SpringAI_aiFactory;
::aion::gameserver::handlers::AIFactory EmpoweredAgent_aiFactory;
::aion::gameserver::handlers::AIFactory EnragedAgent_aiFactory;
::aion::gameserver::handlers::AIFactory GateRepairAI_aiFactory;
::aion::gameserver::handlers::AIFactory SiegeCannonAI_aiFactory;
::aion::gameserver::handlers::AIFactory SiegeWeaponAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::siege

namespace aion::gameserver::services {

/** The service's active sieges (SiegeService.h befriends this struct for the tests) */
struct SiegeServiceTestAccess {
	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<siege::Siege>>& activeSieges(SiegeService& service) { return service.activeSieges; }
};

} // namespace aion::gameserver::services

namespace aion::gameserver::ai::testing {
namespace {

namespace sieges = gameserver::handlers::ai::siege;

using model::gameobjects::siege::SiegeNpc;
using model::siege::SiegeModType;
using model::siege::SiegeRace;
using poll::AIQuestion;
using runtime::Ptr;
using runtime::Ref;

constexpr int32_t FORTRESS_ID = 1011;
constexpr int32_t SIEGE_NPC_ID = 252001;
constexpr int32_t FORTRESS_BOSS_NPC_ID = 252002;
constexpr int32_t OTHER_NPC_ID = 252003;

/** Test rows of npc_templates.xml: a plain siege npc, a GCHIEF_LIGHT fortress boss and a DRAGON npc beside him */
const dataholders::NpcData& siegeA1NpcTemplates() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		const auto row = [](int32_t id, const char* race) {
			return std::string(R"(<npc_template npc_id=")") + std::to_string(id) + R"(" name_id="1" level="65" name="siege npc )" +
				std::to_string(id) + R"(" attack_speed="2000" tribe="MONSTER" rating="NORMAL" rank="NOVICE" race=")" + race +
				R"(" srange="8" sangle="270" arange="2"><stats maxHp="1000" maxMp="0" attack="10">)" +
				R"(<speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)";
		};
		return xml::bindString<dataholders::NpcData>(context, "<npc_templates>" + row(SIEGE_NPC_ID, "DRAGON") +
			row(FORTRESS_BOSS_NPC_ID, "GCHIEF_LIGHT") + row(OTHER_NPC_ID, "DRAGON") + "</npc_templates>")
			.release();
	}();
	return *holder;
}

// the protected handlers the cases drive directly (Java: the AI's events)
struct DredgionCommanderProbe : sieges::DredgionCommanderAI {
	using DredgionCommanderAI::DredgionCommanderAI;
	using DredgionCommanderAI::handleSpawned;
	using DredgionCommanderAI::handleDied;
};
struct MineProbe : sieges::MineAI {
	using MineAI::MineAI;
	using MineAI::handleCreatureAggro;
};
struct TeleporterProbe : sieges::SiegeTeleporterAI {
	using SiegeTeleporterAI::SiegeTeleporterAI;
	using SiegeTeleporterAI::handleSpawned;
	using SiegeTeleporterAI::handleDied;
	using SiegeTeleporterAI::handleDespawned;
};

struct EmpoweredProbe : sieges::EmpoweredAgent {
	using EmpoweredAgent::EmpoweredAgent;
	using EmpoweredAgent::handleSpawned;
	using EmpoweredAgent::handleDied;
};
struct EnragedProbe : sieges::EnragedAgent {
	using EnragedAgent::EnragedAgent;
	using EnragedAgent::handleDied;
};
struct GateRepairProbe : sieges::GateRepairAI {
	using GateRepairAI::GateRepairAI;
	using GateRepairAI::handleDialogStart;
};
struct SiegeWeaponProbe : sieges::SiegeWeaponAI {
	using SiegeWeaponAI::SiegeWeaponAI;
	using SiegeWeaponAI::handleSpawned;
	using SiegeWeaponAI::handleFollowMe;
	using SiegeWeaponAI::handleStopFollowMe;
	using SiegeWeaponAI::handleAttack;
};

/** A Siege that records its stop (SiegeAiHandlersTest.cpp's) */
class StoppableSiege final : public services::siege::Siege {
	AION_MAKE_REF_FRIEND
public:
	explicit StoppableSiege(model::siege::SiegeLocation& location) : Siege(location) {}

	void onSiegeStart() override {}
	void onSiegeFinish() override { finishes++; }
	bool isEndless() override { return true; }
	void onAbyssPointsAdded(model::gameobjects::player::Player&, int32_t) override {}

	int32_t finishes = 0;

protected:
	~StoppableSiege() override = default;
};

class SiegeA1AiTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		AI_TEST_SCOPE;
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady();
		fortressTemplate = xml::bindString<model::templates::siegelocation::SiegeLocationTemplate>(
			siegeContext, R"(<siege_location id="1011" type="FORTRESS" world="210010000" siege_duration="3600"/>)");
		fortress = model::siege::FortressLocation::create(fortressTemplate.get());
		services::SiegeService::getInstance().getFortresses().put(FORTRESS_ID, fortress);
		services::SiegeService::getInstance().getSiegeLocations().put(FORTRESS_ID, fortress);
		savedFortressMultiplier = configs::main::SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load();
		configs::main::SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.store(2.5f);
	}

	void TearDown() override {
		{
			AI_TEST_SCOPE;
			services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance()).remove(FORTRESS_ID);
			services::SiegeService::getInstance().getFortresses().remove(FORTRESS_ID);
			services::SiegeService::getInstance().getSiegeLocations().remove(FORTRESS_ID);
		}
		configs::main::SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.store(savedFortressMultiplier);
		if (skillsPublished)
			dataholders::DataManager::SKILL_DATA.resetForTests();
		summons.clear();
		siegeNpcs.clear();
		siegeSpawns.clear();
		siegeGroups.clear();
		fortress = nullptr;
		regionActivator = nullptr;
		AiWorldTest::TearDown();
	}

	/** A SiegeNpc of fortress 1011 on a siege spawn, placed in the map like makeWorldNpc places a plain npc */
	Ref<SiegeNpc> makeSiegeNpc(int32_t npcId, float x = 501, float y = 500, float z = 100) {
		Ref<model::templates::spawns::SpawnGroup> siegeGroup = model::templates::spawns::SpawnGroup::create(POETA, npcId, 0, nullptr);
		Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> spawn = model::templates::spawns::siegespawns::SiegeSpawnTemplate::create(
			FORTRESS_ID, SiegeRace::BALAUR, SiegeModType::SIEGE, *siegeGroup, x, y, z, int8_t{0}, 0, std::nullopt, 0);
		Ref<SiegeNpc> npc = model::gameobjects::VisibleObject::create<SiegeNpc>(std::make_unique<controllers::NpcController>(), *spawn,
			siegeA1NpcTemplates().getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		siegeGroups.push_back(siegeGroup);
		siegeSpawns.push_back(spawn);
		siegeNpcs.push_back(npc);
		return npc;
	}

	template <class AI, class Owner>
	AI& install(Owner& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	/** SKILL_DATA with one plain template per id (I5AiTest.cpp's); TearDown resets it */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="siege" nameId="1" stack="SIEGE" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
		skillsPublished = true;
	}

	float maxHpBaseRate(AbstractAI& ai, model::gameobjects::Npc& owner) {
		model::stats::calc::AdditionStat stat(model::stats::container::StatEnum::MAXHP, 1000, owner);
		ai.modifyOwnerStat(stat);
		return stat.getBaseRate();
	}

	xml::LoadContext siegeContext;
	xml::LoadContext skillContext;
	bool skillsPublished = false;
	std::unique_ptr<model::templates::siegelocation::SiegeLocationTemplate> fortressTemplate;
	Ref<model::siege::FortressLocation> fortress;
	Ref<model::gameobjects::player::Player> regionActivator;
	std::vector<Ref<model::templates::spawns::SpawnGroup>> siegeGroups;
	std::vector<Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate>> siegeSpawns;
	std::vector<Ref<SiegeNpc>> siegeNpcs;
	std::vector<Ref<model::gameobjects::Summon>> summons;
	std::unique_ptr<model::templates::siegelocation::SiegeLocationTemplate> repairTemplate;
	float savedFortressMultiplier = 0;

	/** A siege weapon of the master: a Summon with its SiegeWeaponController (SummonsService.createSummon's controller for a siege weapon) */
	Ref<model::gameobjects::Summon> makeSiegeWeapon(model::gameobjects::player::Player& master) {
		Ref<model::gameobjects::Summon> summon = model::gameobjects::VisibleObject::create<model::gameobjects::Summon>(
			utils::idfactory::IDFactory::getInstance().nextId(), std::make_unique<controllers::SiegeWeaponController>(SIEGE_NPC_ID),
			makeSpawn(SIEGE_NPC_ID, 502, 500, 100), siegeA1NpcTemplates().getNpcTemplate(SIEGE_NPC_ID), master, 0);
		summon->setKnownlist(std::make_unique<world::knownlist::KnownList>(*summon));
		place(*summon, 502, 500, 100);
		summons.push_back(summon);
		return summon;
	}
};

/** The AION_AI factories of cp1 build their classes on the bases Java gives them (the names are the registry scan's check) */
TEST_F(SiegeA1AiTest, TheFactoriesBuildTheirClasses) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	std::unique_ptr<AbstractAI> dredgion = sieges::DredgionCommanderAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::DredgionCommanderAI*>(dredgion.get()));
	EXPECT_TRUE(dynamic_cast<sieges::SiegeNpcAI*>(dredgion.get()));
	std::unique_ptr<AbstractAI> gate = sieges::FortressGateAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::FortressGateAI*>(gate.get()));
	EXPECT_FALSE(dynamic_cast<sieges::SiegeNpcAI*>(gate.get())) << "a plain NpcAI";
	std::unique_ptr<AbstractAI> incarnate = sieges::IncarnateAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::IncarnateAI*>(incarnate.get()));
	EXPECT_TRUE(dynamic_cast<sieges::SiegeNpcAI*>(incarnate.get()));
	std::unique_ptr<AbstractAI> mercenary = sieges::MercenaryAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::MercenaryAI*>(mercenary.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::GeneralNpcAI*>(mercenary.get()));
	std::unique_ptr<AbstractAI> mine = sieges::MineAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::MineAI*>(mine.get()));
	EXPECT_TRUE(dynamic_cast<sieges::SiegeNpcAI*>(mine.get()));
	std::unique_ptr<AbstractAI> teleporter = sieges::SiegeTeleporterAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::SiegeTeleporterAI*>(teleporter.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::GeneralNpcAI*>(teleporter.get()));
	std::unique_ptr<AbstractAI> spring = sieges::SpringAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::SpringAI*>(spring.get()));
	EXPECT_FALSE(dynamic_cast<gameserver::handlers::ai::GeneralNpcAI*>(spring.get())) << "a plain NpcAI";
}

/** FortressGateAI.java:48-55: AP and the shared rewards, no loot, no respawn */
TEST_F(SiegeA1AiTest, AGateRewardsApButNoLootAndDoesNotRespawn) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	sieges::FortressGateAI& ai = install<sieges::FortressGateAI>(*npc);
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_TRUE(ai.ask(AIQuestion::ALLOW_DECAY)) << "NpcAI's";
}

/** SpringAI.java:45-51: no decay, rewards or loot */
TEST_F(SiegeA1AiTest, ASpringNeitherDecaysNorRewards) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	sieges::SpringAI& ai = install<sieges::SpringAI>(*npc);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::ALLOW_RESPAWN)) << "NpcAI's";
}

/** SiegeNpcAI's questions reach the mine, the incarnate and the commander */
TEST_F(SiegeA1AiTest, TheSiegeNpcsKeepSiegeNpcAIsQuestions) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> mine = makeSiegeNpc(SIEGE_NPC_ID);
	Ref<SiegeNpc> incarnate = makeSiegeNpc(SIEGE_NPC_ID, 502, 500, 100);
	Ref<SiegeNpc> commander = makeSiegeNpc(SIEGE_NPC_ID, 503, 500, 100);
	for (AbstractAI* ai : std::vector<AbstractAI*>{&install<sieges::MineAI>(*mine), &install<sieges::IncarnateAI>(*incarnate),
		     &install<sieges::DredgionCommanderAI>(*commander)}) {
		EXPECT_FALSE(ai->ask(AIQuestion::ALLOW_DECAY));
		EXPECT_FALSE(ai->ask(AIQuestion::ALLOW_RESPAWN));
		EXPECT_FALSE(ai->ask(AIQuestion::REWARD_LOOT));
	}
}

/** MineAI.java:19-29: the aggro deletes the mine 1.5 s later */
TEST_F(SiegeA1AiTest, AMineGoesOffAndIsDeleted) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> mine = makeSiegeNpc(SIEGE_NPC_ID);
	publishSkills({18407});
	MineProbe& ai = install<MineProbe>(*mine);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7101, 502, 500, 100);
	world::World::getInstance().storeObject(*mine); // as VisibleObjectSpawner stores a spawn
	ai.handleCreatureAggro(*player);
	executor->advance(std::chrono::milliseconds(1499));
	EXPECT_TRUE(world::World::getInstance().findVisibleObject(mine->getObjectId()));
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(mine->getObjectId())) << "AIActions.deleteOwner: World.removeObject";
	EXPECT_FALSE(mine->isSpawned());
}

/** SiegeTeleporterAI.java:23-51: its spawn opens the fortress's teleport, its death and its despawn close it */
TEST_F(SiegeA1AiTest, ATeleporterOpensAndClosesTheFortressTeleport) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	TeleporterProbe& ai = install<TeleporterProbe>(*npc);
	fortress->setCanTeleport(false);
	ai.handleSpawned();
	EXPECT_TRUE(fortress->isCanTeleport(nullptr));
	ai.handleDied();
	EXPECT_FALSE(fortress->isCanTeleport(nullptr));
	fortress->setCanTeleport(true);
	ai.handleDespawned();
	EXPECT_FALSE(fortress->isCanTeleport(nullptr));
}

/** DredgionCommanderAI.java:31-45, 57-68: 3 s after its spawn it hates the fortress boss it knows, hits him harder and has more max hp */
TEST_F(SiegeA1AiTest, ADredgionCommanderGoesForTheFortressBoss) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> commander = makeSiegeNpc(SIEGE_NPC_ID);
	Ref<SiegeNpc> boss = makeSiegeNpc(FORTRESS_BOSS_NPC_ID, 503, 500, 100);
	Ref<SiegeNpc> other = makeSiegeNpc(OTHER_NPC_ID, 502, 500, 100);
	know(*commander, *other);
	know(*commander, *boss);
	DredgionCommanderProbe& ai = install<DredgionCommanderProbe>(*commander);
	EXPECT_FLOAT_EQ(maxHpBaseRate(ai, *commander), 2.5f);
	ai.handleSpawned();
	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_FALSE(commander->getAggroList().isHating(*boss));
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(commander->getAggroList().getHate(*boss), 500000);
	EXPECT_FALSE(commander->getAggroList().isHating(*other)) << "only a GCHIEF_LIGHT or GCHIEF_DARK npc";
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(40.0f, *boss, nullptr), 100.0f);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(40.0f, *other, nullptr), 40.0f);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::MAIN_HAND_POWER, 100, *commander);
	ai.modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f) << "only MAXHP";
}

/** DredgionCommanderAI.java:76-83: without a fortress assault its death forgets the boss */
TEST_F(SiegeA1AiTest, ADredgionCommandersDeathForgetsTheBoss) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> commander = makeSiegeNpc(SIEGE_NPC_ID);
	Ref<SiegeNpc> boss = makeSiegeNpc(FORTRESS_BOSS_NPC_ID, 503, 500, 100);
	know(*commander, *boss);
	DredgionCommanderProbe& ai = install<DredgionCommanderProbe>(*commander);
	ai.handleSpawned();
	executor->advance(std::chrono::milliseconds(3000));
	ASSERT_FLOAT_EQ(ai.modifyOwnerDamage(40.0f, *boss, nullptr), 100.0f);
	ai.handleDied();
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(40.0f, *boss, nullptr), 40.0f);
}

/** MercenaryAI.java:46-174, 176-190: a selection answers true even when the fortress has no siege (no mercenary location) */
TEST_F(SiegeA1AiTest, AMercenarySelectionWithoutASiegeDoesNothing) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	sieges::MercenaryAI& ai = install<sieges::MercenaryAI>(*npc);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7102, 502, 500, 100);
	EXPECT_TRUE(ai.onDialogSelect(*player, model::DialogAction::SETPRO1, 0, 0));
	EXPECT_TRUE(ai.onDialogSelect(*player, model::DialogAction::SETPRO3, 0, 0));
}

// ------------------------------------------------------------------------------------------------------------------------------- cp2

/** The AION_AI factories of cp2 (the siege weapon's on a Summon, its owner type) */
TEST_F(SiegeA1AiTest, TheCp2FactoriesBuildTheirClasses) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	std::unique_ptr<AbstractAI> enraged = sieges::EnragedAgent_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::EnragedAgent*>(enraged.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::SummonerAI*>(enraged.get()));
	std::unique_ptr<AbstractAI> repair = sieges::GateRepairAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::GateRepairAI*>(repair.get()));
	std::unique_ptr<AbstractAI> cannon = sieges::SiegeCannonAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::SiegeCannonAI*>(cannon.get()));
	Ref<model::gameobjects::player::Player> master = makeWorldPlayer(7201, 502, 500, 100);
	Ref<model::gameobjects::Summon> weapon = makeSiegeWeapon(*master);
	std::unique_ptr<AbstractAI> weaponAi = sieges::SiegeWeaponAI_aiFactory(*weapon);
	EXPECT_TRUE(dynamic_cast<sieges::SiegeWeaponAI*>(weaponAi.get()));
}

/** EnragedAgent.java:40-56: max hp by the fortress protector multiplier; back home forgets the aggro list; its death ends the siege */
TEST_F(SiegeA1AiTest, AnEnragedAgentEndsTheSiegeLikeAProtector) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> agent = makeSiegeNpc(SIEGE_NPC_ID);
	EnragedProbe& ai = install<EnragedProbe>(*agent);
	EXPECT_FLOAT_EQ(maxHpBaseRate(ai, *agent), 2.5f);
	Ref<model::gameobjects::player::Player> elyos = makeWorldPlayer(7202, 502, 500, 100);
	know(*agent, *elyos);
	agent->getAggroList().addDamage(*elyos, 300, false, std::nullopt);
	ai.handleBackHome();
	EXPECT_TRUE(agent->getAggroList().stream().empty()) << "old damages are not counted in stopSiege";
	Ref<StoppableSiege> siege = runtime::makeRef<StoppableSiege>(*fortress);
	services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance()).put(FORTRESS_ID, siege);
	agent->getAggroList().addDamage(*elyos, 200, false, std::nullopt);
	ai.handleDied();
	EXPECT_EQ(siege->getSiegeCounter()->getRaceCounter(SiegeRace::ELYOS)->getTotalDamage(), 200);
	EXPECT_TRUE(siege->isBossKilled());
	EXPECT_EQ(siege->finishes, 1);
}

/** SiegeCannonAI.java:30-41, 43-131, 133-139: no morph skill for another npc id (SETPRO1 does nothing then), no respawn, no loot */
TEST_F(SiegeA1AiTest, ASiegeCannonWithoutAMorphSkill) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	sieges::SiegeCannonAI& ai = install<sieges::SiegeCannonAI>(*npc);
	EXPECT_EQ(ai.getMorphSkill(), 0) << "not a cannon id";
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT)) << "NpcAI's";
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7203, 502, 500, 100);
	world::World::getInstance().storeObject(*npc);
	EXPECT_TRUE(ai.onDialogSelect(*player, model::DialogAction::SETPRO1, 0, 0));
	EXPECT_TRUE(world::World::getInstance().findVisibleObject(npc->getObjectId())) << "no morph skill: the cannon stays";
	world::World::getInstance().removeObject(*npc);
}

/** GateRepairAI.java:47-71: without the fortress's door repair data the stone asks nothing; with it, it asks STR_ASK_DOOR_REPAIR_POPUPDIALOG */
TEST_F(SiegeA1AiTest, AGateRepairStoneAsksWhenItsFortressRepairsDoors) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> stone = makeSiegeNpc(SIEGE_NPC_ID);
	GateRepairProbe& ai = install<GateRepairProbe>(*stone);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7204, 502, 500, 100);
	ai.handleDialogStart(*player);
	EXPECT_FALSE(player->getResponseRequester().remove(160027)) << "no door_repair_data: no question";
	repairTemplate = xml::bindString<model::templates::siegelocation::SiegeLocationTemplate>(siegeContext,
		R"(<siege_location id="1011" type="FORTRESS" world="210010000" siege_duration="3600">)"
		R"(<door_repair_data item_id="188030000" count="5" cd="30"><door_repair_stone static_id="199" door_id="53" /></door_repair_data>)"
		R"(</siege_location>)");
	Ref<model::siege::FortressLocation> repairing = model::siege::FortressLocation::create(repairTemplate.get());
	services::SiegeService::getInstance().getFortresses().put(FORTRESS_ID, repairing);
	ai.handleDialogStart(*player);
	EXPECT_TRUE(player->getResponseRequester().remove(160027)) << "the first question is pending";
}

/** SiegeWeaponAI.java:36-107: immune to abnormal states; following on request; a null target is ignored; no npc skills fail the spawn */
TEST_F(SiegeA1AiTest, ASiegeWeaponFollowsOnRequest) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::player::Player> master = makeWorldPlayer(7205, 502, 500, 100);
	Ref<model::gameobjects::Summon> weapon = makeSiegeWeapon(*master);
	SiegeWeaponProbe& ai = install<SiegeWeaponProbe>(*weapon);
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY)) << "AITemplate's";
	ai.handleFollowMe(*master);
	EXPECT_EQ(ai.getState(), AIState::FOLLOWING);
	ai.handleStopFollowMe(*master);
	EXPECT_EQ(ai.getState(), AIState::IDLE);
	ai.handleAttack(nullptr); // a null creature returns at once
	EXPECT_THROW(ai.handleSpawned(), runtime::Exception) << "the test npc has no npc skills: Java's getNpcSkills() on null";
}

/**
 * SiegeWeaponController.java:47-70: guard mode follows the master, and attack mode follows a valid target. Each adds
 * FollowStartService.newFollowingToTargetCheckTask to the master's controller as SUMMON_FOLLOW. Before P4-last's cp4 both modes reached the
 * AION_UNPORTED stand-in standins::followStartServiceNewFollowingToTargetCheckTask and threw.
 */
TEST_F(SiegeA1AiTest, ASiegeWeaponsGuardAndAttackModesScheduleTheFollowTaskOnItsMaster) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::player::Player> master = makeWorldPlayer(7206, 502, 500, 100);
	master->getCommonData()->setRace(model::Race::ASMODIANS); // a GCHIEF_LIGHT boss is a valid target of an Asmodian's weapon
	Ref<model::gameobjects::Summon> weapon = makeSiegeWeapon(*master);
	weapon->setEffectController(std::make_unique<controllers::effect::EffectController>(*weapon)); // the move task reads it
	install<SiegeWeaponProbe>(*weapon);
	ASSERT_FALSE(master->getController().hasTask(model::TaskId::SUMMON_FOLLOW));

	ASSERT_NO_THROW(weapon->getController().guardMode());
	EXPECT_TRUE(master->getController().hasTask(model::TaskId::SUMMON_FOLLOW)) << "the follow task of the master";
	EXPECT_EQ(weapon->getTarget().get(), master.get());
	executor->advance(std::chrono::milliseconds(1000)); // the first run: the weapon stands at its master, nothing to do
	EXPECT_TRUE(master->getController().hasTask(model::TaskId::SUMMON_FOLLOW)) << "a fixed-rate task stays scheduled";
	EXPECT_EQ(weapon->getMaster().get(), master.get()) << "in its master's range: not released for DISTANCE";

	Ref<SiegeNpc> boss = makeSiegeNpc(FORTRESS_BOSS_NPC_ID, 504, 500, 100);
	know(*weapon, *boss);
	master->getController().cancelTask(model::TaskId::SUMMON_FOLLOW);
	ASSERT_FALSE(master->getController().hasTask(model::TaskId::SUMMON_FOLLOW));
	ASSERT_NO_THROW(weapon->getController().attackMode(boss->getObjectId()));
	EXPECT_EQ(weapon->getTarget().get(), boss.get());
	EXPECT_TRUE(master->getController().hasTask(model::TaskId::SUMMON_FOLLOW)) << "the follow task of the target";

	master->getController().cancelTask(model::TaskId::SUMMON_FOLLOW);
}

// ------------------------------------------------------------------------------------------------------------------------------- cp3

/** EmpoweredAgent.java:58-61, 219-231: a protector that neither respawns nor loses its effects, max hp by the fortress protector multiplier */
TEST_F(SiegeA1AiTest, AnEmpoweredAgentIsAProtector) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	std::unique_ptr<AbstractAI> made = sieges::EmpoweredAgent_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::EmpoweredAgent*>(made.get()));
	EXPECT_TRUE(dynamic_cast<sieges::AbstractSiegeProtectorAI*>(made.get()));
	EmpoweredProbe& ai = install<EmpoweredProbe>(*npc);
	EXPECT_TRUE(ai.canThink()) << "until its spawn";
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY)) << "SiegeNpcAI's";
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_FLOAT_EQ(maxHpBaseRate(ai, *npc), 2.5f);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::MAIN_HAND_POWER, 100, *npc);
	ai.modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f) << "only MAXHP";
}

/** EmpoweredAgent.java:63-79, 94-100: spawned it stops thinking; the 25 and 5 % phases queue 21778 on itself */
TEST_F(SiegeA1AiTest, AnEmpoweredAgentWaitsAndQueuesItsSkill) {
	AI_TEST_SCOPE;
	publishSkills({21779, 21778});
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID); // not one of the two agents: no walk, no guards
	EmpoweredProbe& ai = install<EmpoweredProbe>(*npc);
	ai.handleSpawned();
	EXPECT_FALSE(ai.canThink()) << "it thinks again when the fight starts";
	const auto queued = [&](int32_t skillId) {
		return npc->hasQueuedSkill([skillId](model::skill::NpcSkillEntry& entry) { return entry.getSkillId() == skillId; });
	};
	EXPECT_FALSE(queued(21778));
	EXPECT_THROW(ai.handleHpPhase(30), runtime::Exception) << "30 % brings guards: guardIds is empty for this npc, Rnd.get(List) fails";
	EXPECT_FALSE(queued(21778)) << "30 % does not queue the skill";
	ai.handleHpPhase(25);
	EXPECT_TRUE(queued(21778));
}

/** EmpoweredAgent.java:204-210: a death without its AgentSiege is Java's NullPointerException of the cast */
TEST_F(SiegeA1AiTest, AnEmpoweredAgentsDeathNeedsItsAgentSiege) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SIEGE_NPC_ID);
	EmpoweredProbe& ai = install<EmpoweredProbe>(*npc);
	try {
		ai.handleDied();
		ADD_FAILURE() << "no exception";
	} catch (const runtime::Exception& e) {
		EXPECT_NE(std::string(e.what()).find("no AgentSiege"), std::string::npos) << e.what();
	}
}

} // namespace
} // namespace aion::gameserver::ai::testing
