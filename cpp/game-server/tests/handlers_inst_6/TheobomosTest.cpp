// I6-2a (docs/design/p6-i6-plan.md §3): TheobomosLabInstance (game-server/data/handlers/instance) and the theobomosLab AIs
// (game-server/data/handlers/ai/instance/theobomosLab) on a real Theobomos Lab instance. Left to a gate: the summons' walks (walker rows; a test's
// DummyAI cannot walk: Java's ClassCastException), their arrival and the casts' effects.

#include "I6TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <typeinfo>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/theobomosLab/SilikorofMemoryAI.h"
#include "aion/gameserver/handlers/ai/instance/theobomosLab/TriroansSummonAI.h"
#include "aion/gameserver/handlers/ai/instance/theobomosLab/UnstableTriroanAI.h"
#include "aion/gameserver/handlers/instance/TheobomosLabInstance.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"

#define AION_THEOBOMOS_AIS(X)                                                                                                                 \
	X(SilikorofMemoryAI)                                                                                                                      \
	X(TriroansSummonAI)                                                                                                                       \
	X(UnstableTriroanAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::instance::theobomosLab {                                                                        \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_THEOBOMOS_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace tl = ::aion::gameserver::handlers::ai::instance::theobomosLab;
using gameserver::ai::AIState;
using model::gameobjects::Creature;
using model::gameobjects::Npc;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleBackHome;
	using AI::handleDied;
	using AI::handleSpawned;
};

class TheobomosTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		lab = &create(THEOBOMOS_LAB, TheobomosLabInstance_instanceFactory);
		enter(*lab, 600.0f, 490.0f, 196.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 605.0f, float y = 490.0f, float z = 196.0f) { return spawnNpc(*lab, npcId, x, y, z, int8_t{0}); }

	size_t count(int32_t id) { return npcs(*lab, id).size(); }

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="lab" nameId="1" stack="LAB" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	void aggro(Npc& npc) {
		InstanceKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* lab = nullptr;
};

/** The three AION_AI factories build their classes */
TEST_F(TheobomosTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = tl::cls##_aiFactory(*npc);                                                                                                \
		EXPECT_NE(dynamic_cast<tl::cls*>(ai.get()), nullptr) << #cls;                                                                         \
	}
	AION_THEOBOMOS_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** TheobomosLabInstance.java:84-93: half of the time the Dimensional Researcher at one of three places */
TEST_F(TheobomosTest, TheResearcherComesHalfOfTheTime) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	int32_t with = 0;
	std::set<float> places;
	const int32_t rounds = 30;
	for (int32_t round = 0; round < rounds; round++) {
		world::WorldMapInstance& other = create(THEOBOMOS_LAB, TheobomosLabInstance_instanceFactory);
		size_t researchers = npcs(other, 798223).size();
		ASSERT_LE(researchers, 1u);
		if (researchers == 1) {
			with++;
			places.insert(npcs(other, 798223)[0]->getX());
		}
	}
	EXPECT_GT(with, 5);
	EXPECT_LT(with, rounds - 5);
	EXPECT_EQ(places, std::set<float>({256.26215f, 360.16098f, 476.77145f}));
}

/** TheobomosLabInstance.java:27-44: Triroan's death opens the exit; nothing after the instance's destroy */
TEST_F(TheobomosTest, TriroansDeathOpensTheExit) {
	lab->getInstanceHandler()->onDie(*place(214669));
	ASSERT_EQ(count(730178), 1u);
	EXPECT_FLOAT_EQ(npcs(*lab, 730178)[0]->getX(), 571.15f);
	lab->getInstanceHandler()->onInstanceDestroy();
	lab->getInstanceHandler()->onDie(*place(214669));
	EXPECT_EQ(count(730178), 1u);
}

