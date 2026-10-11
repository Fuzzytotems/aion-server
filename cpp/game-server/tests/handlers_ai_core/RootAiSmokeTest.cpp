// M5j stage 3 CP3 (m5j-plan.md §18.4, N-02): the AI smoke of handlers-and-porting-plan.md's P5-05 row over every ported root AI (the 38
// AION_AI markers of handlers/ai/*.cpp): for each, an npc of a shipped row (QuestNpcAiTestSupport.h: Jucleas 203752) gets the AI its marker's
// factory builds, next to a player in the fixture's active map region who is its creator and clicks it, and goes through BEFORE_SPAWNED ->
// SPAWNED -> CREATURE_SEE -> DIALOG_START -> ATTACK -> DIED -> DESPAWNED, then all timers for 10 minutes of virtual time: 0 exceptions, 0
// unported or partial hits, no task error (ExecuteWrapper's log), and the AI ends DESPAWNED. QuestNpcAiSmokeTest.cpp is the precedent for the
// three AIs M5d pulled forward; this file runs them again with the rest.
//
// The data some AIs read on the way is published empty or with one row for the npc (Java would throw without it, as a template without the
// data would): AI_DATA (bombs: BombAI.handleSpawned; summons: SummonerAI.handleSpawned), CHEST_DATA (ChestAI's dialog warns), SPAWNS_DATA
// (ConquestOfferingPortalAI.findTargetLocation), BIND_POINT_DATA (ResurrectAI's dialog), SKILL_DATA (the skills cast on the way);
// the player has a mailbox (PostboxAI) and stands in the npc's map instance as its creator (DeliveryManAI.getPlayer). The two kisk AIs are not smoked here: KiskAI casts its owner to Kisk (KiskAI.java:28-31), which
// an npc of a shipped row is not; KiskAiTest.cpp runs them on Kisk objects. ASan, which the row also names, is not run by day.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/dataholders/AIData.bind.h"
#include "aion/gameserver/dataholders/AIData.h"
#include "aion/gameserver/dataholders/ChestData.bind.h"
#include "aion/gameserver/dataholders/BindPointData.bind.h"
#include "aion/gameserver/dataholders/BindPointData.h"
#include "aion/gameserver/dataholders/ChestData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/world/World.h"

