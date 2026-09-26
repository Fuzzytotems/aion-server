// P5-10 team services, M5a subset (m5a-plan.md E1-05): FindGroupService.onLogout, the AutoGroupService singleton and its login/logout/instance
// hooks for a player without registrations, and the group and alliance services for a player without a team.
//
// Expectations are derived by hand from FindGroupService.java:196-200, AutoGroupService.java:180-249, PlayerGroupService.java:77-96,212-219 and
// PlayerAllianceService.java:99-118,214-221.

#include <gtest/gtest.h>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::playerevents::test {
namespace {

class TeamServicesM5aTest : public PlayerEventsTest {};

TEST_F(TeamServicesM5aTest, LogoutOfAPlayerWithoutFindGroupEntriesOrTeam) {
	PlayerFixture f = makePlayer(1, 100);
	runtime::resetUnportedHitsForTests();
	services::findgroup::FindGroupService::getInstance().onLogout(*f.player);
	model::team::group::PlayerGroupService::onPlayerLogout(*f.player);
	model::team::alliance::PlayerAllianceService::onPlayerLogout(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(TeamServicesM5aTest, LoginSearchesNoExpiredTeam) {
	PlayerFixture f = makePlayer(2, 200);
	runtime::resetUnportedHitsForTests();
	model::team::group::PlayerGroupService::onPlayerLogin(*f.player);
	model::team::alliance::PlayerAllianceService::onPlayerLogin(*f.player);
	EXPECT_FALSE(model::team::group::PlayerGroupService::searchGroup(2));
	EXPECT_FALSE(model::team::alliance::PlayerAllianceService::searchAlliance(2));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(TeamServicesM5aTest, AutoGroupServiceIsASingletonWithoutRegistrations) {
	services::AutoGroupService& service = services::AutoGroupService::getInstance();
	EXPECT_EQ(&service, &services::AutoGroupService::getInstance());
	PlayerFixture f = makePlayer(3, 300);
	// a player in Poeta (a position without a map region is not in an instance): onEnterInstance only reads isInInstance (AutoGroupService.java:181)
	f.player->setPosition(runtime::Ptr<world::WorldPosition>(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, 0)));
	runtime::resetUnportedHitsForTests();
	service.onEnterInstance(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	// the registration lookup runs over empty queues; a player of no queue is in no search entry (AutoGroupService.java:204)
	EXPECT_FALSE(service.isSearching(*f.player, 1));
}

TEST_F(TeamServicesM5aTest, AutoGroupLoginChecksTheOpenPeriodicRegistrations) {
	PlayerFixture f = makePlayer(4, 400);
	// Java: PeriodicInstanceManager.checkAndSendOpenRegistrations(player) (P5-13); runs once that body is ported
	SKIP_IF_UNPORTED(services::AutoGroupService::getInstance().onPlayerLogin(*f.player));
}

} // namespace
} // namespace aion::gameserver::playerevents::test
