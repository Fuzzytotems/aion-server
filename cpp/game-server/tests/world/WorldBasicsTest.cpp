// World chunk (P4-10) basics: region id arithmetic, the WorldMapType and ZoneAttributes companions, interned zone names and WorldPosition's
// Java equals/hashCode/toString. Expectations are hand-derived from the Java sources (RegionUtil.java, WorldMapType.java, ZoneAttributes.java,
// ZoneName.java: String.hashCode, WorldPosition.java).

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/spawnengine/WalkerGroupShift.h"
#include "aion/gameserver/world/RegionUtil.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneAttributesInfo.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::world {
namespace {

class RegionUtilTest : public ::testing::Test {
protected:
	void SetUp() override { configs::main::WorldConfig::WORLD_REGION_SIZE.store(128); }
};

TEST_F(RegionUtilTest, TwoDimensionalIds) {
	EXPECT_EQ(RegionUtil::get2DRegionId(128, 0, 0), 0);
	EXPECT_EQ(RegionUtil::get2DRegionId(128, 127.9f, 128.0f), 1);
	EXPECT_EQ(RegionUtil::get2DRegionId(128, 1023.5f, 300), 7 * 1000 + 2);
	EXPECT_EQ(RegionUtil::get2dRegionId(1024, 1024), 8008);
	// Java (int) casts truncate toward zero and integer division too: -1f -> 0, -128f -> -1
	EXPECT_EQ(RegionUtil::get2dRegionId(-1, 5), 0);
	EXPECT_EQ(RegionUtil::get2dRegionId(-128, 128), -1000 + 1);
	EXPECT_EQ(RegionUtil::get2dRegionId(128, -128), 1000 - 1);
	// Java saturates out-of-range and NaN casts
	EXPECT_EQ(RegionUtil::get2DRegionId(128, std::numeric_limits<float>::quiet_NaN(), 0), 0);
	EXPECT_EQ(RegionUtil::getXFrom2dRegionId(7002), 7 * 128);
	EXPECT_EQ(RegionUtil::getYFrom2dRegionId(7002), 2 * 128);
}

TEST_F(RegionUtilTest, ThreeDimensionalIds) {
	int32_t id = RegionUtil::get3dRegionId(300, 520, 1000);
	EXPECT_EQ(id, 2 * 1000000 + 4 * 1000 + 7);
	EXPECT_EQ(RegionUtil::getXFrom3dRegionId(id), 256);
	EXPECT_EQ(RegionUtil::getYFrom3dRegionId(id), 512);
	EXPECT_EQ(RegionUtil::getZFrom3dRegionId(id), 896);
	EXPECT_EQ(RegionUtil::get3DRegionId(256, 300, 520, 1000), 1 * 1000000 + 2 * 1000 + 3);
}

TEST(WorldMapTypeInfoTest, ConstructorDataAndLookups) {
	EXPECT_EQ(getId(WorldMapType::RESHANTA), 400010000);
	EXPECT_EQ(getId(WorldMapType::PANDAEMONIUM), 120010000);
	EXPECT_EQ(getId(WorldMapType::STONESPEAR_REACH), 301500000);
	EXPECT_FALSE(isPersonal(WorldMapType::ORIEL));
	EXPECT_TRUE(isPersonal(WorldMapType::HOUSING_IDDF_PERSONAL));
	EXPECT_EQ(getWorldMapType(210010000), WorldMapType::POETA);
	EXPECT_EQ(getWorldMapType(1), std::nullopt);
	EXPECT_EQ(worldMapTypeOf("Silentera Canyon"), WorldMapType::SILENTERA_CANYON);
	EXPECT_EQ(worldMapTypeOf("test_idarena"), WorldMapType::Test_IDArena);
	EXPECT_EQ(worldMapTypeOf("nowhere"), std::nullopt);
	EXPECT_EQ(getWorldMapTypeMapId("poeta"), 210010000);
	EXPECT_EQ(getWorldMapTypeMapId("nowhere"), 0);
	EXPECT_TRUE(isPanesterraMap(400030000));  // TRANSIDIUM_ANNEX
	EXPECT_FALSE(isPanesterraMap(400010000)); // RESHANTA
	EXPECT_FALSE(isPanesterraMap(1));         // Java: case null -> false
}

TEST(ZoneAttributesInfoTest, IdsAndFromList) {
	EXPECT_EQ(getId(zone::ZoneAttributes::BIND), 1);
	EXPECT_EQ(getId(zone::ZoneAttributes::FLY), 8);
	EXPECT_EQ(getId(zone::ZoneAttributes::PVP_ENABLED), 64);
	EXPECT_EQ(getId(zone::ZoneAttributes::NO_RETURN_BATTLE), 512);
	EXPECT_EQ(zone::fromList({}), 0);
	EXPECT_EQ(zone::fromList({zone::ZoneAttributes::GLIDE, zone::ZoneAttributes::BIND, zone::ZoneAttributes::GLIDE}), 1 | 4);
}

TEST(ZoneNameTest, InterningAndJavaHashCodes) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const zone::ZoneName* none = zone::ZoneName::NONE;
	ASSERT_NE(none, nullptr);
	EXPECT_EQ(none->name(), "NONE");
	EXPECT_EQ(zone::ZoneName::get("none"), none);
	EXPECT_EQ(none->id(), 2402104); // "NONE".hashCode()