#include "QuestNpcAiTestSupport.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory)
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory AbyssGuardSimpleAI_aiFactory;
::aion::gameserver::handlers::AIFactory ActionItemNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory AggressiveBossSummonNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory AggressiveNoLootNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory AggressiveNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory BombAI_aiFactory;
::aion::gameserver::handlers::AIFactory BookAI_aiFactory;
::aion::gameserver::handlers::AIFactory BubblegutAI_aiFactory;
::aion::gameserver::handlers::AIFactory ChestAI_aiFactory;
::aion::gameserver::handlers::AIFactory ConquestOfferingAggressiveAI_aiFactory;
::aion::gameserver::handlers::AIFactory ConquestOfferingBuffNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory ConquestOfferingPortalAI_aiFactory;
::aion::gameserver::handlers::AIFactory ConquestOfferingSpawnerAI_aiFactory;
::aion::gameserver::handlers::AIFactory DeliveryManAI_aiFactory;
::aion::gameserver::handlers::AIFactory FirecrackerAI_aiFactory;
::aion::gameserver::handlers::AIFactory FollowingNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory GeneralNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory HiddenTeleportNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory HomingNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory NeutralGuardAI_aiFactory;
::aion::gameserver::handlers::AIFactory NoActionAI_aiFactory;
::aion::gameserver::handlers::AIFactory NoDmgNoActionAI_aiFactory;
::aion::gameserver::handlers::AIFactory NoInteractionAI_aiFactory;
::aion::gameserver::handlers::AIFactory OneDmgAI_aiFactory;
::aion::gameserver::handlers::AIFactory OneDmgNoActionAI_aiFactory;
::aion::gameserver::handlers::AIFactory PlatinumFountainAI_aiFactory;
::aion::gameserver::handlers::AIFactory PostboxAI_aiFactory;
::aion::gameserver::handlers::AIFactory ResurrectAI_aiFactory;
::aion::gameserver::handlers::AIFactory ServantNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShifterAI_aiFactory;
::aion::gameserver::handlers::AIFactory SkillAreaNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory SkillCooltimeResetAI_aiFactory;
::aion::gameserver::handlers::AIFactory SpeakerAI_aiFactory;
::aion::gameserver::handlers::AIFactory SummonerAI_aiFactory;
::aion::gameserver::handlers::AIFactory TrapNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory UseSkillAndDieAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::ai::testing {
namespace {

namespace roots = gameserver::handlers::ai;

using event::AIEventType;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;

/** ai_templates (bombs.xml, spawn_helpers.xml shapes) for the smoke's npc: a bomb of the fixture-free skill 10034 and a summons percentage */
constexpr const char* SMOKE_AI_XML = R"(<ai_templates><ai npcId="203752"><bombs><bomb skillId="10034" cd="1000"/></bombs>)"
									 R"(<summons><percentage percent="50"><summonGroup npcId="204075" schedule="1000"/></percentage></summons></ai>)"
									 R"(</ai_templates>)";

/**
 * skill_templates.xml shape (the item fixture's 10034 row, its conditions left out) under the ids the AIs cast on the way: BubblegutAI's
 * 16447 at the spawn, ConquestOfferingBuffNpcAI's 21924-21927 at the use
 */
std::string smokeSkillsXml() {
	std::string xml = "<skill_data>";
	for (int32_t id : {16447, 21924, 21925, 21926, 21927})
		xml += R"(<skill_template skill_id=")" + std::to_string(id) +
			R"(" name="smoke" nameId="1" stack="SMOKE" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
			R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
	return xml + "</skill_data>";
}

struct SmokeCase {
	const char* name;
	gameserver::handlers::AIFactory* factory;
};

class RootAiSmokeTest : public QuestNpcAiWorldTest, public ::testing::WithParamInterface<SmokeCase> {
protected:
	void SetUp() override {
		QuestNpcAiWorldTest::SetUp();
		AI_TEST_SCOPE;
		dataholders::DataManager::AI_DATA.publish(xml::bindString<dataholders::AIData>(dataContexts.emplace_back(), SMOKE_AI_XML));
		dataholders::DataManager::CHEST_DATA.publish(xml::bindString<dataholders::ChestData>(dataContexts.emplace_back(), "<chest_templates/>"));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(dataContexts.emplace_back(), "<spawns/>"));
		dataholders::DataManager::BIND_POINT_DATA.publish(
			xml::bindString<dataholders::BindPointData>(dataContexts.emplace_back(), "<bind_points/>"));
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(dataContexts.emplace_back(), smokeSkillsXml()));
	}

	void TearDown() override {
		{
			AI_TEST_SCOPE;
			dataholders::DataManager::SPAWNS_DATA.resetForTests();
			dataholders::DataManager::BIND_POINT_DATA.resetForTests();
			dataholders::DataManager::SKILL_DATA.resetForTests();
			dataholders::DataManager::CHEST_DATA.resetForTests();
			dataholders::DataManager::AI_DATA.resetForTests();
		}
		QuestNpcAiWorldTest::TearDown();
	}

	std::deque<xml::LoadContext> dataContexts;
};

TEST_P(RootAiSmokeTest, LivesThroughItsEventsAndTimers) {
	AI_TEST_SCOPE;
	const SmokeCase& smokeCase = GetParam();
	const uint64_t unportedBefore = runtime::unportedHitCount();
	const uint64_t partialBefore = runtime::partialHitCount();
	LogCapture wrapperLog("com.aionemu.commons.utils.concurrent.ExecuteWrapper");
	runtime::Ref<Npc> npc = makeWorldNpc(JUCLEAS, 505, 500, 100);
	runtime::Ref<Player> player = makeWorldPlayer(881001, 503, 500, 100); // 2 m away: in talk range
	player->setQuestStateList(model::gameobjects::player::QuestStateList::create()); // a loaded player always has one
	world::World::getInstance().storeObject(*player); // Npc.getCreator finds it in the World
	world::World::getInstance().storeObject(*npc);
	mapInstance->addObject(*player); // DeliveryManAI.getPlayer: the creator in the npc's map instance
	player->setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(*player)); // a loaded player always has one (PostboxAI)
	npc->setCreatorId(player->getObjectId());
	know(*npc, *player);
	std::unique_ptr<AbstractAI> created = (*smokeCase.factory)(*npc);
	ASSERT_NE(created, nullptr) << smokeCase.name;
	AbstractAI& ai = *created;
	npc->replaceAi(std::move(created));

	EXPECT_NO_THROW({
		ai.onGeneralEvent(AIEventType::BEFORE_SPAWNED);
		ai.onGeneralEvent(AIEventType::SPAWNED);
		ai.onCreatureEvent(AIEventType::CREATURE_SEE, *player);
		ai.onCreatureEvent(AIEventType::DIALOG_START, *player);
		ai.onCreatureEvent(AIEventType::ATTACK, *player); // the row's ATTACKED: the event an attack fires (AbstractAI.java:335)
		ai.onGeneralEvent(AIEventType::DIED);
		ai.onGeneralEvent(AIEventType::DESPAWNED);
		executor->advance(std::chrono::minutes(10));
	});

	EXPECT_TRUE(ai.isInState(AIState::DESPAWNED));
	EXPECT_EQ(runtime::unportedHitCount(), unportedBefore) << "an unported site was reached";
	EXPECT_EQ(runtime::partialHitCount(), partialBefore) << "a partial site was reached";
	EXPECT_EQ(wrapperLog.text(), "") << "a task threw";
	mapInstance->removeObject(*player);
	world::World::getInstance().removeObject(*npc);
	world::World::getInstance().removeObject(*player);
}

