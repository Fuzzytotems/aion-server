// P5-04, M5e stage 1, E-02 (m5e-plan.md §2.4, E-02 and G-04: "PulledEffect and SimpleRootEffect read geo ... and get a tests/geo-fixture case
// each"): the geo readers of the lane against real geometry. MovementEffectsTest runs them on the empty GeoMap of geo data off, where
// GeoService.getClosestCollision answers the requested point, so it cannot tell whether a body takes the collision's answer or its own
// request. Here GeoService is initialised with geo data on from generated files (the tests/geo GeoWorldLoaderFilesTest format: a big endian
// models.mesh and a 210010000.geo of placements, no terrain) holding four walls in Poeta, one lane each:
//
// - y 500: a PHYSICAL_SEE_THROUGH wall at x 505 (DEFAULT_COLLISIONS stop at it, CANT_SEE_COLLISIONS do not: it blocks moves, not sight)
// - y 520: a PHYSICAL wall at x 505 (blocks both)
// - y 540: a PHYSICAL wall at x 505.4, just behind a monster at x 505
// - y 560: a PHYSICAL wall at x 510
//
// Each wall is a vertical quad 6 m wide (y +-3) and 20 m high (z 95..115). The cases assert the bodies of PulledEffect.java:37-47,
// SimpleRootEffect.java:37-44 and RandomMoveLocEffect.java:42-50 with GeoMap.getClosestCollision's answers (GeoMap.java): a collision farther than
// COLLISION_BOUND_OFFSET + 0.05 m gives the contact point set back 0.5 m towards the origin (and, without ground below it, 1 m lower than the
// ray, i.e. the origin's own z); a nearer one gives the origin itself.
//
// The fixture restores the empty GeoMaps (GeoService.init with geo data off) after each case, and the working directory right after the load
// (GeoWorldLoader reads data/geo/ relative to it).

#include "EffectsMzTestSupport.h"

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;

/** 8441 "Capture", 8219 "Stumble" and 2400 "Boost", their <effects> verbatim from skill_templates.xml (as MovementEffectsTest has them) */
constexpr const char* GEO_SKILLS_XML =
	R"(<skill_template skill_id="8441" name="Capture" nameId="288593" stack="PULLED" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="DEBUFF")"
	R"( activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true"><effects>)"
	R"(<pulled duration1="2000" effectid="20108" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" hopb="100" hopa="100" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8219" name="Stumble" nameId="281599" stack="NORMALATTACK_SIMPLEMOVEBACK" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="NONE" tslot="NOSHOW" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<simpleroot duration1="1000" effectid="20003" e="1" noresist="true" element="WIND" hoptype="SKILLLV" hopb="1000" hopa="100" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="2400" name="Boost" nameId="2286732" cooldownId="1703" group="RI_FORWARDDASH" stack="RI_FORWARDDASH" lvl="1")"
	R"( skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="300" duration="0")"
	R"( cancel_rate="10" chain_skill_prob="100"><effects>)"
	R"(<randommoveloc reserved5="1" direction="0" distance="15" e="1" noresist="true" hoptype="SKILLLV" />)"
	R"(</effects></skill_template>)";

// ---- geo files (GeoWorldLoader.java's formats, written as tests/geo/GeoWorldLoaderFilesTest.cpp writes them) ---------------------------------

class BigEndianWriter {
public:
	std::vector<uint8_t> bytes;

	BigEndianWriter& u8(uint8_t value) {
		bytes.push_back(value);
		return *this;
	}
	BigEndianWriter& i16(int16_t value) {
		bytes.push_back(static_cast<uint8_t>(static_cast<uint16_t>(value) >> 8));
		bytes.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	BigEndianWriter& u32(uint32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8)
			bytes.push_back(static_cast<uint8_t>(value >> shift));
		return *this;
	}
	BigEndianWriter& f32(float value) { return u32(std::bit_cast<uint32_t>(value)); }
	BigEndianWriter& name(std::string_view text) {
		i16(static_cast<int16_t>(text.size()));
		bytes.insert(bytes.end(), text.begin(), text.end());
		return *this;
	}
};

/** CollisionIntention ids (CollisionIntention.java): PHYSICAL 1, PHYSICAL_SEE_THROUGH 1 << 7 */
constexpr uint8_t PHYSICAL = 1;
constexpr uint8_t PHYSICAL_SEE_THROUGH = 1 << 7;

