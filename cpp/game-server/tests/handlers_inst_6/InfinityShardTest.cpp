// I6-2b (docs/design/p6-i6-plan.md §3): InfinityShardInstance (game-server/data/handlers/instance) and the infinityShard AIs IdeResonatorAI and
// HyperionAI (game-server/data/handlers/ai/instance/infinityShard) on a real Infinity Shard instance. Left to a gate: the charge effects'
// messages (onStartEffect needs an Effect of a real skill), the guards' walks (walker rows; a test's DummyAI cannot walk), the logout teleport
// (HousingService) and the casts' effects.

#include "I6TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/infinityShard/HyperionAI.h"
#include "aion/gameserver/handlers/ai/instance/infinityShard/IdeResonatorAI.h"
#include "aion/gameserver/handlers/instance/InfinityShardInstance.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#define AION_INFINITY_AIS(X)                                                                                                                  \
	X(HyperionAI)                                                                                                                             \
	X(IdeResonatorAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::instance::infinityShard {                                                                       \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_INFINITY_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace is = ::aion::gameserver::handlers::ai::instance::infinityShard;
using gameserver::ai::AIState;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleDied;
	using AI::handleSpawned;
};

class InfinityShardTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		shard = &create(INFINITY_SHARD, InfinityShardInstance_instanceFactory);
		enter(*shard, 128.0f, 128.0f, 125.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 130.0f, float y = 130.0f, float z = 125.0f) {
		return spawnNpc(*shard, npcId, x, y, z, int8_t{0});
	}

	size_t count(int32_t id) { return npcs(*shard, id).size(); }

	size_t guards() {
		size_t n = 0;
		for (int32_t id = 231096; id <= 231101; id++)
			n += count(id);
		return n;
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="shard" nameId="1" stack="SHARD" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	void dies(int32_t npcId) { shard->getInstanceHandler()->onDie(*place(npcId)); }

	/** Hyperion with an NpcAI (his cast 21255 needs one) and the barrier */
	runtime::Ptr<Npc> hyperionAndBarrier() {
		runtime::Ptr<Npc> hyperion = place(231073, 128.0f, 135.0f);
		install<gameserver::handlers::ai::AggressiveNpcAI>(*hyperion);
		place(730741);
		return hyperion;
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* shard = nullptr;
};

/** The two AION_AI factories build their classes; Hyperion is the boss */
TEST_F(InfinityShardTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = is::cls##_aiFactory(*npc);                                                                                                \
		EXPECT_NE(dynamic_cast<is::cls*>(ai.get()), nullptr) << #cls;                                                                         \
	}
	AION_INFINITY_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
	auto& handler = dynamic_cast<InfinityShardInstance&>(*shard->getInstanceHandler());
	EXPECT_TRUE(handler.isBoss(*place(231073)));
	EXPECT_FALSE(handler.isBoss(*place(231074)));
	EXPECT_FALSE(handler.isBoss(*place(900200)));
}

/** InfinityShardInstance.java:52-58, 106-122: a generator's death deletes it; while one stands the players hear BROKENPROTECTION */
TEST_F(InfinityShardTest, AGeneratorFallsWhileOthersStand) {
	hyperionAndBarrier();
	place(231078);
	runtime::Ptr<Npc> generator = place(231074);
	shard->getInstanceHandler()->onDie(*generator);
	EXPECT_FALSE(generator->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_BROKENPROTECTION())), 1);
	EXPECT_EQ(count(730741), 1u);
	EXPECT_EQ(count(231092), 0u);
	place(231086); // the last of the four ids stands
	shard->getInstanceHandler()->onDie(*npcs(*shard, 231078)[0]);
	dies(231082);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_BROKENPROTECTION())), 3);
	EXPECT_EQ(count(730741), 1u);
}

/** InfinityShardInstance.java:106-175: the last generator opens the fight: the barrier goes, the resonators come 10 s apart, four turrets at
 * 30-45 s */
