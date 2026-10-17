// I6-5a (docs/design/p6-i6-plan.md §3): TiamatStrongHoldInstance (game-server/data/handlers/instance) and the four brigade generals of
// ai/instance/tiamatStrongHold on a real Tiamat Stronghold instance. Left to a gate: the doors (no door npcs here), the zones (Laksyaka's HQ,
// the Glorious Nexus), the marchers' and drakans' moves, the casts' effects and the Piercing Strike's jump.

#include "I6TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralChantraAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralLaksyakaAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralTahabataAI.h"
#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralTerathAI.h"
#include "aion/gameserver/handlers/instance/TiamatStrongHoldInstance.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#define AION_TIAMAT_GENERALS(X)                                                                                                               \
	X(BrigadeGeneralChantraAI)                                                                                                                \
	X(BrigadeGeneralLaksyakaAI)                                                                                                               \
	X(BrigadeGeneralTahabataAI)                                                                                                               \
	X(BrigadeGeneralTerathAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {                                                                    \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_TIAMAT_GENERALS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ts = ::aion::gameserver::handlers::ai::instance::tiamatStrongHold;
using gameserver::ai::AIState;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleBackHome;
	using AI::handleDied;
};

class TiamatTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		tiamat = &create(TIAMAT_STRONGHOLD, TiamatStrongHoldInstance_instanceFactory);
		enter(*tiamat, 1030.0f, 300.0f, 410.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 1032.0f, float y = 300.0f, float z = 410.0f) {
		return spawnNpc(*tiamat, npcId, x, y, z, int8_t{0});
	}

	size_t count(int32_t id) { return npcs(*tiamat, id).size(); }

	void dies(int32_t npcId) { tiamat->getInstanceHandler()->onDie(*place(npcId)); }

	void aggro(Npc& npc) {
		InstanceKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), 1, true, std::nullopt);
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="tiamat" nameId="1" stack="TIAMAT" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* tiamat = nullptr;
};

/** The four AION_AI factories build their classes; the seven brigade generals are the bosses */
TEST_F(TiamatTest, TheFactoriesAndTheBosses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = ts::cls##_aiFactory(*npc);                                                                                                \
		EXPECT_NE(dynamic_cast<ts::cls*>(ai.get()), nullptr) << #cls;                                                                         \
	}
	AION_TIAMAT_GENERALS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
	auto& handler = dynamic_cast<TiamatStrongHoldInstance&>(*tiamat->getInstanceHandler());
	for (int32_t id = 219352; id <= 219358; id++)
		EXPECT_TRUE(handler.isBoss(*place(id))) << id;
	EXPECT_FALSE(handler.isBoss(*place(219364)));
}

/** TiamatStrongHoldInstance.java:191-218, 245-251: one colonel among three guards at the four corners */
TEST_F(TiamatTest, OneColonelAmongTheGuards) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	std::set<float> colonelPlaces;
	for (int32_t round = 0; round < 20; round++) {
		world::WorldMapInstance& other = create(TIAMAT_STRONGHOLD, TiamatStrongHoldInstance_instanceFactory);
		ASSERT_EQ(npcs(other, 219364).size(), 1u);
		ASSERT_EQ(npcs(other, 219395).size(), 3u);
		colonelPlaces.insert(npcs(other, 219364)[0]->getX());
	}
	EXPECT_EQ(colonelPlaces, std::set<float>({763.4179f, 893.7009f, 893.3f, 762.6f})) << "each of the four corners";
}

/** TiamatStrongHoldInstance.java:43-55, 108-136: the gate's death sends five drakans 5 s later, five deaths seven more, twelve another seven */
TEST_F(TiamatTest, TheGateSendsThreeWaves) {
	dies(730612);
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(count(219369) + count(219373) + count(219411), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(219373), 1u);
	EXPECT_EQ(count(219369) + count(219411), 4u);
	for (int32_t kill = 0; kill < 4; kill++)
		dies(219369);
	EXPECT_EQ(count(219370), 0u);
	dies(219411); // the fifth
	EXPECT_EQ(count(219370), 1u) << "the second wave";
	for (int32_t kill = 0; kill < 6; kill++)
		dies(219373);
	EXPECT_EQ(count(219370), 1u);
	dies(219373); // the twelfth
	EXPECT_EQ(count(219370), 2u) << "the third wave";
	executor().advance(std::chrono::seconds(2)); // the drakans set off at the player
}

/** TiamatStrongHoldInstance.java:56-62, 152-189: Tahabata's death: the reward, and Kahrun's army 7 s later; 13 s after that the marchers who
 * despawn go, Kahrun himself stays */
TEST_F(TiamatTest, TahabatasDeathBringsKahrun) {
	dies(219352);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDTIAMAT_TIAMAT_REWARD_SPAWN())), 1);
	EXPECT_EQ(count(283177), 1u);
	EXPECT_EQ(count(701501), 1u);
	executor().advance(std::chrono::milliseconds(6999));
	EXPECT_EQ(count(800463), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(800463), 8u);
	EXPECT_EQ(count(800374), 2u);
	EXPECT_EQ(count(800336), 1u);
	executor().advance(std::chrono::seconds(13));
	EXPECT_EQ(count(800374) + count(800336), 0u) << "the marchers with despawn go";
	EXPECT_EQ(count(800338), 1u) << "Kahrun";
	EXPECT_EQ(count(800463), 8u);
}

