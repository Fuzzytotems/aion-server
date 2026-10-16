// A1, lane C, slice 4 (worlds/gelkmaros): PadmarashkaAI (game-server/data/handlers/ai/worlds/gelkmaros) on an npc of a real Gelkmaros map
// instance (her protectors and rock slides spawn around (2940, 851)), installed with replaceAi. The slumber's effect (19186) and her berserk
// cast are left to a retest.

#include "ZoneA1TestSupport.h"

#include <cmath>
#include <cstdint>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/worlds/gelkmaros/PadmarashkaAI.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"

namespace aion::gameserver::handlers::ai::worlds::gelkmaros {
::aion::gameserver::handlers::AIFactory PadmarashkaAI_aiFactory;
}

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace gel = ::aion::gameserver::handlers::ai::worlds::gelkmaros;
using model::gameobjects::Creature;
using model::gameobjects::Npc;

struct Probe : gel::PadmarashkaAI {
	using PadmarashkaAI::PadmarashkaAI;
	using PadmarashkaAI::handleAttack;
	using PadmarashkaAI::handleBackHome;
	using PadmarashkaAI::handleDespawned;
	using PadmarashkaAI::handleSpawned;
};

class GelkmarosA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(GELKMAROS, 2940.0f, 860.0f, 35.3f);
		publishSkills({19186, 18730});
	}

	size_t protectors() { return count(281938) + count(281939) + count(281940) + count(281941); }
};

/** The AION_AI factory builds its class; her stats are those of the 12-player tweak */
TEST_F(GelkmarosA1AiTest, TheFactoryBuildsItsClass) {
	runtime::Ptr<Npc> npc = place(900200, 2940.2f, 851.29f, 35.89f);
	auto ai = gel::PadmarashkaAI_aiFactory(*npc);
	ASSERT_NE(dynamic_cast<gel::PadmarashkaAI*>(ai.get()), nullptr);
	model::stats::calc::AdditionStat maxHp(model::stats::container::StatEnum::MAXHP, 1000.0f, *npc);
	ai->modifyOwnerStat(maxHp);
	EXPECT_FLOAT_EQ(maxHp.getExactBaseWithoutBaseRate(), 20'880'000.0f);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::PHYSICAL_ATTACK, 1000.0f, *npc);
	ai->modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getExactBaseWithoutBaseRate(), 2200.0f);
}

/** PadmarashkaAI.java:33-53, 92-117: four protectors at their places, which her despawn takes away; each counts its death once */
TEST_F(GelkmarosA1AiTest, PadmarashkasFourProtectors) {
	runtime::Ptr<Npc> padmarashka = place(900200, 2940.2f, 851.29f, 35.89f);
	auto& ai = install<Probe>(*padmarashka);
	ai.handleSpawned();
	ASSERT_EQ(protectors(), 4u);
	EXPECT_FLOAT_EQ(npcs(*instance, 281938)[0]->getX(), 2906.05f);
	EXPECT_EQ(npcs(*instance, 281941)[0]->getHeading(), 69);
	for (int32_t id : {281938, 281939, 281940, 281941}) // the fourth death takes the Protective Slumber off (none here: a no-op)
		npcs(*instance, id)[0]->getObserveController()->notifyDeathObservers(player());
	ai.handleDespawned();
	EXPECT_EQ(protectors(), 0u);
}

/** PadmarashkaAI.java:55-77, 85-90: at 33 % forty rock slides in a 40-70 m ring; back home they go and the phases start over */
TEST_F(GelkmarosA1AiTest, PadmarashkasRockSlides) {
	runtime::Ptr<Npc> padmarashka = place(900200, 2940.2f, 851.29f, 35.89f);
	auto& ai = install<Probe>(*padmarashka);
	padmarashka->getLifeStats()->setCurrentHpPercent(100);
	ZoneKnownListPairing::pair(*padmarashka, player());
	padmarashka->getAggroList().addDamage(player(), 1, true, std::nullopt);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(281936), 0u);
	padmarashka->getLifeStats()->setCurrentHpPercent(33);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	ASSERT_EQ(count(281936), 40u);
	for (const runtime::Ptr<Npc>& slide : npcs(*instance, 281936)) {
		double distance = std::hypot(slide->getX() - 2940.20f, slide->getY() - 851.29f);
		EXPECT_GE(distance, 39.99);
		EXPECT_LE(distance, 70.01);
	}
	ai.handleBackHome();
	EXPECT_EQ(count(281936), 0u);
	ai.handleAttack(runtime::Ptr<Creature>(player()));
	EXPECT_EQ(count(281936), 40u) << "the phases start over";
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