	const zone::ZoneName* poeta = zone::ZoneName::createOrGet("fly_area_210010000");
	EXPECT_EQ(poeta->name(), "FLY_AREA_210010000");
	EXPECT_EQ(zone::ZoneName::createOrGet("FLY_AREA_210010000"), poeta) << "interned by the upper-case name";
	EXPECT_EQ(zone::ZoneName::get("Fly_Area_210010000"), poeta);
	EXPECT_EQ(poeta->id(), -716927284);
	EXPECT_EQ(zone::ZoneName::getId("fly_area_210010000"), -716927284);
	EXPECT_EQ(zone::ZoneName::getId("NOT_INTERNED_ANYWHERE"), 2402104) << "unknown names get the id of NONE";
	EXPECT_EQ(zone::ZoneName::get("NOT_INTERNED_ANYWHERE"), none) << "get does not intern (it logs Missing zone)";
}

TEST(WorldPositionTest, JavaEqualsHashCodeAndToString) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	runtime::Ref<WorldPosition> a = WorldPosition::create(210010000, 1.5f, 2.0f, -0.0f, int8_t{-3});
	runtime::Ref<WorldPosition> b = WorldPosition::create(210010000, 1.5f, 2.0f, 0.0f, int8_t{-3});
	EXPECT_TRUE(a->equals(*b)) << "Java compares the floats with ==, so -0.0 equals 0.0";
	// Java: 31 * (31 * (31 * (31 * (31 + heading) + mapId) + floatToIntBits(x)) + floatToIntBits(y)) + floatToIntBits(z), int arithmetic
	uint32_t expected = 1;
	expected = 31 * expected + static_cast<uint32_t>(-3);
	expected = 31 * expected + 210010000u;
	expected = 31 * expected + 0x3FC00000u; // 1.5f
	expected = 31 * expected + 0x40000000u; // 2.0f
	expected = 31 * expected + 0x80000000u; // -0.0f
	EXPECT_EQ(a->hashCode(), static_cast<int32_t>(expected));
	EXPECT_NE(a->hashCode(), b->hashCode()) << "floatToIntBits keeps the sign of zero";
	EXPECT_EQ(a->toString(), "WorldPosition [mapId=210010000, x=1.5, y=2.0, z=-0.0, heading=-3, isSpawned=false]");
	EXPECT_EQ(a->toCoordString(), "Map ID: 210010000, Instance ID: 1, X: 1.5, Y: 2.0, Z: -0.0, Heading: -3") << "no map region: instance 1";
	EXPECT_EQ(a->getInstanceId(), 1);
	EXPECT_FALSE(a->isInstanceMap());
	EXPECT_FALSE(a->isMapRegionActive());
	a->setXYZH(std::nullopt, 7.0f, std::nullopt, int8_t{10});
	EXPECT_EQ(a->getX(), 1.5f);
	EXPECT_EQ(a->getY(), 7.0f);
	EXPECT_EQ(a->getHeading(), 10);
}

// ---- WalkerGroup.getLinePoint ------------------------------------------------------------------------------------------------------------------

TEST(WalkerGroupTest, LinePointsFollowJavasFloatAndDoubleArithmetic) {
	// expectations: an independent evaluation of WalkerGroup.java:122-176 with IEEE single precision for the float expressions and double for
	// slope and dx (the float quotient widened to double, Math.sqrt, the (float) casts), printed as hex floats
	struct Case {
		float ox, oy, dx, dy, sagittal, coronal, x, y;
	};
	const std::vector<Case> cases = {
		{10, 20, 30, 20, 2, 1, 0x1.4p+3f, 0x1.2p+4f},       // same y: the sagittal shift moves y, the coronal shift is ignored (dy sign 0)
		{10, 20, 10, 5, -2, 3, 0x1.8p+3f, 0x1.1p+4f},       // same x: both shifts follow the y direction sign
		{0, 0, 3, 4, 2, 0, 0x1.99999ap+0f, -0x1.333334p+0f}, // slope 0.75 as double
		{100.5f, 50.25f, 90, 70, -2, -2, 0x1.8eb11p+6f, 0x1.7c5c92p+5f},
		{1, 1, 5, 2, 0, 2, 0x1.785b42p+1f, 0x1.7c2da2p+0f},  // coronal only: the rotated point of a sagittal shift of |coronal|
		{7.3f, -4.1f, 2.2f, -9.9f, 2, 4, 0x1.940f4p+1f, -0x1.722018p+2f},
	};
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	for (const Case& c : cases) {
		runtime::Ref<spawnengine::WalkerGroupShift> shift = spawnengine::WalkerGroupShift::create(c.sagittal, c.coronal);
		model::templates::zone::Point2D point =
			spawnengine::WalkerGroup::getLinePoint(model::templates::zone::Point2D(c.ox, c.oy), model::templates::zone::Point2D(c.dx, c.dy), *shift);
		EXPECT_EQ(point.getX(), c.x) << c.ox << "," << c.oy << " -> " << c.dx << "," << c.dy << " shift " << c.sagittal << "," << c.coronal;
		EXPECT_EQ(point.getY(), c.y) << c.ox << "," << c.oy << " -> " << c.dx << "," << c.dy << " shift " << c.sagittal << "," << c.coronal;
	}
}

} // namespace
} // namespace aion::gameserver::world
