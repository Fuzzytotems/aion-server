// M5i I-2 (m5i-plan.md 15.3): the siege AIs of D5's stage 1 - SiegeNpcAI and AbstractSiegeProtectorAI (no names), ArtifactProtectorAI
// ("artifact_protector"), FortressProtectorNpcAI ("fortress_protector"), GuardianGeneralAI ("guardian_general"), SiegeRaceProtectorAI
// ("siege_raceprotector") and ShieldNpcAI ("siege_shieldnpc") - of chunk A1, under P5-12a's lease of handlers/ai/siege (chunks.cmake).
//
// Java: data/handlers/ai/siege/SiegeNpcAI.java:21-36, AbstractSiegeProtectorAI.java:15-32, ArtifactProtectorAI.java:19-23,
// FortressProtectorNpcAI.java:22-33, GuardianGeneralAI.java, SiegeRaceProtectorAI.java:17-23, ShieldNpcAI.java:21-52.
//
// LINK WORKAROUND (the one TravelAiHandlersTest.cpp describes): A1's handler library is not linked into this executable, so this file compiles
// the leased sources itself; it is the only translation unit that does.
#include "aion/gameserver/handlers/ai/siege/SiegeNpcAI.cpp"
#include "aion/gameserver/handlers/ai/siege/AbstractSiegeProtectorAI.cpp"
#include "aion/gameserver/handlers/ai/siege/ArtifactProtectorAI.cpp"
#include "aion/gameserver/handlers/ai/siege/FortressProtectorNpcAI.cpp"
#include "aion/gameserver/handlers/ai/siege/GuardianGeneralAI.cpp"
#include "aion/gameserver/handlers/ai/siege/SiegeRaceProtectorAI.cpp"
#include "aion/gameserver/handlers/ai/siege/ShieldNpcAI.cpp"
// M5i I-8 (P5-12b1 lease of A1's files): the base boss and the siege base flag
#include "aion/gameserver/handlers/ai/siege/BaseProtectorAI.cpp"
#include "aion/gameserver/handlers/ai/siege/FlagBaseNpcAI.cpp"

#include "../ai/AiWorldTestSupport.h"

// The siege npcs are SiegeNpcs on SiegeSpawnTemplates of fortress 1011, which the cases put into SiegeService's maps as initSieges would (sieges
// stay disabled); a protector's death is driven through a recording Siege placed among the service's active sieges, as a FortressSiege would be.
// Not covered here: the spawn of a siege's npcs by SiegeService.spawnNpcs and a real FortressSiege's end (the gate's, I-6).

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
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory).
namespace aion::gameserver::handlers::ai::siege {
::aion::gameserver::handlers::AIFactory ArtifactProtectorAI_aiFactory;
::aion::gameserver::handlers::AIFactory FortressProtectorNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory GuardianGeneralAI_aiFactory;
::aion::gameserver::handlers::AIFactory SiegeRaceProtectorAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShieldNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory BaseProtectorAI_aiFactory;
::aion::gameserver::handlers::AIFactory FlagBaseNpcAI_aiFactory;
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
constexpr int32_t GUARDIAN_NPC_ID = 251001;
constexpr int32_t ARTIFACT_PROTECTOR_NPC_ID = 251002;
constexpr int32_t ARTIFACT_PROTECTOR_64_NPC_ID = 251003;
constexpr int32_t NORMAL_PROTECTOR_NPC_ID = 251004;
constexpr int32_t SHIELD_NPC_ID = 251005;
constexpr int32_t FIELD_STONE_NPC_ID = 251006;

/** Test rows of npc_templates.xml: a legendary and a normal guardian deity, artifact protectors of levels 65 and 64, a shield and a field stone */
const dataholders::NpcData& siegeNpcTemplates() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		const auto row = [](int32_t id, const char* level, const char* rating, const char* race, const char* ai) {
			return std::string(R"(<npc_template npc_id=")") + std::to_string(id) + R"(" name_id="1" level=")" + level + R"(" name="siege npc )" +
				std::to_string(id) + R"(" attack_speed="2000" tribe="MONSTER" rating=")" + rating + R"(" rank="NOVICE" race=")" + race + R"(" ai=")" +
				ai + R"(" srange="8" sangle="270" arange="2"><stats maxHp="1000" maxMp="0" attack="10")" +
				R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)";
		};
		return xml::bindString<dataholders::NpcData>(context,
			"<npc_templates>" + row(GUARDIAN_NPC_ID, "65", "LEGENDARY", "DRAGON", "fortress_protector") +
				row(ARTIFACT_PROTECTOR_NPC_ID, "65", "HERO", "DRAGON", "artifact_protector") +
				row(ARTIFACT_PROTECTOR_64_NPC_ID, "64", "HERO", "DRAGON", "artifact_protector") +
				row(NORMAL_PROTECTOR_NPC_ID, "65", "HERO", "DRAGON", "fortress_protector") +
				row(SHIELD_NPC_ID, "65", "NORMAL", "DRAGON", "siege_shieldnpc") + row(FIELD_STONE_NPC_ID, "65", "NORMAL", "CONSTRUCT", "siege_shieldnpc") +
				"</npc_templates>")
			.release();
	}();
	return *holder;
}

