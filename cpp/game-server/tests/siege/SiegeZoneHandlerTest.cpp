// P5-12a siege zone handlers, M5a subset (m5a-plan.md X-01b): the ZoneHandler bodies every NPC of a siege zone runs while SpawnEngine.spawnAll
// brings the world into existence, plus the balance-buff guard they call.
//
// Expectations are derived by hand from SiegeLocation.java:209-227 (onEnterZone, onLeaveZone) and FortressLocation.java:51-110 (onEnterZone,
// onLeaveZone, checkForBalanceBuff). The zone argument is only forwarded, so the tests drive the handler the way ZoneInstance.onEnter and
// ZoneInstance.onLeave do (ZoneInstance.java:64-89) without the controller hooks of a real zone transition.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/zone/WorldZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::siege::FortressLocation;
using model::siege::SiegeLocation;
using model::siege::SiegeRace;
using model::templates::zone::ZoneType;

/** A SiegeZoneInstance whose creature map the test fills directly: ZoneInstance.onEnter would also run the player controller hooks */
class TestSiegeZone final : public world::zone::SiegeZoneInstance {
	AION_MAKE_REF_FRIEND
public:
	TestSiegeZone(int32_t mapId, model::templates::zone::ZoneInfo& template_) : SiegeZoneInstance(mapId, template_) {}

	/** the line of ZoneInstance::onEnter that makes isInsideCreature true */
	void holdCreature(model::gameobjects::Creature& creature) {
		creatures.put(creature.getObjectId(), runtime::Ref<model::gameobjects::Creature>(creature));
	}

	void releaseCreature(model::gameobjects::Creature& creature) { creatures.remove(creature.getObjectId()); }

protected:
	~TestSiegeZone() override = default;
};

class SiegeZoneHandlerTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		regionSize.emplace(configs::main::WorldConfig::WORLD_REGION_SIZE, 128);
		locationTemplate = bindXml<model::templates::siegelocation::SiegeLocationTemplate>(
			R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
		zoneTemplate = std::make_unique<model::templates::zone::WorldZoneTemplate>(3000, 400010000, 0);
		area = model::geometry::PolyArea::create(zoneTemplate->getName(), 400010000, zoneTemplate->getPoints()->getPoint(),
			zoneTemplate->getPoints()->getBottom(), zoneTemplate->getPoints()->getTop());
		zoneInfo = model::templates::zone::ZoneInfo::create(*area, zoneTemplate.get());
		zone = runtime::makeRef<TestSiegeZone>(400010000, *zoneInfo);
	}

	void TearDown() override {
		zone = nullptr;
		zoneInfo = nullptr;
		area = nullptr;
		zoneTemplate.reset();
		locationTemplate.reset();
		regionSize.reset();
		PlayerEventsTest::TearDown();
	}

	std::vector<int32_t> creatureIds(SiegeLocation& location) {
		std::vector<int32_t> ids;
		location.forEachCreature([&ids](model::gameobjects::Creature& creature) { ids.push_back(creature.getObjectId()); });
		return ids;
	}

	std::vector<int32_t> playerIds(SiegeLocation& location) {
		std::vector<int32_t> ids;
		location.forEachPlayer([&ids](model::gameobjects::player::Player& player) { ids.push_back(player.getObjectId()); });
		return ids;
	}

	std::optional<AtomicConfigScope<int32_t>> regionSize;
	std::unique_ptr<model::templates::siegelocation::SiegeLocationTemplate> locationTemplate;
	std::unique_ptr<model::templates::zone::WorldZoneTemplate> zoneTemplate;
	runtime::Ref<model::geometry::PolyArea> area;
	runtime::Ref<model::templates::zone::ZoneInfo> zoneInfo;
	runtime::Ref<TestSiegeZone> zone;
};