/** TiamatStrongHoldInstance.java:72-104, 220-226: the exit comes with the last of the three generals dead */
TEST_F(TiamatTest, TheExitAfterTheThreeGenerals) {
	runtime::Ptr<Npc> first = place(219354);
	runtime::Ptr<Npc> second = place(219356);
	runtime::Ptr<Npc> third = place(219358);
	first->getController().die(player());
	second->getController().die(player());
	EXPECT_EQ(count(800464), 0u);
	EXPECT_EQ(count(730622), 2u);
	third->getController().die(player());
	EXPECT_EQ(count(800464), 1u);
	EXPECT_EQ(count(800465), 1u);
	EXPECT_EQ(count(730629), 1u);
	tiamat->getInstanceHandler()->onInstanceDestroy();
	dies(219352);
	EXPECT_EQ(count(283177), 0u) << "nothing after the destroy";
}

/** BrigadeGeneralChantraAI.java:29-98: from the first attack a ring every 40 s from 5 s, 5 s after it its own trap (283092 -> 283171,
 * 283094 -> 283172); her death stops the rings */
TEST_F(TiamatTest, ChantrasRingsAndTraps) {
	publishSkills({20942});
	runtime::Ptr<Npc> chantra = place(219357);
	auto& ai = install<Probe<ts::BrigadeGeneralChantraAI>>(*chantra);
	aggro(*chantra);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	executor().advance(std::chrono::seconds(5));
	bool seen283092 = false;
	bool seen283094 = false;
	for (int32_t cycle = 0; cycle < 16 && !(seen283092 && seen283094); cycle++) {
		if (cycle > 0)
			executor().advance(std::chrono::seconds(30));
		ASSERT_EQ(count(283092) + count(283094), 1u) << "cycle " << cycle;
		const bool first = count(283092) == 1;
		(first ? seen283092 : seen283094) = true;
		const size_t traps171 = count(283171);
		const size_t traps172 = count(283172);
		executor().advance(std::chrono::seconds(5));
		EXPECT_EQ(count(283092) + count(283094), 0u) << "the ring goes";
		EXPECT_EQ(count(283171), traps171 + (first ? 1u : 0u)) << "its own trap, cycle " << cycle;
		EXPECT_EQ(count(283172), traps172 + (first ? 0u : 1u)) << "its own trap, cycle " << cycle;
		executor().advance(std::chrono::seconds(5));
	}
	EXPECT_TRUE(seen283092 && seen283094);
	ai.handleDied();
	executor().advance(std::chrono::seconds(32)); // inside the next ring's 5 s
	EXPECT_EQ(count(283092) + count(283094), 0u) << "stopped";
}

/** BrigadeGeneralLaksyakaAI.java:35-92: an attack calls four skeletons 10 m around her one time in about thirty, while none stands */
TEST_F(TiamatTest, LaksyakasSkeletons) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	runtime::Ptr<Npc> laksyaka = place(219353);
	auto& ai = install<Probe<ts::BrigadeGeneralLaksyakaAI>>(*laksyaka);
	aggro(*laksyaka);
	int32_t attacks = 0;
	while (count(283115) == 0 && attacks < 1000) {
		ai.handleAttack(runtime::Ptr<Creature>(player()));
		attacks++;
	}
	ASSERT_EQ(count(283115), 4u);
	EXPECT_GT(attacks, 1);
	for (const runtime::Ptr<Npc>& skeleton : npcs(*tiamat, 283115))
		EXPECT_LE(std::hypot(skeleton->getX() - 1032.0f, skeleton->getY() - 300.0f), 10.01f);
	for (int32_t attack = 0; attack < 300; attack++)
		ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(283115), 4u) << "not while they stand";
	ai.handleDied();
}

/** BrigadeGeneralTahabataAI.java:101-188: 96 % a lava eruption (a 283051, the floors 10 s later); 75 % the fire storm from 10 s; his death
 * takes the floors */
TEST_F(TiamatTest, TahabatasEruptionsAndStorm) {
	publishSkills({20754, 20755, 20756, 20758, 20761});
	runtime::Ptr<Npc> tahabata = place(219352);
	auto& ai = install<Probe<ts::BrigadeGeneralTahabataAI>>(*tahabata);
	aggro(*tahabata);
	tahabata->getLifeStats()->setCurrentHpPercent(95);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(283051), 1u);
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count(283116), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(283116), 1u);
	EXPECT_EQ(count(283117), 1u);
	tahabata->getLifeStats()->setCurrentHpPercent(74);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	executor().advance(std::chrono::seconds(10));
	EXPECT_GE(count(283045), 1u) << "the fire storm";
	EXPECT_LE(count(283045), 4u);
	ai.handleDied();
	EXPECT_EQ(count(283116) + count(283117), 0u);
}

/** BrigadeGeneralTerathAI.java:44-168: the first attack brings the Aetheric Field; at 90 % the gravity event, its adds 10 s later and gone at
 * 30 s; back home the field goes and the next fight brings none (Java bug J-I6-4) */
TEST_F(TiamatTest, TerathsGravityEvent) {
	publishSkills({20737, 20739, 20741, 20942});
	runtime::Ptr<Npc> terath = place(219354);
	auto& ai = install<Probe<ts::BrigadeGeneralTerathAI>>(*terath);
	aggro(*terath);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(730692), 1u) << "the Aetheric Field";
	terath->getLifeStats()->setCurrentHpPercent(89);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_FALSE(ai.canThink());
	EXPECT_EQ(count(283558), 2u);
	executor().advance(std::chrono::seconds(10));
	EXPECT_EQ(count(283109), 1u);
	EXPECT_EQ(count(283110), 1u);
	executor().advance(std::chrono::seconds(20));
	EXPECT_EQ(count(283558) + count(283109) + count(283110), 0u);
	EXPECT_TRUE(ai.canThink());
	ai.handleBackHome();
	EXPECT_EQ(count(730692), 0u);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(730692), 0u) << "Java keeps the deleted field: no new one";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
