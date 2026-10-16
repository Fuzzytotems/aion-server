// P5-03, M5b-2 stage 1 part 3, items F-02 and F-05 (m5b2-plan.md §5): EscapeEffect, the effect of 302 Escape - one of the three class-less skills
// every character learns at level 1 (skill_tree.xml:429, m5b2-plan.md §2.4(a)) - on a real Effect of the real template.
//
// Its applyEffect is TeleportService.moveToBindLocation of the effector, so the escaping player stands in the World singleton's test Poeta (the
// world chunk's test holders, which EffectClassTest publishes for every fixture of this binary) the way TeleportOnSameMapTest
// (tests/playersvc/PlayerReviveServiceTest.cpp) places its traveller, with a bind point on the same map: the same-map teleport arm, which reaches
// no unported body. The player has no connection, so spawnOnSameMap's packets go nowhere.

#include "EffectClassTestSupport.h"

#include <cstdint>
#include <memory>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using model::Effect;

/** 302 Escape, skill_templates.xml:3725-3734 verbatim */
constexpr const char* ESCAPE_XML =
	R"(<skill_template skill_id="302" name="Escape" nameId="2283012" cooldownId="2014" stack="EM_RETURNHOME1" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="36000" duration="20000" cancel_rate="100000">)"
	R"(<properties first_target="ME" first_target_range="1" target_relation="FRIEND" target_type="ONLYONE" />)"
	R"(<useconditions><move_casting allow="false" /></useconditions><effects><escape e="1" noresist="true" /></effects>)"
	R"(<motion name="heal" /></skill_template>)";

/** The bind point: on the test Poeta (instance 1), above its death level */
constexpr float BIND_X = 500.0f;
constexpr float BIND_Y = 400.0f;
constexpr float BIND_Z = 12.5f;

class EscapeEffectTest : public EffectClassTest {
protected:
	void TearDown() override {
		{
			EFFECT_TEST_SCOPE; // the World and the zone queue load shared pointers
			if (escaper.player) {
				// ZoneUpdateService is a process-wide queue that no task drains here (TeleportStatementsTest.cpp's TearDown): drain this test's own
				// entries while the character exists, and cancel the drowning the bind point's z (under water_level 16) starts
				world::zone::ZoneUpdateService::getInstance().run();
				escaper.player->getController().cancelTask(gameserver::model::TaskId::DROWN);
				if (escaper.player->isSpawned())
					world::World::getInstance().despawn(*escaper.player);
				world::World::getInstance().removeObject(*escaper.player);
				escaper.player->setTarget(nullptr);
			}
			escaper = {};
		}
		EffectClassTest::TearDown();
	}

	/** Java PlayerService.getPlayer + World.storeObject/setPosition/spawn at (300, 300, 10) of the test Poeta, bind point on the same map */
	void spawnEscaper() {
		escaper = cp::makePlayer(7301, 9731, "Escaper");
		escaper.player->setMotions(std::make_unique<gameserver::model::gameobjects::player::motion::MotionList>(*escaper.player));
		world::World& world = world::World::getInstance();
		world.storeObject(*escaper.player);
		ASSERT_TRUE(world.setPosition(*escaper.player, POETA, 300.0f, 300.0f, 10.0f, int8_t{0}));
		world.spawn(Ptr<gameserver::model::gameobjects::VisibleObject>(*escaper.player));
		ASSERT_TRUE(escaper.player->isSpawned());
		escaper.player->setBindPoint(gameserver::model::gameobjects::player::BindPointPosition::create(POETA, BIND_X, BIND_Y, BIND_Z, int8_t{7}));
	}

	cp::PlayerFixture escaper;
};

/**
 * EscapeEffect.calculate (EscapeEffect.java:23-27) succeeds for a spawned effected without any resist check, and applyEffect (:18-21) moves the
 * effector to its bind location: the escaper lands on the bind point.
 */
TEST_F(EscapeEffectTest, EscapeTeleportsTheCasterToItsBindPoint) {
	EFFECT_TEST_SCOPE;
	spawnEscaper();
	const model::SkillTemplate* skill = bindSkill(ESCAPE_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "EscapeEffect");

	Ref<Effect> escape = cast(*escaper.player, *escaper.player, skill, 1);
	EXPECT_TRUE(escape->isInSuccessEffects(1));
	EXPECT_FLOAT_EQ(escaper.player->getX(), BIND_X);
	EXPECT_FLOAT_EQ(escaper.player->getY(), BIND_Y);
	EXPECT_FLOAT_EQ(escaper.player->getZ(), BIND_Z);
	EXPECT_EQ(escaper.player->getHeading(), 7);
}

/**
 * The two arms Escape has besides the teleport: an effected that is not spawned gets no success effect (so the effect fails and is never
 * applied), and an effector that is not a player is Java's ClassCastException of `(Player) effect.getEffector()`.
 */
TEST_F(EscapeEffectTest, EscapeNeedsASpawnedEffectedAndAPlayerEffector) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* skill = bindSkill(ESCAPE_XML);
	Ref<Player> player = makePlayer(7311);
	Ref<Npc> npc = makeNpc(701311);

	player->getPosition()->setIsSpawned(false);
	Ref<Effect> unspawned = Effect::create(*player, Ptr<Creature>(*player), skill, 1);
	unspawned->initialize();
	EXPECT_FALSE(unspawned->isInSuccessEffects(1)) << "effect.getEffected().isSpawned() is false";
	EXPECT_EQ(unspawned->getEffectResult(), model::EffectResult::DODGE) << "no success: <escape> has no element, so the effect is a dodge";
	player->getPosition()->setIsSpawned(true);

	Ref<Effect> byNpc = Effect::create(*npc, Ptr<Creature>(*player), skill, 1);
	byNpc->initialize();
	ASSERT_TRUE(byNpc->isInSuccessEffects(1));
	EXPECT_THROW(effectOf(*skill, 0).applyEffect(*byNpc), runtime::ClassCastException);
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
