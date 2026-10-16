// A1, lane B, Ahserion's Flight (docs/design/a1-laneb-plan.md cp6): AhserionAggressiveNpcAI, AhserionSorcerer, AhserionAssaultCommanderAI,
// EreshkigalsVoiceAI, AhserionConstructAI, AhserionGate, CampDefenseCannonAI, AhserionAssaultPod, AhserionSkyAssaulterAI and
// AhserionRegistrationCorridor (game-server/data/handlers/ai/worlds/panesterra/ahserionsflight) on npcs of the AI tests' Poeta map.
//
// Not covered here, and by no gate (the raid cannot run in a gate yet): the troopers the pod and the sky assaulter land (rndSpawn/spawn need
// the test map in the World), the gate's buff, the skills' effects, joining a team.

#include "../ai/AiWorldTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/AggressiveNoLootNpcAI.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAssaultCommanderAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAssaultPod.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionGate.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionRegistrationCorridor.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionSkyAssaulterAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionSorcerer.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/CampDefenseCannonAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/EreshkigalsVoiceAI.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {
::aion::gameserver::handlers::AIFactory AhserionAggressiveNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory AhserionAssaultCommanderAI_aiFactory;
::aion::gameserver::handlers::AIFactory AhserionAssaultPod_aiFactory;
::aion::gameserver::handlers::AIFactory AhserionGate_aiFactory;
::aion::gameserver::handlers::AIFactory AhserionRegistrationCorridor_aiFactory;
::aion::gameserver::handlers::AIFactory AhserionSkyAssaulterAI_aiFactory;
::aion::gameserver::handlers::AIFactory AhserionSorcerer_aiFactory;
::aion::gameserver::handlers::AIFactory CampDefenseCannonAI_aiFactory;
::aion::gameserver::handlers::AIFactory EreshkigalsVoiceAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight

namespace aion::gameserver::ai::testing {
namespace {

namespace flight = gameserver::handlers::ai::worlds::panesterra::ahserionsflight;

using poll::AIQuestion;
using runtime::Ref;
using services::panesterra::ahserion::PanesterraFaction;
using model::templates::item::ItemAttackType;

constexpr int32_t PLAIN_NPC_ID = 252101;
constexpr int32_t TIMED_AGGRESSIVE_NPC_ID = 277242; // AhserionAggressiveNpcAI.java:27
constexpr int32_t BELUS_CORRIDOR_NPC_ID = 802219;   // AhserionRegistrationCorridor.java:32
constexpr int32_t ASK_PASS_BY_SVS_DIRECT_PORTAL = 905067;

/** Test rows of npc_templates.xml for the ids the cases need */
const dataholders::NpcData& flightNpcTemplates() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		std::string xml = "<npc_templates>";
		for (int32_t id : {PLAIN_NPC_ID, TIMED_AGGRESSIVE_NPC_ID, BELUS_CORRIDOR_NPC_ID, 802221})
			xml += std::string(R"(<npc_template npc_id=")") + std::to_string(id) +
				R"(" name_id="1" level="65" name="flight npc" attack_speed="2000" tribe="MONSTER" rating="NORMAL" rank="NOVICE" race="DRAGON")"
				R"( srange="8" sangle="270" arange="2"><stats maxHp="1000" maxMp="0" attack="10">)"
				R"(<speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)";
		return xml::bindString<dataholders::NpcData>(context, xml + "</npc_templates>").release();
	}();
	return *holder;
}

struct AggressiveProbe : flight::AhserionAggressiveNpcAI {
	using AhserionAggressiveNpcAI::AhserionAggressiveNpcAI;
	using AhserionAggressiveNpcAI::handleSpawned;
};
struct VoiceProbe : flight::EreshkigalsVoiceAI {
	using EreshkigalsVoiceAI::EreshkigalsVoiceAI;
	using EreshkigalsVoiceAI::handleSpawned;
	using EreshkigalsVoiceAI::handleDespawned;
};
struct CorridorProbe : flight::AhserionRegistrationCorridor {
	using AhserionRegistrationCorridor::AhserionRegistrationCorridor;
	using AhserionRegistrationCorridor::handleSpawned;
};

class AhserionA1AiTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		AI_TEST_SCOPE;
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady();
		std::string table = "<player_experience_table>";
		for (int32_t level = 0; level < 67; level++)
			table += "<exp>" + std::to_string(level * 1000) + "</exp>";
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(experienceContext, table + "</player_experience_table>"));
		savedMaxPlayers = configs::main::SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load();
		configs::main::SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.store(200);
	}

	void TearDown() override {
		configs::main::SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.store(savedMaxPlayers);
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		if (skillsPublished)
			dataholders::DataManager::SKILL_DATA.resetForTests();
		flightNpcs.clear();
		regionActivator = nullptr;
		AiWorldTest::TearDown();
	}

	/** an npc of the given test template, placed in the map like makeWorldNpc places one */
	Ref<model::gameobjects::Npc> makeNpc(int32_t npcId, float x = 501) {
		Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), makeSpawn(npcId, x, 500, 100), flightNpcTemplates().getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, 500, 100);
		flightNpcs.push_back(npc);
		return npc;
	}

	template <class AI>
	AI& install(model::gameobjects::Npc& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="flight" nameId="1" stack="FLIGHT" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
		skillsPublished = true;
	}

	float maxHpBaseRate(AbstractAI& ai, model::gameobjects::Npc& owner) {
		model::stats::calc::AdditionStat stat(model::stats::container::StatEnum::MAXHP, 1000, owner);
		ai.modifyOwnerStat(stat);
		return stat.getBaseRate();
	}

	xml::LoadContext skillContext;
	xml::LoadContext experienceContext;
	bool skillsPublished = false;
	int32_t savedMaxPlayers = 0;
	std::vector<Ref<model::gameobjects::Npc>> flightNpcs;
	Ref<model::gameobjects::player::Player> regionActivator;
};

