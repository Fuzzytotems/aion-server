// The ascension lane (m5d-plan.md A-01, P5-05 / aion_gs_handlers_ai_core): the last conjunct of AbyssGuardSimpleAI's npc-vs-npc aggro rule,
// `PositionUtil.isInRange(owner, npc, owner.getAggroRange()) && GeoService.getInstance().canSee(owner, npc)` (AbyssGuardSimpleAI.java:72-73).
// AbyssGuardSimpleAiTest runs with geo data off, where GeoService.canSee answers true before it looks at any geometry (GeoService.java:76-78,
// GeoService.cpp:117-119), so it cannot see whether the rule asks the geo at all. Here GeoService is initialised with geo data on from generated
// files in the format of tests/geo/GeoWorldLoaderFilesTest.cpp (a big endian models.mesh and a 210010000.geo of placements, no terrain; the
// writers are copied from tests/effects_mz/GeoMovementEffectsTest.cpp), holding two walls between a guard and an enemy npc 5 m apart:
//
// - lane y 520: a PHYSICAL wall at x 502.5 (in CANT_SEE_COLLISIONS: it blocks the sight)
// - lane y 540: a PHYSICAL_SEE_THROUGH wall at x 502.5 (not in CANT_SEE_COLLISIONS: the sight passes it)
// - lane y 560: no wall
//
// Each wall is a vertical quad 6 m wide (y +-3) and 20 m high (z 95..115); both npcs' sight lines run at z 101.25 (getSeeCheckOffset,
// GeoService.java:114-121: upper 2 is not above 2.5, so 1.25). All three lanes are in the region the fixture wakes. The fixture restores the
// empty GeoMaps (GeoService.init with geo data off) after the case, and the working directory right after the load (GeoWorldLoader reads
// data/geo/ relative to it).

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/geoEngine/GeoCallbacks.h"
#include "aion/gameserver/handlers/ai/AbyssGuardSimpleAI.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "QuestNpcAiTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

namespace roots = gameserver::handlers::ai;

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;

// ---- geo files (GeoWorldLoader.java's formats; the writers of tests/effects_mz/GeoMovementEffectsTest.cpp) --------------------------------

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

/** The shipped AbyssGuardSimpleAI with the aggro hook recorded instead of run (as AbyssGuardSimpleAiTest's RecordingAbyssGuard) */
class RecordingAbyssGuard final : public roots::AbyssGuardSimpleAI {
public:
	using AbyssGuardSimpleAI::AbyssGuardSimpleAI;

	int32_t aggroes = 0;

protected:
	void handleCreatureAggro(Creature& creature) override {
		static_cast<void>(creature);
		aggroes++;
	}
};

class AbyssGuardGeoTest : public QuestNpcAiWorldTest {
protected:
	void SetUp() override {
		QuestNpcAiWorldTest::SetUp();
		AI_TEST_SCOPE;
		canSeeEnabled = configs::main::GeoDataConfig::CANSEE_ENABLE.load();
		geoEnabled = configs::main::GeoDataConfig::GEO_ENABLE.load();

		root = std::filesystem::temp_directory_path() / ("aion_gs_handlers_ai_core_geo_" + std::to_string(std::random_device()()));
		std::filesystem::create_directories(root / "data" / "geo");
		BigEndianWriter meshes;
		writeWall(meshes, "levels/test/wall.cgf", PHYSICAL);
		writeWall(meshes, "levels/test/wall_see_through.cgf", PHYSICAL_SEE_THROUGH);
		writeFile(root / "data" / "geo" / "models.mesh", meshes.bytes);
		BigEndianWriter geo;
		writePlacement(geo, "levels/test/wall.cgf", 502.5f, 520, 100);
		writePlacement(geo, "levels/test/wall_see_through.cgf", 502.5f, 540, 100);
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
			AI_TEST_SCOPE;
			configs::main::GeoDataConfig::GEO_ENABLE.store(false);
			world::geo::GeoService::getInstance().init(); // empty GeoMaps: what a server with geo data off has
		}
		configs::main::GeoDataConfig::GEO_ENABLE.store(geoEnabled);
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(canSeeEnabled);
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
		QuestNpcAiWorldTest::TearDown();
	}

	/** the CREATURE_AGGRO events one CREATURE_SEE of the npc made */
	static int32_t aggroesOf(RecordingAbyssGuard& ai, Npc& npc) {
		ai.aggroes = 0;
		ai.onCreatureEvent(AIEventType::CREATURE_SEE, npc);
		return ai.aggroes;
	}

	std::filesystem::path root;
	bool canSeeEnabled = false;
	bool geoEnabled = false;
};

/**
 * Jucleas and an enemy Balder 5 m apart, well inside the aggro range of 7 m, on each lane: the wall that blocks the sight is the only reason the
 * guard leaves the npc alone. The can-see check is on, as the server default has it (gameserver.geodata.cansee.enable).
 */
TEST_F(AbyssGuardGeoTest, AWallThatBlocksTheSightKeepsTheGuardFromAnEnemyNpcInRange) {
	AI_TEST_SCOPE;
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(true);
	runtime::Ref<Npc> walledGuard = makeWorldNpc(JUCLEAS, 500, 520, 100);
	runtime::Ref<Npc> walledBalder = makeWorldNpc(BALDER, 505, 520, 100);
	runtime::Ref<Npc> seeThroughGuard = makeWorldNpc(JUCLEAS, 500, 540, 100);
	runtime::Ref<Npc> seeThroughBalder = makeWorldNpc(BALDER, 505, 540, 100);
	runtime::Ref<Npc> freeGuard = makeWorldNpc(JUCLEAS, 500, 560, 100);
	runtime::Ref<Npc> freeBalder = makeWorldNpc(BALDER, 505, 560, 100);
	RecordingAbyssGuard& walled = installLeaf<RecordingAbyssGuard>(*walledGuard);
	RecordingAbyssGuard& seeThrough = installLeaf<RecordingAbyssGuard>(*seeThroughGuard);
	RecordingAbyssGuard& free = installLeaf<RecordingAbyssGuard>(*freeGuard);

	world::geo::GeoService& geo = world::geo::GeoService::getInstance();
	ASSERT_FALSE(geo.canSee(*walledGuard, *walledBalder)) << "the fixture's physical wall blocks the sight";
	ASSERT_TRUE(geo.canSee(*seeThroughGuard, *seeThroughBalder));
	ASSERT_TRUE(walledGuard->getPosition()->isMapRegionActive());

	EXPECT_EQ(aggroesOf(walled, *walledBalder), 0) << "behind the physical wall: GeoService.canSee(owner, npc) is false";
	EXPECT_EQ(aggroesOf(seeThrough, *seeThroughBalder), 1) << "a see-through wall does not hide it";
	EXPECT_EQ(aggroesOf(free, *freeBalder), 1) << "no wall";

	// with the can-see check off (GeoDataConfig) GeoService.canSee answers true and the same walled pair is aggroed
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
	EXPECT_EQ(aggroesOf(walled, *walledBalder), 1) << "the wall was the only reason";
}

} // namespace
} // namespace aion::gameserver::ai::testing