/** A models.mesh entry of one model: a vertical quad in the y-z plane (x 0), y -3..3, z -5..15, with the collision intention */
void writeWall(BigEndianWriter& out, std::string_view name, uint8_t intentions) {
	const float vertices[] = {0, -3, -5, 0, 3, -5, 0, -3, 15, 0, 3, 15};
	const int8_t indices[] = {0, 1, 2, 1, 3, 2};
	out.name(name).u8(1); // one model
	out.i16(4);
	for (float value : vertices)
		out.f32(value);
	out.i16(2).u8(1); // two triangles, byte indices
	for (int8_t index : indices)
		out.u8(static_cast<uint8_t>(index));
	out.u8(0).u8(intentions); // material 0 (no material template), the intentions
}

/** A .geo placement: the model's name, its location, the identity rotation, scale 1, no despawnable type */
void writePlacement(BigEndianWriter& out, std::string_view name, float x, float y, float z) {
	out.name(name).f32(x).f32(y).f32(z);
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			out.f32(i == j ? 1.0f : 0.0f);
	out.f32(1).f32(1).f32(1).u8(0).i16(0).u8(0);
}

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

class GeoMovementEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + GEO_SKILLS_XML);
		canSeeEnabled = configs::main::GeoDataConfig::CANSEE_ENABLE.load();
		geoEnabled = configs::main::GeoDataConfig::GEO_ENABLE.load();

		root = std::filesystem::temp_directory_path() / ("aion_gs_effects_mz_geo_" + std::to_string(std::random_device()()));
		std::filesystem::create_directories(root / "data" / "geo");
		BigEndianWriter meshes;
		writeWall(meshes, "levels/test/wall_see_through.cgf", PHYSICAL_SEE_THROUGH);
		writeWall(meshes, "levels/test/wall.cgf", PHYSICAL);
		writeFile(root / "data" / "geo" / "models.mesh", meshes.bytes);
		BigEndianWriter geo;
		writePlacement(geo, "levels/test/wall_see_through.cgf", 505, 500, 100);
		writePlacement(geo, "levels/test/wall.cgf", 505, 520, 100);
		writePlacement(geo, "levels/test/wall.cgf", 505.4f, 540, 100);
		writePlacement(geo, "levels/test/wall.cgf", 510, 560, 100);
		writeFile(root / "data" / "geo" / "210010000.geo", geo.bytes);

		// GeoService.init with geo data on: new GeoMaps for every map, loaded from data/geo/ of the working directory
		const std::filesystem::path workingDirectory = std::filesystem::current_path();
		std::filesystem::current_path(root);
		configs::main::GeoDataConfig::GEO_ENABLE.store(true);
		try {
			world::geo::GeoService::getInstance().init();
		} catch (...) {
			std::filesystem::current_path(workingDirectory);
			throw;
		}
		std::filesystem::current_path(workingDirectory);
		geoEngine::GeoCallbacks::setMaterialZoneSink(nullptr); // set by init with geo data on; the walls have no material
	}

	void TearDown() override {
		{
			EFFECT_TEST_SCOPE;
			configs::main::GeoDataConfig::GEO_ENABLE.store(false);
			world::geo::GeoService::getInstance().init(); // the empty GeoMaps the other cases of this process expect
		}
		configs::main::GeoDataConfig::GEO_ENABLE.store(geoEnabled);
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(canSeeEnabled);
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
		EffectsMzTest::TearDown();
	}

	/** Java `new Effect(effector, effected, template, 1, null, null, true, null)`: the sub effect EffectTemplate.calculateSubEffect creates */
	Ref<Effect> subEffect(int32_t skillId, Creature& effector, Creature& effected) {
		Ref<Effect> effect = Effect::create(effector, Ptr<Creature>(effected), skillTemplate(skillId), 1, std::nullopt, nullptr, true, nullptr);
		effect->initialize();
		return effect;
	}

	std::filesystem::path root;
	bool canSeeEnabled = false;
	bool geoEnabled = false;
};

/**
 * The fixture's geometry answers as described: a move along lane y 500 or y 520 is stopped at x 505, sight is blocked on lane 520 only, and
 * the empty lane y 580 is free.
 */
TEST_F(GeoMovementEffectsTest, TheWallsStopMovesAndThePhysicalOnesSight) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> seeThrough = monster(510, 500, 100);
	Ref<Npc> physical = monster(510, 520, 100);
	Ref<Npc> free = monster(510, 580, 100);
	world::geo::GeoService& geo = world::geo::GeoService::getInstance();
	EXPECT_EQ(geo.getClosestCollision(*seeThrough, 501.5f, 500, 100).getX(), 505.5f);
	EXPECT_EQ(geo.getClosestCollision(*physical, 501.5f, 520, 100).getX(), 505.5f);
	EXPECT_EQ(geo.getClosestCollision(*free, 501.5f, 580, 100).getX(), 501.5f);
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(true);
	Ref<Player> onSeeThroughLane = player(6901, PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Player> onPhysicalLane = player(6902, PlayerClass::WARRIOR, 1, 500, 520, 100);
	EXPECT_TRUE(geo.canSee(*seeThrough, *onSeeThroughLane));
	EXPECT_FALSE(geo.canSee(*physical, *onPhysicalLane));
}