TEST_F(SiegeZoneHandlerTest, EnteringRegistersTheCreatureOnceAndPlayersTwice) {
	runtime::Ref<SiegeLocation> location = SiegeLocation::create(locationTemplate.get());
	PlayerFixture f = makePlayer(1, 100);
	runtime::resetUnportedHitsForTests();

	location->onEnterZone(*f.player, *zone);
	EXPECT_EQ(creatureIds(*location), (std::vector<int32_t>{f.player->getObjectId()}));
	EXPECT_EQ(playerIds(*location), (std::vector<int32_t>{f.player->getObjectId()})) << "a Player goes into both maps (SiegeLocation.java:211-215)";

	location->onEnterZone(*f.player, *zone); // Java: the containsKey guard skips the second put
	EXPECT_EQ(creatureIds(*location).size(), 1u);
	EXPECT_EQ(playerIds(*location).size(), 1u);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeZoneHandlerTest, LeavingRemovesTheCreatureOnlyWhenItIsOutsideEveryZoneOfTheLocation) {
	runtime::Ref<SiegeLocation> location = SiegeLocation::create(locationTemplate.get());
	location->addZone(*zone);
	PlayerFixture f = makePlayer(2, 200);
	location->onEnterZone(*f.player, *zone);
	zone->holdCreature(*f.player); // the creature left one zone but is still inside this one
	runtime::resetUnportedHitsForTests();

	location->onLeaveZone(*f.player, *zone);
	EXPECT_EQ(creatureIds(*location).size(), 1u) << "isInsideLocation is still true (SiegeLocation.java:220)";
	EXPECT_EQ(playerIds(*location).size(), 1u);

	zone->releaseCreature(*f.player);
	location->onLeaveZone(*f.player, *zone);
	EXPECT_TRUE(creatureIds(*location).empty());
	EXPECT_TRUE(playerIds(*location).empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeZoneHandlerTest, FortressCountsTheSiegeZoneTypeAndKeepsTheBaseBookkeeping) {
	runtime::Ref<FortressLocation> fortress = FortressLocation::create(locationTemplate.get());
	PlayerFixture f = makePlayer(3, 300);
	runtime::resetUnportedHitsForTests();

	fortress->onEnterZone(*f.player, *zone);
	EXPECT_TRUE(f.player->isInsideZoneType(ZoneType::SIEGE));
	EXPECT_EQ(creatureIds(*fortress), (std::vector<int32_t>{f.player->getObjectId()})) << "super.onEnterZone ran";

	fortress->onLeaveZone(*f.player, *zone);
	EXPECT_FALSE(f.player->isInsideZoneType(ZoneType::SIEGE)) << "unsetInsideZoneType (FortressLocation.java:67)";
	EXPECT_TRUE(creatureIds(*fortress).empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u)
		<< "the shield branch needs isUnderShield(), which is false while no siege runs, so ShieldService stays unreached";
}

TEST_F(SiegeZoneHandlerTest, TheBalanceBuffIsSkippedWhileTheFortressIsInvulnerableOrBalanced) {
	runtime::Ref<FortressLocation> fortress = FortressLocation::create(locationTemplate.get());
	PlayerFixture f = makePlayer(4, 400);
	runtime::resetUnportedHitsForTests();

	// invulnerable and balanced: both guards of FortressLocation.java:75 fail, so neither SkillEngine nor a packet is reached
	EXPECT_FALSE(fortress->isVulnerable());
	EXPECT_EQ(fortress->getFactionBalance(), 0);
	fortress->checkForBalanceBuff(*f.player, FortressLocation::SiegeBuffAction::ADD);
	fortress->checkForBalanceBuff(*f.player, FortressLocation::SiegeBuffAction::LEAVE_ZONE_REMOVE);
	fortress->checkForBalanceBuff(*f.player, FortressLocation::SiegeBuffAction::SIEGE_END_REMOVE);

	// a balanced but vulnerable fortress is skipped as well
	fortress->setVulnerable(true);
	fortress->checkForBalanceBuff(*f.player, FortressLocation::SiegeBuffAction::ADD);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeZoneHandlerTest, TheAddBranchWarnsTheFavouredRaceInsteadOfBuffingIt) {
	runtime::Ref<FortressLocation> fortress = FortressLocation::create(locationTemplate.get());
	PlayerFixture f = makePlayer(5, 500, model::Race::ELYOS);
	fortress->setVulnerable(true);
	fortress->setFactionBalance(3); // asmodians are handicapped, so an Elyos only gets the warning (FortressLocation.java:100)
	runtime::resetUnportedHitsForTests();

	fortress->checkForBalanceBuff(*f.player, FortressLocation::SiegeBuffAction::ADD);

	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "balance >= 0 takes the branch without SkillEngine.applyEffectDirectly";
}

TEST_F(SiegeZoneHandlerTest, TheSiegeRaceCompanionMatchesTheJavaConstructorData) {
	EXPECT_EQ(model::siege::getByRace(model::Race::ELYOS), SiegeRace::ELYOS);
	EXPECT_EQ(model::siege::getByRace(model::Race::ASMODIANS), SiegeRace::ASMODIANS);
	EXPECT_EQ(model::siege::getByRace(model::Race::DRAKAN), SiegeRace::BALAUR) << "the default branch of SiegeRace.getByRace";
	EXPECT_EQ(model::siege::getByRace(model::Race::NONE), SiegeRace::BALAUR);
	EXPECT_EQ(model::siege::getRaceId(SiegeRace::ELYOS), model::getRaceId(model::Race::ELYOS));
	EXPECT_EQ(model::siege::getRaceId(SiegeRace::ASMODIANS), model::getRaceId(model::Race::ASMODIANS));
	EXPECT_EQ(model::siege::getRaceId(SiegeRace::BALAUR), 2);
	EXPECT_EQ(model::siege::getL10nId(SiegeRace::BALAUR), 900242);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