/** A Siege that records its stop */
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

class SiegeAiHandlersTest : public AiWorldTest {
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
		savedArtifactMultiplier = configs::main::SiegeConfig::ARTIFACT_PROTECTOR_HEALTH_MULTIPLIER.load();
		savedFortressMultiplier = configs::main::SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load();
		configs::main::SiegeConfig::ARTIFACT_PROTECTOR_HEALTH_MULTIPLIER.store(1.5f);
		configs::main::SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.store(2.5f);
		savedAutoAssault = configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.load();
		configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.store(false);
	}

	void TearDown() override {
		{
			AI_TEST_SCOPE;
			services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance()).remove(FORTRESS_ID);
			services::SiegeService::getInstance().getFortresses().remove(FORTRESS_ID);
			services::SiegeService::getInstance().getSiegeLocations().remove(FORTRESS_ID);
		}
		configs::main::SiegeConfig::ARTIFACT_PROTECTOR_HEALTH_MULTIPLIER.store(savedArtifactMultiplier);
		configs::main::SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.store(savedFortressMultiplier);
		configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.store(savedAutoAssault);
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
			siegeNpcTemplates().getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		siegeGroups.push_back(siegeGroup);
		siegeSpawns.push_back(spawn);
		siegeNpcs.push_back(npc);
		return npc;
	}

	template <class AI>
	AI& install(model::gameobjects::Npc& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	float maxHpBaseRate(AbstractAI& ai, model::gameobjects::Npc& owner) {
		model::stats::calc::AdditionStat stat(model::stats::container::StatEnum::MAXHP, 1000, owner);
		ai.modifyOwnerStat(stat);
		return stat.getBaseRate();
	}

	xml::LoadContext siegeContext;
	std::unique_ptr<model::templates::siegelocation::SiegeLocationTemplate> fortressTemplate;
	Ref<model::siege::FortressLocation> fortress;
	Ref<model::gameobjects::player::Player> regionActivator;
	std::vector<Ref<model::templates::spawns::SpawnGroup>> siegeGroups;
	std::vector<Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate>> siegeSpawns;
	std::vector<Ref<SiegeNpc>> siegeNpcs;
	float savedArtifactMultiplier = 0;
	float savedFortressMultiplier = 0;
	bool savedAutoAssault = false;
};

TEST_F(SiegeAiHandlersTest, TheNamedFactoriesBuildTheirAIs) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(GUARDIAN_NPC_ID);
	std::unique_ptr<AbstractAI> artifact = sieges::ArtifactProtectorAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::ArtifactProtectorAI*>(artifact.get()));
	EXPECT_TRUE(dynamic_cast<sieges::AbstractSiegeProtectorAI*>(artifact.get()));
	std::unique_ptr<AbstractAI> fortressAi = sieges::FortressProtectorNpcAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::FortressProtectorNpcAI*>(fortressAi.get()));
	std::unique_ptr<AbstractAI> general = sieges::GuardianGeneralAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::GuardianGeneralAI*>(general.get()));
	EXPECT_FALSE(dynamic_cast<sieges::AbstractSiegeProtectorAI*>(general.get())) << "a SiegeNpcAI, not a protector";
	std::unique_ptr<AbstractAI> raceProtector = sieges::SiegeRaceProtectorAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::SiegeRaceProtectorAI*>(raceProtector.get()));
	std::unique_ptr<AbstractAI> shield = sieges::ShieldNpcAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::ShieldNpcAI*>(shield.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::AggressiveNpcAI*>(shield.get())) << "SiegeNpcAI extends AggressiveNpcAI";
}

TEST_F(SiegeAiHandlersTest, SiegeNpcsNeitherDecayNorRespawnNorDropLoot) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(GUARDIAN_NPC_ID);
	sieges::GuardianGeneralAI& ai = install<sieges::GuardianGeneralAI>(*npc);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT)) << "the other questions are NpcAI's";
}