/**
 * 8441 across the see-through wall (the can-see check on, as the server default has it): the monster (510, 500) is in sight, so it is pulled,
 * but towards (501.5, 500) only as far as the wall lets it - GeoService.getClosestCollision(effected, effector.x + 1.5, effector.y, effector.z)
 * stops at x 505 and sets it back to 505.5 (PulledEffect.java:45-46 take the collision's point, not the request). startEffect moves it there.
 */
TEST_F(GeoMovementEffectsTest, APullStopsAtAWallItCanBeSeenThrough) {
	EFFECT_TEST_SCOPE;
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(true);
	Ref<Player> templar = player(6911, PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> npc = monster(510, 500, 100);
	Ref<Effect> effect = subEffect(8441, *templar, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getTargetX(), 505.5f) << "the wall's contact point set back 0.5 m towards the monster";
	EXPECT_EQ(effect->getTargetY(), 500.0f);
	EXPECT_EQ(effect->getTargetZ(), 100.0f);
	effect->applyEffect();
	EXPECT_EQ(npc->getX(), 505.5f);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::PULLED));
}

/** 8441 across the physical wall: GeoService.canSee(effected, effector) is false, so calculate returns before any success (PulledEffect.java:34-36) */
TEST_F(GeoMovementEffectsTest, NoPullThroughAWallThatBlocksTheSight) {
	EFFECT_TEST_SCOPE;
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(true);
	Ref<Player> templar = player(6921, PlayerClass::WARRIOR, 1, 500, 520, 100);
	Ref<Npc> npc = monster(510, 520, 100);
	Ref<Effect> effect = subEffect(8441, *templar, *npc);
	EXPECT_FALSE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::NONE);
	effect->applyEffect();
	EXPECT_EQ(npc->getX(), 510.0f);

	// with the can-see check off (GeoDataConfig), the same pull happens and stops at the wall
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
	Ref<Effect> blind = subEffect(8441, *templar, *npc);
	ASSERT_TRUE(blind->isInSuccessEffects(1));
	EXPECT_EQ(blind->getTargetX(), 505.5f);
}

/**
 * 8219 on a monster standing 0.4 m before a wall (lane y 540): the set-back of 0.7 m towards the wall collides nearer than
 * COLLISION_BOUND_OFFSET + 0.05, so GeoMap.getClosestCollision answers the monster's own position and it stays where it is
 * (SimpleRootEffect.java:42-43 take the answer); a monster in the free lane is set back the whole 0.7 m.
 */
TEST_F(GeoMovementEffectsTest, ASetBackAgainstAWallKeepsTheMonsterInPlace) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(6931, PlayerClass::SCOUT, 1, 500, 540, 100);
	Ref<Npc> npc = monster(505, 540, 100);
	Ref<Effect> effect = subEffect(8219, *assassin, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getTargetX(), 505.0f) << "the origin: the wall is nearer than 0.55 m";
	EXPECT_EQ(effect->getTargetY(), 540.0f);
	effect->applyEffect();
	EXPECT_EQ(npc->getX(), 505.0f);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::SIMPLE_MOVE_BACK));

	Ref<Player> other = player(6932, PlayerClass::SCOUT, 1, 500, 580, 100);
	Ref<Npc> free = monster(505, 580, 100);
	Ref<Effect> back = subEffect(8219, *other, *free);
	ASSERT_TRUE(back->isInSuccessEffects(1));
	EXPECT_EQ(back->getTargetX(), 505.0f + 0.7f);
}

/**
 * 2400 Boost from a flying rider on lane y 560 (heading 0, 15 m forwards): the move collides with the wall at x 510 and ends set back at 509.5
 * (RandomMoveLocEffect.java:49-50 put GeoService.findMovementCollision's answer into the skill's target position); applyEffect moves the rider
 * there.
 */
TEST_F(GeoMovementEffectsTest, ABoostEndsBeforeAWall) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(6941, PlayerClass::ENGINEER, 1, 500, 560, 100);
	rider->setFlyState(gameserver::model::gameobjects::state::FlyState::FLYING);
	Ref<model::Skill> skill = model::Skill::create(skillTemplate(2400), *rider, Ptr<Creature>(rider), 1);
	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(rider));
	effect->initialize();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(skill->getX(), 509.5f);
	EXPECT_EQ(skill->getY(), 560.0f);
	EXPECT_EQ(skill->getZ(), 100.0f);
	effect->applyEffect();
	EXPECT_EQ(rider->getX(), 509.5f);
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