TEST_F(InfinityShardTest, TheLastGeneratorOpensTheFight) {
	publishSkills({21254, 21255, 21199});
	hyperionAndBarrier();
	dies(231086);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_BROKENPROTECTIONALL())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_CHARGING())), 1);
	EXPECT_EQ(count(730741), 0u);
	EXPECT_EQ(count(231092), 1u);
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count(231093), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(231093), 1u);
	executor().advance(std::chrono::seconds(20));
	EXPECT_EQ(count(231094), 1u);
	EXPECT_EQ(count(231095), 1u);
	EXPECT_EQ(count(231102), 0u) << "the turrets not before 30 s";
	executor().advance(std::chrono::seconds(15));
	EXPECT_EQ(count(231102), 4u);
}

/** InfinityShardInstance.java:142-161, 177-180: 20 minutes with Hyperion alive fail the run: he goes, USER_KILL, 1.5 s later all go and the exit
 * comes */
TEST_F(InfinityShardTest, TwentyMinutesFailTheRun) {
	publishSkills({21254, 21255, 21199});
	hyperionAndBarrier();
	dies(231086);
	executor().advance(std::chrono::minutes(20) - std::chrono::milliseconds(1));
	EXPECT_EQ(count(231073), 1u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(231073), 0u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDRUNEWP_USER_KILL())), 1);
	EXPECT_EQ(count(730842), 0u);
	executor().advance(std::chrono::milliseconds(1500));
	EXPECT_EQ(count(730842), 1u);
	EXPECT_EQ(count(231092), 0u) << "everything else despawned";
}

/** InfinityShardInstance.java:59-66: a dead resonator or turret comes back where it fell after 29-31 s */
TEST_F(InfinityShardTest, ResonatorsAndTurretsComeBack) {
	runtime::Ptr<Npc> resonator = place(231093, 126.5471f, 154.47961f);
	resonator->getController().delete_();
	shard->getInstanceHandler()->onDie(*resonator);
	executor().advance(std::chrono::milliseconds(28999));
	EXPECT_EQ(count(231093), 0u);
	executor().advance(std::chrono::seconds(2) + std::chrono::milliseconds(1));
	ASSERT_EQ(count(231093), 1u);
	EXPECT_FLOAT_EQ(npcs(*shard, 231093)[0]->getY(), 154.47961f);
}

/** InfinityShardInstance.java:67-71, 81-89: Hyperion's death, or his first return home, despawns everything and opens the exit */
TEST_F(InfinityShardTest, HyperionsEndOpensTheExit) {
	runtime::Ptr<Npc> hyperion = hyperionAndBarrier();
	shard->getInstanceHandler()->onBackHome(*hyperion);
	EXPECT_EQ(count(730842), 1u);
	EXPECT_EQ(count(730741), 0u);
	runtime::Ptr<Npc> again = place(231073);
	runtime::Ptr<Npc> bystander = place(900200);
	shard->getInstanceHandler()->onBackHome(*again);
	EXPECT_TRUE(bystander->isSpawned()) << "once: the second return despawns nothing";
	shard->getInstanceHandler()->onDie(*again);
	EXPECT_EQ(count(730842), 1u) << "his death despawns the exit and opens a new one";
}

/** IdeResonatorAI.java:26-90: targets Hyperion; its charge casts end in 21371 on itself; no decay, respawn or rewards */
TEST_F(InfinityShardTest, AResonatorServesHyperion) {
	publishSkills({21257, 21258, 21371});
	runtime::Ptr<Npc> hyperion = hyperionAndBarrier();
	runtime::Ptr<Npc> resonator = place(231092);
	auto& ai = install<Probe<is::IdeResonatorAI>>(*resonator);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleSpawned();
	EXPECT_EQ(resonator->getTarget(), runtime::Ptr<model::gameobjects::VisibleObject>(hyperion));
	executor().advance(std::chrono::seconds(18));
	ai.handleDied();
	runtime::Ptr<Npc> lonely = place(231093);
	hyperion->getController().delete_();
	auto& lonelyAi = install<Probe<is::IdeResonatorAI>>(*lonely);
	lonely->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(*lonely));
	lonelyAi.handleSpawned();
	EXPECT_EQ(lonely->getTarget(), nullptr) << "Java's targetCreature(this, null) clears the target";
}