/** The nine AION_AI factories build their classes on the bases Java gives them */
TEST_F(AhserionA1AiTest, TheFactoriesBuildTheirClasses) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> npc = makeNpc(PLAIN_NPC_ID);
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::AggressiveNoLootNpcAI*>(flight::AhserionAggressiveNpcAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<flight::AhserionAggressiveNpcAI*>(flight::AhserionSorcerer_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<flight::AhserionAggressiveNpcAI*>(flight::AhserionAssaultCommanderAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<flight::AhserionConstructAI*>(flight::AhserionGate_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::AggressiveNoLootNpcAI*>(flight::CampDefenseCannonAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<flight::AhserionAssaultPod*>(flight::AhserionAssaultPod_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<flight::AhserionSkyAssaulterAI*>(flight::AhserionSkyAssaulterAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<flight::AhserionRegistrationCorridor*>(flight::AhserionRegistrationCorridor_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<flight::EreshkigalsVoiceAI*>(flight::EreshkigalsVoiceAI_aiFactory(*npc).get()));
	for (gameserver::handlers::AIFactory* general : {&flight::AhserionAssaultPod_aiFactory, &flight::AhserionSkyAssaulterAI_aiFactory,
		     &flight::AhserionRegistrationCorridor_aiFactory})
		EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::GeneralNpcAI*>((*general)(*npc).get()));
}

/** AhserionSorcerer.java:17-20, CampDefenseCannonAI.java:44-47: magical fire */
TEST_F(AhserionA1AiTest, TheSorcererAndTheCannonAttackWithMagicalFire) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> sorcerer = makeNpc(PLAIN_NPC_ID, 501);
	Ref<model::gameobjects::Npc> cannon = makeNpc(PLAIN_NPC_ID, 502);
	EXPECT_EQ(install<flight::AhserionSorcerer>(*sorcerer).modifyAttackType(ItemAttackType::PHYSICAL), ItemAttackType::MAGICAL_FIRE);
	EXPECT_EQ(install<flight::CampDefenseCannonAI>(*cannon).modifyAttackType(ItemAttackType::PHYSICAL), ItemAttackType::MAGICAL_FIRE);
}

/** AhserionConstructAI.java:22-48, CampDefenseCannonAI.java:24-42: max hp by max.players.per.team / 100; no respawn, no loot */
TEST_F(AhserionA1AiTest, TheConstructsScaleWithTheTeamSize) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> construct = makeNpc(PLAIN_NPC_ID, 501);
	flight::AhserionConstructAI& ai = install<flight::AhserionConstructAI>(*construct);
	EXPECT_FLOAT_EQ(maxHpBaseRate(ai, *construct), 2.0f);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT)) << "NpcAI's";
	Ref<model::gameobjects::Npc> attacker = makeNpc(PLAIN_NPC_ID, 503);
	EXPECT_FLOAT_EQ(ai.modifyDamage(*attacker, 40.0f, nullptr), 40.0f) << "no effect: NpcAI's";
	Ref<model::gameobjects::Npc> cannon = makeNpc(PLAIN_NPC_ID, 502);
	flight::CampDefenseCannonAI& cannonAi = install<flight::CampDefenseCannonAI>(*cannon);
	EXPECT_FLOAT_EQ(maxHpBaseRate(cannonAi, *cannon), 2.0f);
	EXPECT_FLOAT_EQ(cannonAi.modifyDamage(*attacker, 40.0f, nullptr), 40.0f);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::MAIN_HAND_POWER, 100, *construct);
	ai.modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f) << "only MAXHP";
}

/** CampDefenseCannonAI.java:49-57: after an attack the cannon forgets its skill number */
TEST_F(AhserionA1AiTest, ACampCannonResetsAfterItsAttack) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> cannon = makeNpc(PLAIN_NPC_ID, 501);
	flight::CampDefenseCannonAI& ai = install<flight::CampDefenseCannonAI>(*cannon);
	cannon->setSkillNumber(3);
	ai.handleFinishAttack();
	EXPECT_EQ(cannon->getSkillNumber(), 0);
}

