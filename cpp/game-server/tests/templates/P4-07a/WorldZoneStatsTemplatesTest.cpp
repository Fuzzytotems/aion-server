// P4-07a world, zone and stats templates: WorldMapTemplate's flags hook and config-limited twin counts, the weather table lookups, the zone shapes
// and ZoneTemplate's name setter (ZoneName interning belongs to P4-10: those checks are skipped while ZoneName::createOrGet is unported), the
// map zone of WorldZoneTemplate, and StatsTemplate's speed and attribute defaults.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.bind.h"
#include "aion/gameserver/model/templates/world/WeatherTable.bind.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.bind.h"
#include "aion/gameserver/model/templates/zone/Cylinder.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/model/templates/zone/Semisphere.h"
#include "aion/gameserver/model/templates/zone/Sphere.h"
#include "aion/gameserver/model/templates/zone/WorldZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.bind.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::model::templates {
namespace {

template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

/** ZoneName::createOrGet (P4-10) is still AION_UNPORTED: zone name checks skip until it is ported */
bool zoneNamesArePorted() {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	try {
		::aion::gameserver::world::zone::ZoneName::createOrGet("P4_07A_TEST_ZONE");
		return true;
	} catch (const runtime::UnportedException&) {
		return false;
	}
}

/** restores a WorldConfig value at the end of a test */
class ConfigValue {
public:
	explicit ConfigValue(std::atomic<int32_t>& field) : field(field), saved(field.load()) {}
	~ConfigValue() { field.store(saved); }

private:
	std::atomic<int32_t>& field;
	int32_t saved;
};

// ---- world ---------------------------------------------------------------------------------------------------------------------------------

TEST(WorldMapTemplateTest, HookCombinesTheZoneAttributeFlags) {
	std::unique_ptr<world::WorldMapTemplate> map = bindXml<world::WorldMapTemplate>(
		R"(<map id="210010000" cName="LF1" name_id="5" death_level="100" water_level="80" flags="BIND FLY PVP NO_RETURN_BATTLE">)"
		R"(<ai_info chase_target="60"/></map>)");
	// ids: BIND 1, RECALL 2, GLIDE 4, FLY 8, RIDE 16, FLY_RIDE 32, PVP_ENABLED 64, DUEL_SAME_RACE_ENABLED 128, DUEL_OTHER_RACE_ENABLED 256,
	// NO_RETURN_BATTLE 512 (@XmlEnumValue "PVP" for PVP_ENABLED)
	EXPECT_EQ(map->getFlags(), 1 + 8 + 64 + 512);
	EXPECT_TRUE(map->canPutKisk());
	EXPECT_TRUE(map->isFly());
	EXPECT_TRUE(map->isPvpAllowed());
	EXPECT_TRUE(map->canReturnToBattle());
	EXPECT_FALSE(map->canRecall());
	EXPECT_FALSE(map->canGlide());
	EXPECT_FALSE(map->canRide());
	EXPECT_FALSE(map->canFlyRide());
	EXPECT_FALSE(map->isSameRaceDuelsAllowed());
	EXPECT_FALSE(map->isOtherRaceDuelsAllowed());
	EXPECT_EQ(map->getName(), "LF1") << "name absent: cName";
	EXPECT_EQ(map->getL10nId(), 5);
	EXPECT_EQ(map->getAiInfo()->getChaseTarget(), 60);
	EXPECT_EQ(map->getAiInfo()->getChaseHome(), 200);

	std::unique_ptr<world::WorldMapTemplate> empty =
		bindXml<world::WorldMapTemplate>(R"(<map id="1" cName="X" name="Named" death_level="0" water_level="0" flags=""/>)");
	EXPECT_EQ(empty->getFlags(), 0) << "present-empty flags";
	EXPECT_EQ(empty->getName(), "Named");
	EXPECT_EQ(empty->getAiInfo(), &world::AiInfo::DEFAULT) << "AiInfo.DEFAULT";

	EXPECT_THROW(bindXml<world::WorldMapTemplate>(R"(<map id="1" cName="X" death_level="0" water_level="0"/>)"), xml::StaticDataException)
		<< "Java: NullPointerException in ZoneAttributes.fromList";
}

TEST(WorldMapTemplateTest, TwinCountsFollowWorldConfig) {
	ConfigValue usual(configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL);
	ConfigValue beginner(configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER);
	std::unique_ptr<world::WorldMapTemplate> map =
		bindXml<world::WorldMapTemplate>(R"(<map id="1" cName="X" death_level="0" water_level="0" flags="" twin_count="5" beginner_twin_count="3"/>)");
	configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(0);
	configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(0);
	EXPECT_EQ(map->getTwinCount(), 5) << "0: no limit";
	EXPECT_EQ(map->getBeginnerTwinCount(), 3);
	configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(2);
	configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(-1);
	EXPECT_EQ(map->getTwinCount(), 2);
	EXPECT_EQ(map->getBeginnerTwinCount(), 0) << "-1: disabled";
	configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(4);
	EXPECT_EQ(map->getBeginnerTwinCount(), 3);
}

TEST(WeatherTableTest, WeatherAfterAndWeathersForZone) {
	std::unique_ptr<world::WeatherTable> table = bindXml<world::WeatherTable>(R"(<weather_table id="210010000" weather_count="4" zone_count="2">)"
		R"(<table zone_id="1" code="1" rank="1" name="rain" before="true"/><table zone_id="1" code="2" rank="1" name="rain"/>)"
		R"(<table zone_id="1" code="3" rank="1" name="rain" after="true"/><table zone_id="2" code="4" rank="1" name="rain"/>)"
		R"(<table zone_id="2" code="5" rank="1"/></weather_table>)");
	const auto& entries = table->getZoneData();
	ASSERT_EQ(entries.size(), 5u);
	EXPECT_EQ(table->getWeatherAfter(entries[0].get()), entries[1].get()) << "before -> plain";
	EXPECT_EQ(table->getWeatherAfter(entries[1].get()), entries[2].get()) << "plain -> after";
	EXPECT_EQ(table->getWeatherAfter(entries[2].get()), nullptr) << "after has no successor";
	EXPECT_EQ(table->getWeatherAfter(entries[3].get()), nullptr) << "no after entry in zone 2";
	EXPECT_EQ(table->getWeatherAfter(entries[4].get()), nullptr) << "no name";
	EXPECT_EQ(table->getWeatherAfter(nullptr), nullptr);
	EXPECT_EQ(table->getWeathersForZone(2), (std::vector<const world::WeatherEntry*>{entries[3].get(), entries[4].get()}));
	EXPECT_TRUE(table->getWeathersForZone(9).empty());
	world::WeatherEntry created(7, 3);
	EXPECT_EQ(created.getZoneId(), 7);
	EXPECT_EQ(created.getCode(), 3);
	EXPECT_EQ(world::WeatherEntry::NONE.getCode(), 0);
	EXPECT_EQ(world::WeatherEntry::NONE.getWeatherName(), "");
}

// ---- zone ------------------------------------------------------------------------------------------------------------------------------------

TEST(ZoneTemplateTest, ShapeConstructors) {
	zone::Cylinder cylinder(1, 2, 3, 40, -5);
	EXPECT_EQ(cylinder.getX(), 1.0f);
	EXPECT_EQ(cylinder.getY(), 2.0f);
	EXPECT_EQ(cylinder.getR(), 3.0f);
	EXPECT_EQ(cylinder.getTop(), 40.0f);
	EXPECT_EQ(cylinder.getBottom(), -5.0f);
	EXPECT_EQ(zone::Cylinder().getTop(), std::nullopt) << "Float null";
	zone::Semisphere semisphere(1, 2, 3, 4);
	EXPECT_EQ(semisphere.getZ(), 3.0f);
	EXPECT_EQ(semisphere.getR(), 4.0f);
	zone::Sphere sphere(5, 6, 7, 8);
	EXPECT_EQ(sphere.getX(), 5.0f);
	zone::Points points(-1, 10);
	EXPECT_EQ(points.getBottom(), -1.0f);
	EXPECT_EQ(points.getTop(), 10.0f);
	EXPECT_TRUE(points.getPoint().empty());
	zone::Point2D point(3, 4);
	EXPECT_EQ(point.getX(), 3.0f);
	EXPECT_EQ(point.getY(), 4.0f);
}

TEST(ZoneTemplateTest, NameSetterInternsTheZoneName) {
	if (!zoneNamesArePorted())
		GTEST_SKIP() << "ZoneName::createOrGet (P4-10) is not ported yet";
	std::unique_ptr<zone::ZoneTemplate> zone = bindXml<zone::ZoneTemplate>(
		R"(<zone name="DF1_ZONE" mapid="220010000" priority="2" area_type="CYLINDER" zone_type="FLY">)"
		R"(<cylinder x="1" y="2" r="3" top="4" bottom="5"/></zone>)");
	ASSERT_NE(zone->getName(), nullptr);
	EXPECT_EQ(zone->getXmlName(), zone->getName()->name());
	EXPECT_EQ(zone->getMapid(), 220010000);
	EXPECT_EQ(zone->getFlags(), -1);
	EXPECT_EQ(zone->getAreaType(), zone::AreaType::CYLINDER);
}

TEST(ZoneTemplateTest, WorldZonePointsFollowJavaArithmetic) {
	auto corners = [](const zone::Points& points) {
		std::vector<std::pair<float, float>> result;
		for (const zone::Point2D& point : points.getPoint())
			result.emplace_back(point.getX(), point.getY());
		return result;
	};
	// maxZ = Math.round(1000f / 128) * 128 = 8 * 128 (7.8125 rounds up)
	std::unique_ptr<zone::Points> points = zone::WorldZoneTemplate::createMapPoints(1000, 128);
	EXPECT_EQ(points->getBottom(), -1.0f);
	EXPECT_EQ(points->getTop(), 1025.0f);
	EXPECT_EQ(corners(*points), (std::vector<std::pair<float, float>>{{-1.0f, -1.0f}, {-1.0f, 1001.0f}, {1001.0f, 1001.0f}, {1001.0f, -1.0f}}));
	// 3072f / 128 = 24 exactly; a real map size
	EXPECT_EQ(zone::WorldZoneTemplate::createMapPoints(3072, 128)->getTop(), 3073.0f);
	// 320f / 128 = 2.5: Math.round rounds half up -> 3 * 128 (not banker's rounding to 2)
	EXPECT_EQ(zone::WorldZoneTemplate::createMapPoints(320, 128)->getTop(), 385.0f);
	// 191f / 128 = 1.4921875 -> 1
	EXPECT_EQ(zone::WorldZoneTemplate::createMapPoints(191, 128)->getTop(), 129.0f);
	// -320f / 128 = -2.5: half up -> -2 * 128; corners use size + 1 = -319
	std::unique_ptr<zone::Points> negative = zone::WorldZoneTemplate::createMapPoints(-320, 128);
	EXPECT_EQ(negative->getTop(), -255.0f);
	EXPECT_EQ(corners(*negative), (std::vector<std::pair<float, float>>{{-1.0f, -1.0f}, {-1.0f, -319.0f}, {-319.0f, -319.0f}, {-319.0f, -1.0f}}));
	// region size 0: 1000f / 0 = Infinity, Math.round saturates to Integer.MAX_VALUE, times 0 is 0
	EXPECT_EQ(zone::WorldZoneTemplate::createMapPoints(1000, 0)->getTop(), 1.0f);
	// the int product wraps: Math.round(2147483647f / 3) = 715827904 (the float quotient), * 3 = 2147483712 wraps to -2147483584, which widens
	// to the float -2147483648 (a tie, rounded to even); + 1 stays -2147483648
	EXPECT_EQ(zone::WorldZoneTemplate::createMapPoints(2147483647, 3)->getTop(), -2147483648.0f);
	// size + 1 wraps to Integer.MIN_VALUE before the widening
	EXPECT_EQ(zone::WorldZoneTemplate::createMapPoints(2147483647, 3)->getPoint()[2].getX(), -2147483648.0f);
}

TEST(ZoneTemplateTest, WorldZoneTemplateCoversTheMap) {
	if (!zoneNamesArePorted())
		GTEST_SKIP() << "ZoneName::createOrGet (P4-10) is not ported yet";
	ConfigValue regionSize(configs::main::WorldConfig::WORLD_REGION_SIZE);
	configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	zone::WorldZoneTemplate zone(1000, 210010000, 77);
	// maxZ = Math.round(1000f / 128) * 128 = 8 * 128 (7.8125 rounds up)
	ASSERT_NE(zone.getPoints(), nullptr);
	EXPECT_EQ(zone.getPoints()->getTop(), 1025.0f);
	EXPECT_EQ(zone.getPoints()->getBottom(), -1.0f);
	std::vector<std::pair<float, float>> corners;
	for (const zone::Point2D& point : zone.getPoints()->getPoint())
		corners.emplace_back(point.getX(), point.getY());
	EXPECT_EQ(corners, (std::vector<std::pair<float, float>>{{-1.0f, -1.0f}, {-1.0f, 1001.0f}, {1001.0f, 1001.0f}, {1001.0f, -1.0f}}));
	EXPECT_EQ(zone.getZoneType(), zone::ZoneClassName::DUMMY);
	EXPECT_EQ(zone.getMapid(), 210010000);
	EXPECT_EQ(zone.getFlags(), 77);
	EXPECT_EQ(zone.getXmlName(), "210010000");
}

// ---- stats -------------------------------------------------------------------------------------------------------------------------------------

TEST(StatsTemplateTest, SpeedsAndAttributeDefaults) {
	std::unique_ptr<stats::StatsTemplate> withSpeeds =
		bindXml<stats::StatsTemplate>(R"(<stats maxHp="100"><speeds walk="1.5" run="6" group_walk="1.2" run_fight="7" group_run_fight="6.5" fly="9"/>)"
			R"(</stats>)");
	EXPECT_EQ(withSpeeds->getMaxHp(), 100);
	EXPECT_FLOAT_EQ(withSpeeds->getWalkSpeed(), 1.5f);
	EXPECT_FLOAT_EQ(withSpeeds->getRunSpeed(), 6.0f);
	EXPECT_FLOAT_EQ(withSpeeds->getGroupWalkSpeed(), 1.2f);
	EXPECT_FLOAT_EQ(withSpeeds->getRunSpeedFight(), 7.0f);
	EXPECT_FLOAT_EQ(withSpeeds->getGroupRunSpeedFight(), 6.5f);
	EXPECT_FLOAT_EQ(withSpeeds->getFlySpeed(), 9.0f);
	std::unique_ptr<stats::StatsTemplate> noSpeeds = bindXml<stats::StatsTemplate>(R"(<stats/>)");
	EXPECT_EQ(noSpeeds->getRunSpeed(), 0.0f) << "speeds null: 0";
	EXPECT_EQ(noSpeeds->getPower(), 100);
	EXPECT_EQ(noSpeeds->getHealth(), 100);
	EXPECT_EQ(noSpeeds->getAgility(), 100);
	EXPECT_EQ(noSpeeds->getBaseAccuracy(), 100);
	EXPECT_EQ(noSpeeds->getKnowledge(), 100);
	EXPECT_EQ(noSpeeds->getWill(), 100);
	EXPECT_EQ(noSpeeds->getStunLikeResistance(), 0);
	noSpeeds->setStunLikeResistance(35);
	EXPECT_EQ(noSpeeds->getStunLikeResistance(), 35);
}

/** A run-time subclass like Java's PlayerClass.PlayerStatsTemplate, which overrides the speed and base attribute getters */
class OverridingStatsTemplate final : public stats::StatsTemplate {
public:
	float getWalkSpeed() const override { return 1.3f; }
	float getRunSpeed() const override { return 6.2f; }
	float getFlySpeed() const override { return 9.1f; }
	int32_t getPower() const override { return 110; }
	int32_t getHealth() const override { return 111; }
	int32_t getAgility() const override { return 112; }
	int32_t getBaseAccuracy() const override { return 113; }
	int32_t getKnowledge() const override { return 114; }
	int32_t getWill() const override { return 115; }
};

TEST(StatsTemplateTest, OverriddenGettersDispatchThroughTheBase) {
	// header request templates-1: PlayerGameStats reads the player's template through a StatsTemplate pointer
	std::unique_ptr<stats::StatsTemplate> playerTemplate = std::make_unique<OverridingStatsTemplate>();
	const stats::StatsTemplate& base = *playerTemplate;
	EXPECT_FLOAT_EQ(base.getWalkSpeed(), 1.3f);
	EXPECT_FLOAT_EQ(base.getRunSpeed(), 6.2f);
	EXPECT_FLOAT_EQ(base.getFlySpeed(), 9.1f);
	EXPECT_EQ(base.getPower(), 110);
	EXPECT_EQ(base.getHealth(), 111);
	EXPECT_EQ(base.getAgility(), 112);
	EXPECT_EQ(base.getBaseAccuracy(), 113);
	EXPECT_EQ(base.getKnowledge(), 114);
	EXPECT_EQ(base.getWill(), 115);
	EXPECT_EQ(base.getGroupWalkSpeed(), 0.0f) << "not overridden in Java: the base getter";
}

} // namespace
} // namespace aion::gameserver::model::templates
