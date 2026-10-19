// P6 leftovers (lane C, 2026-10-18): chunk I1's last AI, ai/instance/custom/pvpveMap/ModifiedIronWallAggressiveAI (game-server/data/handlers),
// on npcs of a real instance (Haramel's map: the AI does not read the map), installed with replaceAi. The PvP map's random boss (its damage
// cap and 882,321 HP) needs PvpMapService's map handler, which no unit test creates: left to a gate, like the casts and the forced moves.

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/ai/instance/custom/pvpveMap/ModifiedIronWallAggressiveAI.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::ai::instance::custom::pvpveMap {
::aion::gameserver::handlers::AIFactory ModifiedIronWallAggressiveAI_aiFactory;
}

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace iw = ::aion::gameserver::handlers::ai::instance::custom::pvpveMap;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::stats::container::StatEnum;
using skillengine::model::Effect;
using controllers::attack::AggroTarget;

struct IronWallProbe : iw::ModifiedIronWallAggressiveAI {
	using ModifiedIronWallAggressiveAI::ModifiedIronWallAggressiveAI;
	using ModifiedIronWallAggressiveAI::handleAttack;
	using ModifiedIronWallAggressiveAI::handleBackHome;
	using ModifiedIronWallAggressiveAI::handleNotAtHome;
	std::vector<int32_t> phases;
	/** false: AttackEventHandler.onAttack stops before the think that sends the AI home (the test's player is no hostile), so only the
	 * phases run */
	bool thinks = true;
	bool canThink() override { return thinks && ModifiedIronWallAggressiveAI::canThink(); }
	void handleHpPhase(int32_t phaseHpPercent) override {
		phases.push_back(phaseHpPercent);
		ModifiedIronWallAggressiveAI::handleHpPhase(phaseHpPercent);
	}
};

class IronWallAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(HARAMEL, HaramelInstance_instanceFactory);
		enter(*instance, 224.0f, 300.0f, 142.0f);
	}

	IronWallProbe& install(Npc& npc) {
		auto ai = std::make_unique<IronWallProbe>(npc);
		IronWallProbe& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 226.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 142.0f, int8_t{7}); }

	/** an attack that only runs the phases (IronWallProbe::thinks) */
	void attack(IronWallProbe& ai) {
		ai.thinks = false;
		ai.handleAttack(runtime::Ptr<Creature>(player()));
		ai.thinks = true;
	}

	static bool queued(Npc& npc, int32_t skillId) {
		return npc.hasQueuedSkill([skillId](model::skill::NpcSkillEntry& entry) { return entry.getSkillId() == skillId; });
	}

	/** the MAXHP stat after the AI's modifyOwnerStat: {base, baseRate} */
	static std::pair<int32_t, float> maxHp(IronWallProbe& ai, Npc& npc) {
		model::stats::calc::AdditionStat stat(StatEnum::MAXHP, 1000.0f, npc);
		ai.modifyOwnerStat(stat);
		return {stat.getBase(), stat.getBaseRate()};
	}

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="ironwall" nameId="1" stack="IRONWALL" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
};

/** The AION_AI factory builds its class */
TEST_F(IronWallAiTest, TheFactoryBuildsItsClass) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<iw::ModifiedIronWallAggressiveAI*>(iw::ModifiedIronWallAggressiveAI_aiFactory(*npc).get()), nullptr);
}

/** ModifiedIronWallAggressiveAI.java:30-45, 124-129: 231304 queues 21165 at 98, 77, 56, 35 and 10 % HP, one phase an attack; back home they
 * start over; another npc has no phases */
TEST_F(IronWallAiTest, TheBossCasts21165AtItsPhases) {
	runtime::Ptr<Npc> boss = place(231304);
	IronWallProbe& ai = install(*boss);
	InstanceKnownListPairing::pair(*boss, player());
	boss->getLifeStats()->setCurrentHpPercent(99);
	attack(ai);
	EXPECT_TRUE(ai.phases.empty()) << "above 98 %";
	EXPECT_FALSE(queued(*boss, 21165));
	boss->getLifeStats()->setCurrentHpPercent(98);
	attack(ai);
	EXPECT_EQ(ai.phases, std::vector<int32_t>{98});
	EXPECT_TRUE(queued(*boss, 21165)) << "handleHpPhase queues 21165";
	attack(ai);
	EXPECT_EQ(ai.phases.size(), 1u) << "98 % is spent";
	boss->getLifeStats()->setCurrentHpPercent(5);
	for (int32_t i = 0; i < 6; i++)
		attack(ai);
	EXPECT_EQ(ai.phases, (std::vector<int32_t>{98, 77, 56, 35, 10})) << "one phase an attack";
	ai.handleBackHome();
	boss->getLifeStats()->setCurrentHpPercent(98);
	attack(ai);
	EXPECT_EQ(ai.phases.back(), 98) << "the phases start over";
	EXPECT_EQ(ai.phases.size(), 6u);

	runtime::Ptr<Npc> other = place(218547);
	IronWallProbe& otherAi = install(*other);
	InstanceKnownListPairing::pair(*other, player());
	other->getLifeStats()->setCurrentHpPercent(5);
	attack(otherAi);
	EXPECT_TRUE(otherAi.phases.empty()) << "no phases";
}

/** ModifiedIronWallAggressiveAI.java:47-78, 145-147: clerics and mages deal 1.2-1.6x by skill, 2.4-3.2x by hit; keymasters unchanged; the
 * rest 2-2.8x by skill, 1.8-2.3x by hit */