TEST_F(SiegeAiHandlersTest, RaceProtectorsDecayAndReward) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(GUARDIAN_NPC_ID);
	sieges::SiegeRaceProtectorAI& ai = install<sieges::SiegeRaceProtectorAI>(*npc);
	EXPECT_TRUE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN)) << "SiegeNpcAI's";
	EXPECT_FALSE(ai.ask(AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE));
}

TEST_F(SiegeAiHandlersTest, ShieldsRewardApButNoLootAndDoNotRespawn) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SHIELD_NPC_ID);
	sieges::ShieldNpcAI& ai = install<sieges::ShieldNpcAI>(*npc);
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY)) << "SiegeNpcAI's";
	EXPECT_TRUE(ai.canThink());
	Ref<SiegeNpc> stone = makeSiegeNpc(FIELD_STONE_NPC_ID);
	EXPECT_FALSE(install<sieges::ShieldNpcAI>(*stone).canThink()) << "a field stone (CONSTRUCT) does not think, so it does not reset";
}

TEST_F(SiegeAiHandlersTest, AShieldRaisesTheFortressShieldOnSpawnAndDropsItOnDespawn) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(SHIELD_NPC_ID);
	install<sieges::ShieldNpcAI>(*npc);
	ASSERT_FALSE(fortress->isUnderShield());
	{
		npc->getAi().onGeneralEvent(event::AIEventType::SPAWNED);
	}
	EXPECT_TRUE(fortress->isUnderShield());
	{
		npc->getAi().onGeneralEvent(event::AIEventType::DESPAWNED);
	}
	EXPECT_FALSE(fortress->isUnderShield());
}

TEST_F(SiegeAiHandlersTest, ProtectorsMultiplyTheirMaxHp) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> artifact65 = makeSiegeNpc(ARTIFACT_PROTECTOR_NPC_ID);
	Ref<SiegeNpc> artifact64 = makeSiegeNpc(ARTIFACT_PROTECTOR_64_NPC_ID);
	Ref<SiegeNpc> legendary = makeSiegeNpc(GUARDIAN_NPC_ID);
	Ref<SiegeNpc> hero = makeSiegeNpc(NORMAL_PROTECTOR_NPC_ID);
	EXPECT_FLOAT_EQ(maxHpBaseRate(install<sieges::ArtifactProtectorAI>(*artifact65), *artifact65), 1.5f);
	EXPECT_FLOAT_EQ(maxHpBaseRate(install<sieges::ArtifactProtectorAI>(*artifact64), *artifact64), 1.0f) << "below level 65";
	EXPECT_FLOAT_EQ(maxHpBaseRate(install<sieges::FortressProtectorNpcAI>(*legendary), *legendary), 2.5f);
	EXPECT_FLOAT_EQ(maxHpBaseRate(install<sieges::FortressProtectorNpcAI>(*hero), *hero), 1.0f) << "not legendary";

	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::MAIN_HAND_POWER, 100, *legendary);
	legendary->getAi().modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f) << "only MAXHP";
}

TEST_F(SiegeAiHandlersTest, AGuardianDeityHitsNpcsFiveTimesAsHard) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> guardian = makeSiegeNpc(GUARDIAN_NPC_ID);
	sieges::FortressProtectorNpcAI& ai = install<sieges::FortressProtectorNpcAI>(*guardian);
	Ref<model::gameobjects::Npc> npc = makeWorldNpc(210663, 502, 500, 100);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7001, 502, 500, 100);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(40.0f, *npc, nullptr), 200.0f);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(40.0f, *player, nullptr), 40.0f);
}

TEST_F(SiegeAiHandlersTest, AProtectorsDeathCreditsItsAggroListAndStopsTheSiege) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> guardian = makeSiegeNpc(GUARDIAN_NPC_ID);
	install<sieges::FortressProtectorNpcAI>(*guardian);
	Ref<StoppableSiege> siege = runtime::makeRef<StoppableSiege>(*fortress);
	services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance()).put(FORTRESS_ID, siege);
	Ref<model::gameobjects::player::Player> elyos = makeWorldPlayer(7002, 502, 500, 100);
	{
		know(*guardian, *elyos);
		guardian->getAggroList().addDamage(*elyos, 300, false, std::nullopt);
		guardian->getAggroList().addDamage(*elyos, 200, false, std::nullopt);
		sieges::AbstractSiegeProtectorAI::stopSiege(*guardian);
	}
	Ptr<services::siege::SiegeRaceCounter> elyosCounter = siege->getSiegeCounter()->getRaceCounter(SiegeRace::ELYOS);
	EXPECT_EQ(elyosCounter->getTotalDamage(), 500);
	EXPECT_EQ(elyosCounter->getPlayerDamageCounter(), (std::vector<std::pair<int32_t, int64_t>>{{7002, 500}}));
	EXPECT_TRUE(siege->isBossKilled());
	EXPECT_EQ(siege->finishes, 1);
	EXPECT_FALSE(services::SiegeService::getInstance().isSiegeInProgress(FORTRESS_ID));
}