#define AION_SMOKE(Class, aiName) SmokeCase{aiName, &roots::Class##_aiFactory}

INSTANTIATE_TEST_SUITE_P(EveryRootAi, RootAiSmokeTest,
	::testing::Values(AION_SMOKE(AbyssGuardSimpleAI, "simple_abyssguard"), AION_SMOKE(ActionItemNpcAI, "useitem"),
		AION_SMOKE(AggressiveBossSummonNpcAI, "aggressive_boss_summon"), AION_SMOKE(AggressiveNoLootNpcAI, "aggressive_no_loot"),
		AION_SMOKE(AggressiveNpcAI, "aggressive"), AION_SMOKE(BombAI, "bomb"), AION_SMOKE(BookAI, "book"), AION_SMOKE(BubblegutAI, "bubblegut"),
		AION_SMOKE(ChestAI, "chest"), AION_SMOKE(ConquestOfferingAggressiveAI, "conquest_offering_aggressive"),
		AION_SMOKE(ConquestOfferingBuffNpcAI, "conquest_offering_buff_npc"), AION_SMOKE(ConquestOfferingPortalAI, "conquest_offering_portal"),
		AION_SMOKE(ConquestOfferingSpawnerAI, "conquest_offering_spawner"), AION_SMOKE(DeliveryManAI, "deliveryman"),
		AION_SMOKE(FirecrackerAI, "firecracker"), AION_SMOKE(FollowingNpcAI, "following"), AION_SMOKE(GeneralNpcAI, "general"),
		AION_SMOKE(HiddenTeleportNpcAI, "hidden_teleporter"), AION_SMOKE(HomingNpcAI, "homing"), AION_SMOKE(NeutralGuardAI, "neutralguard"),
		AION_SMOKE(NoActionAI, "noaction"), AION_SMOKE(NoDmgNoActionAI, "no_dmg_no_action"), AION_SMOKE(NoInteractionAI, "no_interaction"),
		AION_SMOKE(OneDmgAI, "onedmg_aggressive"), AION_SMOKE(OneDmgNoActionAI, "onedmg_passive"), AION_SMOKE(PlatinumFountainAI, "fountain"),
		AION_SMOKE(PostboxAI, "postbox"), AION_SMOKE(ResurrectAI, "resurrect"), AION_SMOKE(ServantNpcAI, "servant"),
		AION_SMOKE(ShifterAI, "shifter"), AION_SMOKE(SkillAreaNpcAI, "skillarea"), AION_SMOKE(SkillCooltimeResetAI, "customcdreset"),
		AION_SMOKE(SpeakerAI, "speaker"), AION_SMOKE(SummonerAI, "summoner"), AION_SMOKE(TrapNpcAI, "trap"),
		AION_SMOKE(UseSkillAndDieAI, "useSkillAndDie")),
	[](const ::testing::TestParamInfo<SmokeCase>& info) {
		std::string name;
		for (const char* c = info.param.name; *c != '\0'; ++c)
			if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9'))
				name += *c;
		return name;
	});

} // namespace
} // namespace aion::gameserver::ai::testing