/** AhserionAggressiveNpcAI.java:24-31: 277242 despawns after 8 minutes, another id stays */
TEST_F(AhserionA1AiTest, The277242DespawnsAfterEightMinutes) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> timed = makeNpc(TIMED_AGGRESSIVE_NPC_ID, 501);
	Ref<model::gameobjects::Npc> other = makeNpc(PLAIN_NPC_ID, 502);
	install<AggressiveProbe>(*timed).handleSpawned();
	install<AggressiveProbe>(*other).handleSpawned();
	world::World::getInstance().storeObject(*timed);
	world::World::getInstance().storeObject(*other);
	EXPECT_TRUE(timed->getController().hasTask(model::TaskId::DESPAWN));
	EXPECT_FALSE(other->getController().hasTask(model::TaskId::DESPAWN));
	executor->advance(std::chrono::minutes(8) - std::chrono::milliseconds(1));
	EXPECT_TRUE(world::World::getInstance().findVisibleObject(timed->getObjectId()));
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(timed->getObjectId()));
	EXPECT_TRUE(world::World::getInstance().findVisibleObject(other->getObjectId()));
	world::World::getInstance().removeObject(*other);
}

/** AhserionAssaultCommanderAI.java:20-24: its Pincer Attack (20648) adds 100000 hate to a target of its aggro list; another skill does not */
TEST_F(AhserionA1AiTest, TheAssaultCommandersPincerAttackAddsHate) {
	AI_TEST_SCOPE;
	publishSkills({20648, 20649});
	Ref<model::gameobjects::Npc> commander = makeNpc(PLAIN_NPC_ID, 501);
	flight::AhserionAssaultCommanderAI& ai = install<flight::AhserionAssaultCommanderAI>(*commander);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7501, 502, 500, 100);
	know(*commander, *player);
	commander->getAggroList().addHate(*player, 10);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(20649), 1);
	EXPECT_EQ(commander->getAggroList().getHate(*player), 10);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(20648), 1);
	EXPECT_EQ(commander->getAggroList().getHate(*player), 100010);
}

/** EreshkigalsVoiceAI.java:24-39: 30 s after its spawn it speaks and despawns 2 s later; a despawn before cancels it */
TEST_F(AhserionA1AiTest, EreshkigalsVoiceSpeaksAndGoes) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> voice = makeNpc(PLAIN_NPC_ID, 501);
	VoiceProbe& ai = install<VoiceProbe>(*voice);
	world::World::getInstance().storeObject(*voice);
	ai.handleSpawned();
	executor->advance(std::chrono::milliseconds(30000));
	EXPECT_TRUE(voice->getController().hasTask(model::TaskId::DESPAWN)) << "it spoke and scheduled its despawn";
	executor->advance(std::chrono::milliseconds(1999));
	EXPECT_TRUE(world::World::getInstance().findVisibleObject(voice->getObjectId()));
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(voice->getObjectId()));

	Ref<model::gameobjects::Npc> silenced = makeNpc(PLAIN_NPC_ID, 502);
	VoiceProbe& silencedAi = install<VoiceProbe>(*silenced);
	silencedAi.handleSpawned();
	silencedAi.handleDespawned();
	executor->advance(std::chrono::milliseconds(30000));
	EXPECT_FALSE(silenced->getController().hasTask(model::TaskId::DESPAWN)) << "the idle task was cancelled";
}

/** AhserionRegistrationCorridor.java:28-65: the faction by npc id, the despawn after 10 minutes, the level-65 question */
TEST_F(AhserionA1AiTest, TheRegistrationCorridor) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> corridor = makeNpc(BELUS_CORRIDOR_NPC_ID, 501);
	CorridorProbe& ai = install<CorridorProbe>(*corridor);
	world::World::getInstance().storeObject(*corridor);
	ai.handleSpawned();
	EXPECT_EQ(ai.getFaction(), PanesterraFaction::BELUS);
	Ref<model::gameobjects::Npc> aspida = makeNpc(802221, 502);
	CorridorProbe& aspidaAi = install<CorridorProbe>(*aspida);
	aspidaAi.handleSpawned();
	EXPECT_EQ(aspidaAi.getFaction(), PanesterraFaction::ASPIDA);
	Ref<model::gameobjects::player::Player> low = makeWorldPlayer(7502, 502, 500, 100);
	low->getCommonData()->setLevel(64);
	ai.handleDialogStart(*low);
	EXPECT_FALSE(low->getResponseRequester().remove(ASK_PASS_BY_SVS_DIRECT_PORTAL)) << "below level 65";
	Ref<model::gameobjects::player::Player> high = makeWorldPlayer(7503, 502, 500, 100);
	high->getCommonData()->setLevel(65);
	ai.handleDialogStart(*high);
	EXPECT_TRUE(high->getResponseRequester().remove(ASK_PASS_BY_SVS_DIRECT_PORTAL));
	executor->advance(std::chrono::minutes(10) - std::chrono::milliseconds(1));
	EXPECT_TRUE(world::World::getInstance().findVisibleObject(corridor->getObjectId()));
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(corridor->getObjectId()));
}

} // namespace
} // namespace aion::gameserver::ai::testing
