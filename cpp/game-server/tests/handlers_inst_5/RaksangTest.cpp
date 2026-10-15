// P6I-7b (docs/design/p6-instances-plan.md §4): RaksangRuinsInstance and the raksang AIs (game-server/data/handlers) on a real Raksang Ruins
// instance (1536: the AIs spawn at its coordinates), each AI installed with replaceAi. The vault's teleport (HousingService loads from the
// database), the zone entry of the vault trial (no zones here), the casts' effects and the executors' walks are left to a gate.

#include "I5TestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/raksang/IllusionMasterSharikAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/RaksangGargoyleAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/RaksangRubbleAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/RakshaAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/RakshaSealingWallAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/ScaldingExecutorAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/SealGeneratorAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/TheFlamelordAI.h"
#include "aion/gameserver/handlers/ai/instance/raksang/VasukiLifesparkAI.h"
#include "aion/gameserver/handlers/instance/RaksangRuinsInstance.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#define AION_RAKSANG_AIS(X)                                                                                                                   \
	X(IllusionMasterSharikAI)                                                                                                                     \
	X(RaksangGargoyleAI)                                                                                                                          \
	X(RaksangRubbleAI)                                                                                                                            \
	X(RakshaAI)                                                                                                                                   \
	X(RakshaSealingWallAI)                                                                                                                        \
	X(ScaldingExecutorAI)                                                                                                                         \
	X(SealGeneratorAI)                                                                                                                            \
	X(TheFlamelordAI)                                                                                                                             \
	X(VasukiLifesparkAI)

namespace aion::gameserver::handlers::ai::instance::raksang {
#define AION_DECLARE_FACTORY(cls) ::aion::gameserver::handlers::AIFactory cls##_aiFactory;
AION_RAKSANG_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY
} // namespace aion::gameserver::handlers::ai::instance::raksang

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::raksang;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// the protected handlers the cases drive directly (Java: the AI's events)
template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleDied;
	using AI::handleSpawned;
	using AI::handleBackHome;
};
template <class AI>
struct MovedProbe : AI {
	using AI::AI;
	using AI::handleCreatureMoved;
	using AI::handleSpawned;
};

class RaksangTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(RAKSANG_RUINS, RaksangRuinsInstance_instanceFactory);
		enter(*instance, 800.0f, 960.0f, 792.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 805.0f, float y = 960.0f) { return spawnNpc(*instance, npcId, x, y, 792.0f, int8_t{0}); }

	runtime::Ptr<Npc> dies(int32_t npcId) {
		runtime::Ptr<Npc> npc = place(npcId);
		instance->getInstanceHandler()->onDie(*npc);
		return npc;
	}

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	/** SKILL_DATA with one plain template per id (the base fixture publishes an empty holder; its TearDown resets this one) */
	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="raksang" nameId="1" stack="RAKSANG" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	void aggro(Npc& npc) {
		InstanceKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** RaksangRuinsInstance.java:43-51: 1.8 times the experience; the entry spawns the way's npc of the player's race */
TEST_F(RaksangTest, TheEntryPicksTheWaysNpc) {
	EXPECT_FLOAT_EQ(instance->getInstanceHandler()->getExpMultiplier(), 1.8f);
	instance->getInstanceHandler()->onEnterInstance(player());
	EXPECT_EQ(count({206378, 206379, 206380}), 1u) << "an Elyos";
	EXPECT_EQ(count({206395, 206396, 206397}), 0u);
}

/** :56-65, 140-145: the vault's 31 kills give the message and open the vault; before, its door refuses */
TEST_F(RaksangTest, TheVaultNeedsItsTrial) {
	runtime::Ptr<Npc> vault = place(730438);
	instance->getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *vault);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_DOOR_CONDITION())), 1);
	for (int32_t i = 0; i < 30; i++)
		dies(236010 + i % 5);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_END())), 0);
	dies(236014);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_END())), 1);
}

/** :277-301, 159-259, 80-96: the arena's watcher starts eight rounds every 10 s (4 + 3 alternating: 28); the 28th kill ends the trial; the
 * second arena's watcher starts the next eight rounds, whose 28 more kills bring the boss */
TEST_F(RaksangTest, TheArenasRounds) {
	runtime::Ptr<Npc> watcher = place(206198);
	instance->getInstanceHandler()->onCreatureDetected(*watcher, player());
	EXPECT_FALSE(watcher->isSpawned());
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count({236020}), 4u);
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count({236021}), 3u);
	executor().advance(std::chrono::seconds(80));
	EXPECT_EQ(count({236020, 236021}), 28u) << "eight rounds";
	for (int32_t i = 0; i < 27; i++)
		dies(236020);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_B_END())), 0);
	dies(236021);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_B_END())), 1);
	runtime::Ptr<Npc> second = place(206199);
	instance->getInstanceHandler()->onCreatureDetected(*second, player());
	executor().advance(std::chrono::milliseconds(0));
	EXPECT_EQ(count({236096}), 4u);
	for (int32_t i = 0; i < 27; i++)
		dies(236096);
	EXPECT_EQ(count({236305}), 0u);
	dies(236097);
	EXPECT_EQ(count({236305}), 1u) << "the 56th kill brings the boss";
}

/** :123-139, 264-275: the forge calls five guards near the player (2 s, 8 s, 12 s) and goes; their 15th death opens door 457 */
TEST_F(RaksangTest, TheForgeCallsItsGuards) {
	runtime::Ptr<Npc> forge = place(702673);
	instance->getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), *forge);
	EXPECT_FALSE(forge->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_A_START())), 1);
	executor().advance(std::chrono::milliseconds(1999));
	EXPECT_EQ(count({236074, 236075, 236076}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({236074, 236075, 236076}), 2u);
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count({236074, 236075, 236076}), 5u);
	for (int32_t i = 0; i < 14; i++)
		dies(236074);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_C_END())), 0);
	dies(236075);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_TAMES_SOLO_C_END())), 1);
}

