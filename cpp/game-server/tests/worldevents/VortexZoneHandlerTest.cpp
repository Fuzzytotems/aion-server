// P5-12b vortex zone handlers, M5a subset (m5a-plan.md X-01b): the ZoneHandler bodies every creature of an invasion zone runs while
// SpawnEngine.spawnAll brings the world into existence, with the vortex disabled (plan D1) so no DimensionalVortex exists.
//
// Expectations are derived by hand from VortexLocation.java:124-200 (isInsideLocation, onEnterZone, onLeaveZone) and the two vortex_location
// entries of data/static_data/vortex/dimensional_vortex.xml. The zone argument is unused by the handler, so the tests drive it the way
// ZoneInstance.onEnter and ZoneInstance.onLeave do (ZoneInstance.java:64-89) without the controller hooks of a real zone transition.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/templates/vortex/VortexTemplate.bind.h"
#include "aion/gameserver/model/templates/vortex/VortexTemplate.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/model/templates/zone/WorldZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/zone/InvasionZoneInstance.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::vortex::VortexLocation;

/** An InvasionZoneInstance whose creature map the test fills directly: ZoneInstance.onEnter would also run the player controller hooks */
class TestInvasionZone final : public world::zone::InvasionZoneInstance {
	AION_MAKE_REF_FRIEND
public:
	TestInvasionZone(int32_t mapId, model::templates::zone::ZoneInfo& template_) : InvasionZoneInstance(mapId, template_) {}

	/** the line of ZoneInstance::onEnter that makes isInsideCreature true */
	void holdCreature(model::gameobjects::Creature& creature) {
		creatures.put(creature.getObjectId(), runtime::Ref<model::gameobjects::Creature>(creature));
	}

	void releaseCreature(model::gameobjects::Creature& creature) { creatures.remove(creature.getObjectId()); }

protected:
	~TestInvasionZone() override = default;
};

class VortexZoneHandlerTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		regionSize.emplace(configs::main::WorldConfig::WORLD_REGION_SIZE, 128);
		// vortex_location id="0" of dimensional_vortex.xml
		vortexTemplate = bindXml<model::templates::vortex::VortexTemplate>(
			R"(<vortex_location id="0" defends_race="ELYOS" offence_race="ASMODIANS">)"
			R"(<home_point map="120080000" x="559.4" y="207.8" z="93.5" h="0"/>)"
			R"(<resurrection_point map="210060000" x="951.0" y="2433.0" z="107.0" h="0"/>)"
			R"(<start_point map="210060000" x="951.0" y="2433.0" z="107.0" h="0"/></vortex_location>)");
		zoneTemplate = std::make_unique<model::templates::zone::WorldZoneTemplate>(3000, 210060000, 0);
		area = model::geometry::PolyArea::create(zoneTemplate->getName(), 210060000, zoneTemplate->getPoints()->getPoint(),
			zoneTemplate->getPoints()->getBottom(), zoneTemplate->getPoints()->getTop());
		zoneInfo = model::templates::zone::ZoneInfo::create(*area, zoneTemplate.get());
		zone = runtime::makeRef<TestInvasionZone>(210060000, *zoneInfo);
	}

	void TearDown() override {
		zone = nullptr;
		zoneInfo = nullptr;
		area = nullptr;
		zoneTemplate.reset();
		vortexTemplate.reset();
		regionSize.reset();
		PlayerEventsTest::TearDown();
	}

	std::optional<AtomicConfigScope<int32_t>> regionSize;
	std::unique_ptr<model::templates::vortex::VortexTemplate> vortexTemplate;
	std::unique_ptr<model::templates::zone::WorldZoneTemplate> zoneTemplate;
	runtime::Ref<model::geometry::PolyArea> area;
	runtime::Ref<model::templates::zone::ZoneInfo> zoneInfo;
	runtime::Ref<TestInvasionZone> zone;
};

