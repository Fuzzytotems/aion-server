// M5i I-10 (P5-12b1): PanesterraService's team bookkeeping with no team (PanesterraService.java:82-111, 173-181, 222-227, 280-295): an
// elimination by //base (no team), the member count and the player's team, a fortress siege of a fortress without related bases, and
// Java's NullPointerException when a Panesterra siege ends without its teams. Creating the teams starts with the corridor spawns
// (prepareFortressSiege), which are the gate's.

#include <gtest/gtest.h>

#include <memory>

#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"

namespace aion::gameserver::playerevents::test {
namespace {

using services::panesterra::PanesterraService;
using services::panesterra::ahserion::PanesterraFaction;

class PanesterraServiceTest : public PlayerEventsTest {
protected:
	runtime::Ref<model::siege::FortressLocation> fortress(int32_t id) {
		templates.push_back(xml::bindString<model::templates::siegelocation::SiegeLocationTemplate>(context,
			"<siege_location id=\"" + std::to_string(id) + "\" type=\"FORTRESS\" world=\"400030000\"/>"));
		return model::siege::FortressLocation::create(templates.back().get());
	}

	xml::LoadContext context;
	std::vector<std::unique_ptr<model::templates::siegelocation::SiegeLocationTemplate>> templates;
};

TEST_F(PanesterraServiceTest, WithoutTeamsTheQueriesAnswerNothing) {
	runtime::resetUnportedHitsForTests();
	PanesterraService& service = PanesterraService::getInstance();
	EXPECT_FALSE(service.handleTeamElimination(PanesterraFaction::IVY_TEMPLE)) << "//base: no team, null";
	EXPECT_EQ(service.getTeamMemberCount(PanesterraFaction::BELUS), 0);
	EXPECT_FALSE(service.getTeam(PanesterraFaction::BELUS));
	PlayerFixture f = makePlayer(61, 6100);
	EXPECT_FALSE(service.getTeam(*f.player));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(PanesterraServiceTest, AFortressWithoutRelatedBasesChangesNoBase) {
	runtime::resetUnportedHitsForTests();
	runtime::Ref<model::siege::FortressLocation> other = fortress(1011);
	PanesterraService::getInstance().startFortressSiege(*other);
	PanesterraService::getInstance().stopFortressSiege(*other); // not a Panesterra fortress: no team to remove, no related base
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(PanesterraServiceTest, APanesterraSiegeEndingWithoutItsTeamsIsJavasNullPointerException) {
	// PanesterraService.java:222-227: removeTeams calls moveTeamMembersToOriginPosition on activeFactionTeams.remove(faction), null here
	runtime::Ref<model::siege::FortressLocation> belus = fortress(10111);
	EXPECT_THROW(PanesterraService::getInstance().stopFortressSiege(*belus), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