TEST_F(SiegeAiHandlersTest, AProtectorBackHomeForgetsItsAggroList) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> guardian = makeSiegeNpc(GUARDIAN_NPC_ID);
	sieges::FortressProtectorNpcAI& ai = install<sieges::FortressProtectorNpcAI>(*guardian);
	Ref<model::gameobjects::player::Player> elyos = makeWorldPlayer(7003, 502, 500, 100);
	{
		know(*guardian, *elyos);
		guardian->getAggroList().addDamage(*elyos, 300, false, std::nullopt);
		ASSERT_FALSE(guardian->getAggroList().stream().empty());
		ai.handleBackHome();
	}
	EXPECT_TRUE(guardian->getAggroList().stream().empty()) << "old damages are not counted in stopSiege";
}


// ------------------------------------------------------------------------------------------------------------------- the bases (M5i I-8)

TEST_F(SiegeAiHandlersTest, TheBaseAIsAreNamedAndABaseBossNeitherDecaysNorRespawnsNorDropsLoot) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> npc = makeSiegeNpc(GUARDIAN_NPC_ID);
	std::unique_ptr<AbstractAI> protector = sieges::BaseProtectorAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::BaseProtectorAI*>(protector.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::AggressiveNpcAI*>(protector.get())) << "BaseProtectorAI extends AggressiveNpcAI";
	std::unique_ptr<AbstractAI> flag = sieges::FlagBaseNpcAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<sieges::FlagBaseNpcAI*>(flag.get()));
	EXPECT_FALSE(dynamic_cast<gameserver::handlers::ai::AggressiveNpcAI*>(flag.get())) << "a plain NpcAI";

	sieges::BaseProtectorAI& ai = install<sieges::BaseProtectorAI>(*npc);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(ai.ask(AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT)) << "the other questions are AggressiveNpcAI's";
}

TEST_F(SiegeAiHandlersTest, ABaseBossFromLevel65TakesTheBaseProtectorMultiplier) {
	AI_TEST_SCOPE;
	const float saved = configs::main::SiegeConfig::BASE_PROTECTOR_HEALTH_MULTIPLIER.load();
	configs::main::SiegeConfig::BASE_PROTECTOR_HEALTH_MULTIPLIER.store(3.0f);
	Ref<SiegeNpc> boss65 = makeSiegeNpc(ARTIFACT_PROTECTOR_NPC_ID);
	Ref<SiegeNpc> boss64 = makeSiegeNpc(ARTIFACT_PROTECTOR_64_NPC_ID);
	const float rate65 = maxHpBaseRate(install<sieges::BaseProtectorAI>(*boss65), *boss65);
	const float rate64 = maxHpBaseRate(install<sieges::BaseProtectorAI>(*boss64), *boss64);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::MAIN_HAND_POWER, 100, *boss65);
	boss65->getAi().modifyOwnerStat(attack);
	configs::main::SiegeConfig::BASE_PROTECTOR_HEALTH_MULTIPLIER.store(saved);
	EXPECT_FLOAT_EQ(rate65, 3.0f);
	EXPECT_FLOAT_EQ(rate64, 1.0f) << "below level 65";
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f) << "only MAXHP";
}

TEST_F(SiegeAiHandlersTest, ASiegeBaseFlagNeitherTakesNorDealsDamage) {
	AI_TEST_SCOPE;
	Ref<SiegeNpc> flag = makeSiegeNpc(GUARDIAN_NPC_ID);
	sieges::FlagBaseNpcAI& ai = install<sieges::FlagBaseNpcAI>(*flag);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7002, 502, 500, 100);
	EXPECT_FLOAT_EQ(ai.modifyDamage(*player, 40.0f, nullptr), 0.0f);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(40.0f, *player, nullptr), 0.0f);
}

} // namespace
} // namespace aion::gameserver::ai::testing