TEST_F(IronWallAiTest, TheDamageIsScaledPerNpc) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	publishSkills({1});
	runtime::Ptr<Effect> effect = Effect::create(player(), runtime::Ptr<Creature>(player()), dataholders::DataManager::SKILL_DATA->getSkillTemplate(1), 1);
	auto range = [&](int32_t npcId, bool bySkill) {
		IronWallProbe& ai = install(*place(npcId));
		float low = 1e9f, high = 0;
		for (int32_t i = 0; i < 400; i++) {
			float d = ai.modifyOwnerDamage(100.0f, player(), bySkill ? effect : runtime::Ptr<Effect>());
			low = std::min(low, d);
			high = std::max(high, d);
		}
		return std::pair{low, high};
	};
	auto expectWithin = [](std::pair<float, float> r, float lo, float hi, const char* what) {
		EXPECT_GE(r.first, lo - 0.01f) << what;
		EXPECT_LE(r.second, hi + 0.01f) << what;
		if (hi > lo) {
			EXPECT_LT(r.first, lo + (hi - lo) * 0.2f) << what << ": the range's low end";
			EXPECT_GT(r.second, hi - (hi - lo) * 0.2f) << what << ": the range's high end";
		}
	};
	expectWithin(range(218547, true), 120, 160, "a cleric's skill");
	expectWithin(range(218547, false), 240, 320, "a cleric's hit");
	expectWithin(range(219193, true), 100, 100, "a keymaster's skill");
	expectWithin(range(219218, false), 100, 100, "a keymaster's hit");
	expectWithin(range(900200, true), 200, 280, "another npc's skill");
	expectWithin(range(900200, false), 180, 230, "another npc's hit");
}

/** ModifiedIronWallAggressiveAI.java:80-104: MAXHP rates 3.2 (219167, 219169, 219190), 7 (219218), unchanged (the keymasters 219191-219193),
 * 1.6 otherwise; other stats go to the superclass */
TEST_F(IronWallAiTest, TheMaxHpIsScaledPerNpc) {
	auto rate = [&](int32_t npcId) {
		runtime::Ptr<Npc> npc = place(npcId);
		return maxHp(install(*npc), *npc).second;
	};
	EXPECT_FLOAT_EQ(rate(219167), 3.2f);
	EXPECT_FLOAT_EQ(rate(219218), 7.0f);
	EXPECT_FLOAT_EQ(rate(219193), 1.0f);
	EXPECT_FLOAT_EQ(rate(218547), 1.6f);
	runtime::Ptr<Npc> npc = place(218547);
	IronWallProbe& ai = install(*npc);
	model::stats::calc::AdditionStat attack(StatEnum::MAIN_HAND_POWER, 1000.0f, *npc);
	ai.modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f) << "not MAXHP";
	EXPECT_EQ(attack.getBase(), 1000);
}

/** ModifiedIronWallAggressiveAI.java:106-122: after 21165 the boss jumps to its random target within 26 m 0.5 s later and queues 21171 0.5 s
 * after that; another skill, another npc or no target: nothing */
TEST_F(IronWallAiTest, TheBossJumpsToARandomTargetAfter21165) {
	publishSkills({21165, 21166});
	runtime::Ptr<Npc> boss = place(231304, 226.0f, 300.0f);
	IronWallProbe& ai = install(*boss);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21165), 1);
	executor().advance(std::chrono::seconds(2));
	EXPECT_NEAR(boss->getX(), 226.0f, 0.5f) << "no target";
	InstanceKnownListPairing::pair(*boss, player());
	boss->getAggroList().addDamage(player(), 1, true, std::nullopt);
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21166), 1);
	executor().advance(std::chrono::seconds(2));
	EXPECT_NEAR(boss->getX(), 226.0f, 0.5f) << "another skill";
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21165), 1);
	executor().advance(std::chrono::milliseconds(499));
	EXPECT_NEAR(boss->getX(), 226.0f, 0.5f);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FLOAT_EQ(boss->getX(), player().getX()) << "at its target";
	EXPECT_FLOAT_EQ(boss->getY(), player().getY());
	EXPECT_FALSE(queued(*boss, 21171));
	executor().advance(std::chrono::milliseconds(500));
	EXPECT_TRUE(queued(*boss, 21171));

	runtime::Ptr<Npc> other = place(218547, 228.0f, 300.0f);
	IronWallProbe& otherAi = install(*other);
	otherAi.thinks = false; // no think sends it home and empties its aggro list before the 0.5 s task
	InstanceKnownListPairing::pair(*other, player());
	other->getAggroList().addDamage(player(), 1, true, std::nullopt);
	ASSERT_NE(other->getAggroList().getTarget(AggroTarget::RANDOM, 26), nullptr) << "it has a target to jump to";
	otherAi.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21165), 1);
	executor().advance(std::chrono::seconds(2));
	ASSERT_NE(other->getAggroList().getTarget(AggroTarget::RANDOM, 26), nullptr) << "the target is still there";
	EXPECT_NEAR(other->getX(), 228.0f, 0.5f) << "not the boss";
}

/** ModifiedIronWallAggressiveAI.java:131-143: away from home the boss is put back on its spawn at once and its phases start over */
TEST_F(IronWallAiTest, TheBossIsPutBackHome) {
	runtime::Ptr<Npc> boss = place(231304, 226.0f, 300.0f);
	IronWallProbe& ai = install(*boss);
	InstanceKnownListPairing::pair(*boss, player());
	boss->getLifeStats()->setCurrentHpPercent(98);
	attack(ai);
	boss->clearQueuedSkills();
	world::World::getInstance().updatePosition(*boss, 236.0f, 310.0f, 142.0f, int8_t{0});
	ai.handleNotAtHome();
	EXPECT_FLOAT_EQ(boss->getX(), boss->getSpawn()->getX());
	EXPECT_FLOAT_EQ(boss->getY(), boss->getSpawn()->getY());
	attack(ai);
	EXPECT_TRUE(queued(*boss, 21165)) << "98 % again";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