/** TheobomosLabInstance.java:58-82: a guard dying away from the orb comes back; both dying at it take Silikor's 18481 and the orb 1 s later */
TEST_F(TheobomosTest, SilikorsGuardsMustDieAtTheOrb) {
	publishSkills({18481});
	runtime::Ptr<Npc> orb = place(280973, 620.0f);
	place(214668, 630.0f);
	runtime::Ptr<Npc> farGuard = place(280971, 640.0f);
	lab->getInstanceHandler()->onDie(*farGuard);
	EXPECT_FALSE(farGuard->isSpawned());
	ASSERT_EQ(count(280971), 1u) << "back where it fell";
	EXPECT_FLOAT_EQ(npcs(*lab, 280971)[0]->getX(), 640.0f);
	EXPECT_EQ(npcs(*lab, 280971)[0]->getHeading(), 41);
	lab->getInstanceHandler()->onDie(*place(280971, 624.0f));
	executor().advance(std::chrono::seconds(2));
	EXPECT_TRUE(orb->isSpawned()) << "one guard is not enough";
	lab->getInstanceHandler()->onDie(*place(280972, 616.0f));
	executor().advance(std::chrono::milliseconds(999));
	EXPECT_TRUE(orb->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(orb->isSpawned());
}

/** SilikorofMemoryAI.java:25-63: at 50, 25 and 10 % a 281054 and a 281053 up to 2 m from him; his death takes them */
TEST_F(TheobomosTest, SilikorCallsHisMemories) {
	publishSkills({18481});
	runtime::Ptr<Npc> silikor = place(214668);
	auto& ai = install<Probe<tl::SilikorofMemoryAI>>(*silikor);
	ai.handleSpawned(); // casts 18481 on himself
	aggro(*silikor);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(281054), 0u);
	silikor->getLifeStats()->setCurrentHpPercent(9);
	for (int32_t phase = 0; phase < 3; phase++)
		ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(281054), 3u);
	EXPECT_EQ(count(281053), 3u);
	for (const runtime::Ptr<Npc>& memory : npcs(*lab, 281054))
		EXPECT_LE(std::hypot(memory->getX() - 605.0f, memory->getY() - 490.0f), 2.01f);
	ai.handleDied();
	EXPECT_EQ(count(281054) + count(281053), 0u);
}

/** UnstableTriroanAI.java:29-92: 16699 at 99 %, then the element summons from (601.97, 488.85): fire first at 90 % */
TEST_F(TheobomosTest, TriroanCallsTheElements) {
	publishSkills({16699});
	runtime::Ptr<Npc> triroan = place(214669);
	auto& ai = install<Probe<tl::UnstableTriroanAI>>(*triroan);
	aggro(*triroan);
	triroan->getLifeStats()->setCurrentHpPercent(98);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(280975), 0u) << "99 %: the cast only";
	triroan->getLifeStats()->setCurrentHpPercent(89);
	EXPECT_THROW(ai.handleAttack(runtime::Ptr<Creature>(player())), std::bad_cast) << "the summon's DummyAI cannot walk (Java: ClassCastException)";
	ASSERT_EQ(count(280975), 1u);
	EXPECT_FLOAT_EQ(npcs(*lab, 280975)[0]->getX(), 601.966f);
	EXPECT_EQ(npcs(*lab, 280975)[0]->getSpawn()->getWalkerId(), "3101100002");
}

/** TriroansSummonAI.java:29-121: no thinking; Triroan's pillar (1-4) or none (0) by where he stands */
TEST_F(TheobomosTest, TheSummonsReadTriroansPillar) {
	runtime::Ptr<Npc> fire = place(280975);
	auto& ai = install<Probe<tl::TriroansSummonAI>>(*fire);
	ai.handleSpawned();
	EXPECT_FALSE(ai.canThink());
	runtime::Ptr<Npc> triroan = place(214669, 628.0f, 474.241f, 196.160f);
	EXPECT_EQ(ai.checkDistance(), 1) << "4 m from the first pillar";
	triroan->getController().delete_();
	place(214669, 578.323f, 475.784f, 196.463f);
	EXPECT_EQ(ai.checkDistance(), 4);
	for (const runtime::Ptr<Npc>& npc : npcs(*lab, 214669))
		npc->getController().delete_();
	place(214669, 600.0f, 490.0f, 196.0f);
	EXPECT_EQ(ai.checkDistance(), 0);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
