// P5-12a SiegeShield zone handler (m5a-plan.md R-01): the handler that every creature standing inside a fortress shield runs. The real-client
// session of 2026-09-21 lost 13 npc spawns to its unported body (7 in Verteron, 6 in Reshanta): geo zones only fire with geodata enabled, which
// the scenario gate turns off, so nothing but a real start had reached it.
//
// Expectations are derived by hand from SiegeShield.java:26-62. The zone argument is only forwarded, so the tests drive the handler the way
// ZoneInstance.onEnter and ZoneInstance.onLeave do.

#include <gtest/gtest.h>

#include <memory>
#include <string_view>

#include "../ai/AiTestSupport.h"
#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode.h"
#include "aion/gameserver/geoEngine/scene/Node.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/siege/SiegeShield.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/model/templates/zone/WorldZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::playerevents::test {
namespace {

using geoEngine::scene::DespawnableNode;
using geoEngine::scene::Node;
using model::siege::SiegeShield;

/** What the geo loader hands SiegeShield: shield geometry (material id 11) below the DespawnableNode of its map. */
struct ShieldGeometry {
	runtime::Ref<DespawnableNode> parent = DespawnableNode::create();
	runtime::Ref<Node> geometry = Node::create(std::string_view("shield"));

	ShieldGeometry() { parent->attachChild(geometry); }
};

/** A zone of the Reshanta map; SiegeShield forwards the argument without reading it. */
struct ShieldZone {
	std::unique_ptr<model::templates::zone::WorldZoneTemplate> template_ =
		std::make_unique<model::templates::zone::WorldZoneTemplate>(3000, 400010000, 0);
	runtime::Ref<model::geometry::PolyArea> area = model::geometry::PolyArea::create(template_->getName(), 400010000,
		template_->getPoints()->getPoint(), template_->getPoints()->getBottom(), template_->getPoints()->getTop());
	runtime::Ref<model::templates::zone::ZoneInfo> info = model::templates::zone::ZoneInfo::create(*area, template_.get());
	runtime::Ref<world::zone::ZoneInstance> zone = world::zone::ZoneInstance::create(400010000, *info);
};

class SiegeShieldNpcTest : public ai::testing::AiTest {};

TEST_F(SiegeShieldNpcTest, AnNpcInsideAShieldIsIgnoredAndTheShieldMarksItsNode) {
	AI_TEST_SCOPE; // AiTest opens its scope in SetUp only (AiTestSupport.h); every pointer load of the body needs its own (C2)
	AtomicConfigScope<int32_t> regionSize(configs::main::WorldConfig::WORLD_REGION_SIZE, 128);
	ShieldGeometry geometry;
	ShieldZone zone;
	runtime::Ref<SiegeShield> shield = SiegeShield::create(*geometry.geometry);
	EXPECT_EQ(geometry.parent->type.get(), DespawnableNode::DespawnableType::SHIELD) << "the constructor types the parent (SiegeShield.java:29)";

	shield->setSiegeLocationId(1011);
	EXPECT_EQ(geometry.parent->id.get(), 1011) << "setSiegeLocationId also ids the parent (SiegeShield.java:58-60)";

	runtime::Ref<ai::testing::AiTestNpc> npc = createNpc(plainTemplate);
	runtime::resetUnportedHitsForTests();

	// Java enters the body only for a Player, so a spawning npc must pass through both handlers untouched
	EXPECT_NO_THROW(shield->onEnterZone(*npc, *zone.zone));
	EXPECT_NO_THROW(shield->onLeaveZone(*npc, *zone.zone));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

class SiegeShieldPlayerTest : public PlayerEventsTest {};

TEST_F(SiegeShieldPlayerTest, APlayerOfAShieldWithoutItsFortressFailsLikeJava) {
	AtomicConfigScope<int32_t> regionSize(configs::main::WorldConfig::WORLD_REGION_SIZE, 128);
	AtomicConfigScope<bool> siegesDisabled(configs::main::SiegeConfig::SIEGE_ENABLED, false);
	ShieldGeometry geometry;
	ShieldZone zone;
	runtime::Ref<SiegeShield> shield = SiegeShield::create(*geometry.geometry);
	shield->setSiegeLocationId(1011);
	PlayerFixture f = makePlayer(1, 100);
	runtime::resetUnportedHitsForTests();

	// SiegeService.getFortress is null while sieges are disabled and Java dereferences it anyway (SiegeShield.java:40-41): the port throws the
	// NullPointerException of that read instead of inventing a guard. M5a players never reach a shielded fortress.
	EXPECT_THROW(shield->onEnterZone(*f.player, *zone.zone), runtime::NullPointerException);

	// onLeaveZone has no Player branch in Java: an unregistered creature simply finds nothing to remove
	EXPECT_NO_THROW(shield->onLeaveZone(*f.player, *zone.zone));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