/** :111-117, 313-318: the last boss's death opens the exit; the destroy stops the rounds */
TEST_F(RaksangTest, TheExitAndTheDestroy) {
	dies(236306);
	EXPECT_EQ(count({730445}), 1u);
	runtime::Ptr<Npc> watcher = place(206198);
	instance->getInstanceHandler()->onCreatureDetected(*watcher, player());
	executor().advance(std::chrono::milliseconds(0));
	instance->getInstanceHandler()->onInstanceDestroy();
	executor().advance(std::chrono::seconds(80));
	EXPECT_EQ(count({236020, 236021}), 4u) << "only the first round";
}

/** The nine AION_AI factories build their classes */
TEST_F(RaksangTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls) EXPECT_NE(dynamic_cast<ais::cls*>(ais::cls##_aiFactory(*npc).get()), nullptr) << #cls;
	AION_RAKSANG_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** RakshaSealingWallAI.java:32-53: a player within 35 m calls the stronger Raksha while Sharik lives, the weaker after; the wall goes */
TEST_F(RaksangTest, TheSealingWallCallsRaksha) {
	runtime::Ptr<Npc> sharik = place(217425, 850.0f, 960.0f);
	runtime::Ptr<Npc> wall = place(900200);
	auto& ai = install<MovedProbe<ais::RakshaSealingWallAI>>(*wall);
	EXPECT_FALSE(ai.canThink());
	ai.handleCreatureMoved(player());
	EXPECT_EQ(count({217647}), 1u);
	EXPECT_FALSE(wall->isSpawned());
	sharik->getController().delete_();
	runtime::Ptr<Npc> second = place(900300);
	auto& secondAi = install<MovedProbe<ais::RakshaSealingWallAI>>(*second);
	secondAi.handleCreatureMoved(player());
	EXPECT_EQ(count({217475}), 1u);
}

/** RakshaAI.java:40-117: below 75 % every 15 s from 3 s a cast and, 3 s later, rubble at each living player she knows; her death opens the
 * exit */
TEST_F(RaksangTest, RakshaDropsRubble) {
	publishSkills({19938});
	runtime::Ptr<Npc> raksha = place(900200);
	auto& ai = install<Probe<ais::RakshaAI>>(*raksha);
	aggro(*raksha);
	raksha->getLifeStats()->setCurrentHpPercent(70);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::seconds(3));
	EXPECT_EQ(count({282325}), 0u);
	executor().advance(std::chrono::seconds(3));
	EXPECT_EQ(count({282325}), 1u) << "one living player";
	ai.handleDied();
	EXPECT_EQ(count({730445}), 1u);
}

/** TheFlamelordAI.java:30-122: at 40 % the round stops and 5 s later one executor comes from the stairs */
TEST_F(RaksangTest, TheFlamelordSendsAnExecutor) {
	publishSkills({19925, 19980, 19924});
	runtime::Ptr<Npc> flamelord = place(900200);
	auto& ai = install<Probe<ais::TheFlamelordAI>>(*flamelord);
	aggro(*flamelord);
	flamelord->getLifeStats()->setCurrentHpPercent(85);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	flamelord->getLifeStats()->setCurrentHpPercent(35);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(count({282451}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count({282451}), 1u);
	EXPECT_EQ(count({282452}), 0u);
	ai.handleDied();
}

/** IllusionMasterSharikAI.java:58-72, 158-165: at 80 % her mirror on her side (position 1); her death takes it */
TEST_F(RaksangTest, ShariksMirrorAndReturn) {
	runtime::Ptr<Npc> sharik = place(900200); // near the player: far away an attack sends her home
	auto& ai = install<Probe<ais::IllusionMasterSharikAI>>(*sharik);
	aggro(*sharik);
	sharik->getLifeStats()->setCurrentHpPercent(75);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	ASSERT_EQ(count({730446}), 1u) << "position 1";
	EXPECT_EQ(npcs(*instance, 730446)[0]->getSpawn()->getStaticId(), 5);
	ai.handleDied();
	EXPECT_EQ(count({730446}), 0u);
}

/** RaksangRubbleAI.java:22-38: a second after its spawn the rubble casts and goes 9 s later */
TEST_F(RaksangTest, TheRubbleGoes) {
	publishSkills({19937});
	runtime::Ptr<Npc> rubble = place(900200);
	auto& ai = install<Probe<ais::RaksangRubbleAI>>(*rubble);
	EXPECT_FALSE(ai.canThink());
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_TRUE(rubble->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(rubble->isSpawned());
}

/** SealGeneratorAI.java:33-52: a player within 30 m makes it announce itself once; every hit does 1 */
TEST_F(RaksangTest, TheSealGenerator) {
	runtime::Ptr<Npc> generator = place(900200);
	auto& ai = install<MovedProbe<ais::SealGeneratorAI>>(*generator);
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 500.0f, nullptr), 1.0f);
	ai.handleCreatureMoved(player());
	ai.handleCreatureMoved(player());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1401156, std::vector<std::string>{}))), 1);
}

/** ScaldingExecutorAI.java:203-210: no decay, respawn or reward; immune; it does not think */
TEST_F(RaksangTest, TheExecutorsQuestions) {
	runtime::Ptr<Npc> executorNpc = place(900200);
	auto& ai = install<ais::ScaldingExecutorAI>(*executorNpc);
	EXPECT_FALSE(ai.canThink());
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