/** HyperionAI.java:48-82: beyond 4 m (radii off) he attacks ranged with earth magic, 1.5 times the damage, beyond 10 m 4.5 times */
TEST_F(InfinityShardTest, HyperionHitsHarderFromAfar) {
	runtime::Ptr<Npc> hyperion = place(231073, 128.0f, 128.0f);
	auto& ai = install<Probe<is::HyperionAI>>(*hyperion);
	EXPECT_EQ(ai.modifyInitialSkillDelay(3000), 0);
	EXPECT_EQ(ai.getAttackTypeAnimation(*place(900200, 129.0f, 128.0f)), model::animations::AttackTypeAnimation::MELEE);
	EXPECT_EQ(ai.modifyAttackType(model::templates::item::ItemAttackType::MAGICAL_FIRE), model::templates::item::ItemAttackType::PHYSICAL);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 100.0f);
	EXPECT_EQ(ai.getAttackTypeAnimation(*place(900200, 135.0f, 128.0f)), model::animations::AttackTypeAnimation::RANGED);
	EXPECT_EQ(ai.modifyAttackType(model::templates::item::ItemAttackType::PHYSICAL), model::templates::item::ItemAttackType::MAGICAL_EARTH);
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 150.0f);
	ai.getAttackTypeAnimation(*place(900200, 145.0f, 128.0f));
	EXPECT_FLOAT_EQ(ai.modifyOwnerDamage(100.0f, player(), nullptr), 450.0f);
}

/** HyperionAI.java:89-150: 80 % two ancient tyrhunds around him, 75 % the first guard wave (one a side) with two more */
TEST_F(InfinityShardTest, HyperionsPhasesCallTyrhundsAndGuards) {
	runtime::Ptr<Npc> hyperion = place(231073, 128.0f, 128.0f);
	auto& ai = install<Probe<is::HyperionAI>>(*hyperion);
	InstanceKnownListPairing::pair(*hyperion, player());
	hyperion->getAggroList().addDamage(player(), 1, true, std::nullopt);
	hyperion->getLifeStats()->setCurrentHpPercent(74);
	for (int32_t phase = 0; phase < 3; phase++) // 100 % (the blasts), 80 %, 75 %
		ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(231103), 4u);
	EXPECT_EQ(guards(), 2u);
	for (const runtime::Ptr<Npc>& tyrhund : npcs(*shard, 231103))
		EXPECT_LE(std::hypot(tyrhund->getX() - 128.0f, tyrhund->getY() - 128.0f), 12.01f);
	ai.handleDied();
}

/** HyperionAI.java:110-145: 21253's end calls a guard wave by the stage; 21246's starts the last phase (eight guards and four tyrhunds every 60 s
 * from 35 s), which his death stops */
TEST_F(InfinityShardTest, HyperionsLastPhase) {
	publishSkills({21253, 21246});
	runtime::Ptr<Npc> hyperion = place(231073, 128.0f, 128.0f);
	auto& ai = install<Probe<is::HyperionAI>>(*hyperion);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21253), 56);
	EXPECT_EQ(guards(), 0u) << "stage 0: no wave";
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21246), 56);
	executor().advance(std::chrono::milliseconds(34999));
	EXPECT_EQ(guards(), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(guards(), 8u);
	EXPECT_EQ(count(231103), 4u);
	executor().advance(std::chrono::seconds(60));
	EXPECT_EQ(guards(), 16u);
	ai.handleDied();
	executor().advance(std::chrono::seconds(60));
	EXPECT_EQ(guards(), 16u) << "stopped";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