TEST_F(VortexZoneHandlerTest, TheRacesAndTheIdComeFromTheTemplate) {
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	runtime::resetUnportedHitsForTests();

	EXPECT_EQ(location->getId(), 0);
	EXPECT_EQ(location->getDefendersRace(), model::Race::ELYOS);
	EXPECT_EQ(location->getInvadersRace(), model::Race::ASMODIANS);
	EXPECT_EQ(location->getHomeWorldId(), 120080000);
	EXPECT_EQ(location->getInvasionWorldId(), 210060000);
	EXPECT_FALSE(location->isActive()) << "no DimensionalVortex was set";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);

	// a vortex_location without the races: Java would return null and never dereference it, C++ reports the missing attribute
	std::unique_ptr<model::templates::vortex::VortexTemplate> bare =
		bindXml<model::templates::vortex::VortexTemplate>(R"(<vortex_location id="7"/>)");
	runtime::Ref<VortexLocation> bareLocation = VortexLocation::create(bare.get());
	EXPECT_THROW(bareLocation->getInvadersRace(), runtime::NullPointerException);
	EXPECT_THROW(bareLocation->getDefendersRace(), runtime::NullPointerException);

	// Java compares with race.equals(getInvadersRace()) (VortexLocation.java:140, :164), which is false for a null race and never throws: the
	// zone handlers go through isInvadersRace, so a hand-edited vortex file cannot abort the spawn path that has no catch
	EXPECT_TRUE(location->isInvadersRace(model::Race::ASMODIANS));
	EXPECT_FALSE(location->isInvadersRace(model::Race::ELYOS));
	EXPECT_FALSE(bareLocation->isInvadersRace(model::Race::ASMODIANS));
	EXPECT_FALSE(bareLocation->isInvadersRace(model::Race::ELYOS));
	PlayerFixture f = makePlayer(9, 109, model::Race::ASMODIANS);
	EXPECT_NO_THROW(bareLocation->onEnterZone(*f.player, *zone));
	EXPECT_EQ(bareLocation->getPlayers().size(), 1);
}

TEST_F(VortexZoneHandlerTest, AnInactiveVortexOnlyKeepsThePlayerBookkeeping) {
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	location->addZone(*zone);
	PlayerFixture f = makePlayer(1, 100, model::Race::ASMODIANS); // the invaders race of this location
	runtime::resetUnportedHitsForTests();

	location->onEnterZone(*f.player, *zone);
	EXPECT_EQ(location->getPlayers().size(), 1);
	EXPECT_TRUE(location->getInvadersKisks().isEmpty());

	location->onEnterZone(*f.player, *zone); // Java: the containsKey guard skips the second put
	EXPECT_EQ(location->getPlayers().size(), 1);

	// isActive() is false, so neither the vortex controller nor the active vortex is touched: no unported body is reached
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(VortexZoneHandlerTest, LeavingRemovesThePlayerOnlyWhenItIsOutsideEveryZone) {
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	location->addZone(*zone);
	PlayerFixture f = makePlayer(2, 200, model::Race::ELYOS);
	location->onEnterZone(*f.player, *zone);
	zone->holdCreature(*f.player);
	runtime::resetUnportedHitsForTests();

	EXPECT_TRUE(location->isInsideLocation(*f.player));
	location->onLeaveZone(*f.player, *zone);
	EXPECT_EQ(location->getPlayers().size(), 1) << "still inside a zone of the location (VortexLocation.java:166)";

	zone->releaseCreature(*f.player);
	EXPECT_FALSE(location->isInsideLocation(*f.player));
	location->onLeaveZone(*f.player, *zone);
	EXPECT_TRUE(location->getPlayers().isEmpty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the kick timers are only scheduled while the vortex is active";
}

TEST_F(VortexZoneHandlerTest, ALocationWithoutZonesHoldsNobody) {
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	PlayerFixture f = makePlayer(3, 300);
	runtime::resetUnportedHitsForTests();

	EXPECT_FALSE(location->isInsideLocation(*f.player)) << "the empty zone list short-circuits (VortexLocation.java:125)";
	location->onEnterZone(*f.player, *zone);
	location->onLeaveZone(*f.player, *zone);
	EXPECT_TRUE(location->getPlayers().isEmpty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
