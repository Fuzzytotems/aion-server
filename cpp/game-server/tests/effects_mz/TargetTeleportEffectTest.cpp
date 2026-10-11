// P5-04, M5j stage 3 CP3 (m5j-plan.md §18.4, N-04): TargetTeleportEffect - without an alias location the effected player is put a metre in
// front of the effector (GeoService.getClosestCollision answers the point itself without geo data); with one, at a random position of the
// alias on the player's own map, and nowhere on another map or for an unknown alias. The teleports are on the same map of the World's test
// Poeta, so TeleportService takes the spawnOnSameMap arm (as OtherEffectsTest's ReturnEffect case: the character has no client connection,
// SM_PLAYER_INFO would read houses from a database). TargetTeleportEffect.java:33-53.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <string>

#include "aion/gameserver/dataholders/SkillAliasLocationData.bind.h"
#include "aion/gameserver/dataholders/SkillAliasLocationData.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

/** skill_templates.xml:152190-152206 (18708 Criminal Conspiracy)'s targetteleport, and test skills with alias locations */
constexpr const char* TARGET_TELEPORT_SKILLS_XML =
	R"(<skill_template skill_id="64730" name="mz target teleport" nameId="1" stack="MZ_TTELE" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET" first_target_range="50")"
	R"( target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" /><effects><targetteleport distance="5" e="1" noresist="true")"
	R"( hoptype="DAMAGE" /></effects></skill_template>)"
	R"(<skill_template skill_id="64731" name="mz alias teleport" nameId="1" stack="MZ_TTELE2" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET" first_target_range="50")"
	R"( target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" /><effects><targetteleport alias_location="MZ_Poeta" e="1")"
	R"( noresist="true" /></effects></skill_template>)"
	R"(<skill_template skill_id="64732" name="mz other map alias" nameId="1" stack="MZ_TTELE3" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET" first_target_range="50")"
	R"( target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" /><effects><targetteleport alias_location="Alias_5" e="1")"
	R"( noresist="true" /></effects></skill_template>)"
	R"(<skill_template skill_id="64733" name="mz unknown alias" nameId="1" stack="MZ_TTELE4" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET" first_target_range="50")"
	R"( target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" /><effects><targetteleport alias_location="MZ_Nowhere" e="1")"
	R"( noresist="true" /></effects></skill_template>)"
	R"(<skill_template skill_id="64734" name="mz empty alias" nameId="1" stack="MZ_TTELE5" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="TARGET" first_target_range="50")"
	R"( target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" /><effects><targetteleport alias_location="MZ_Empty" e="1")"
	R"( noresist="true" /></effects></skill_template>)";

/** alias_locations.xml shape: one alias with a single position on the test Poeta, alias_locations.xml:9-10 (Alias_5 on 400030000), one empty */
constexpr const char* ALIAS_LOCATIONS_XML = R"(<alias_locations>)"
											R"(<alias_location name="MZ_Poeta" world_id="210010000"><alias_pos x="420.5" y="380.25" z="11"/></alias_location>)"
											R"(<alias_location name="Alias_5" world_id="400030000"><alias_pos x="489.59015" y="463.3067" z="675.85"/></alias_location>)"
											R"(<alias_location name="MZ_Empty" world_id="210010000"></alias_location>)"
											R"(</alias_locations>)";

class TargetTeleportEffectTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests();
		publishSkillData(effectsMzSkills() + TARGET_TELEPORT_SKILLS_XML);
		dataholders::DataManager::SKILL_ALIAS_LOCATION_DATA.publish(
			xml::bindString<dataholders::SkillAliasLocationData>(aliasContext, ALIAS_LOCATIONS_XML));
		traveller = player(9401);
		traveller->setClientConnection(nullptr);
		traveller->setMotions(std::make_unique<gameserver::model::gameobjects::player::motion::MotionList>(*traveller));
		world::World& world = world::World::getInstance();
		world.storeObject(*traveller);
		EXPECT_TRUE(world.setPosition(*traveller, effecttest::POETA, 300.0f, 300.0f, 10.0f, int8_t{0}));
		world.spawn(Ptr<gameserver::model::gameobjects::VisibleObject>(*traveller));
		caster = monster(500, 500, 100);
	}

	void TearDown() override {
		{
			EFFECT_TEST_SCOPE;
			world::zone::ZoneUpdateService::getInstance().run();
			world::World& world = world::World::getInstance();
			world.despawn(*traveller);
			world.removeObject(*traveller);
			traveller->setTarget(nullptr);
		}
		traveller = nullptr;
		caster = nullptr;
		dataholders::DataManager::SKILL_ALIAS_LOCATION_DATA.resetForTests();
		EffectsMzTest::TearDown();
	}

	void expectAt(float x, float y, float z) {
		EXPECT_FLOAT_EQ(traveller->getX(), x);
		EXPECT_FLOAT_EQ(traveller->getY(), y);
		EXPECT_FLOAT_EQ(traveller->getZ(), z);
	}

	xml::LoadContext aliasContext;
	Ref<Player> traveller;
	Ref<Npc> caster;
};

/** no alias: a metre in front of the effector along its heading (0: +x; 90 degrees: heading 30, +y), at the effector's z */
TEST_F(TargetTeleportEffectTest, WithoutAnAliasTheTargetLandsInFrontOfTheCaster) {
	EFFECT_TEST_SCOPE;
	applied(64730, *caster, *traveller);
	expectAt(501.0f, 500.0f, 100.0f);
	EXPECT_TRUE(traveller->isSpawned()) << "spawnOnSameMap";

	world::World::getInstance().updatePosition(*caster, 500.0f, 500.0f, 100.0f, int8_t{30});
	applied(64730, *caster, *traveller);
	EXPECT_NEAR(traveller->getX(), 500.0f, 1e-4);
	EXPECT_FLOAT_EQ(traveller->getY(), 501.0f);
}

/** an alias on the player's map: one of its positions; another map's alias, an unknown one: no teleport; an empty one: Java's null position */
TEST_F(TargetTeleportEffectTest, AnAliasOnTheTargetsMapPicksOneOfItsPositions) {
	EFFECT_TEST_SCOPE;
	applied(64732, *caster, *traveller);
	expectAt(300.0f, 300.0f, 10.0f);
	applied(64733, *caster, *traveller);
	expectAt(300.0f, 300.0f, 10.0f);
	applied(64731, *caster, *traveller);
	expectAt(420.5f, 380.25f, 11.0f);
	Ref<Effect> empty = calculated(64734, *caster, *traveller);
	EXPECT_THROW(empty->getEffectTemplates()[0]->applyEffect(*empty), runtime::NullPointerException);
}

/** an npc effected is not teleported */
TEST_F(TargetTeleportEffectTest, AnNpcIsNotTeleported) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> other = monster(510, 500, 100);
	applied(64730, *caster, *other);
	EXPECT_FLOAT_EQ(other->getX(), 510.0f);
	applied(64731, *caster, *other);
	EXPECT_FLOAT_EQ(other->getX(), 510.0f);
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
