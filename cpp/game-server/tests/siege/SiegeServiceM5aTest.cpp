// P5-12a siege, M5a subset (m5a-plan.md E1-06): the SiegeService singleton with gameserver.siege.enable=false (plan D1), its lookups over the
// empty location maps, initSieges and onPlayerLogin guards, and Influence over no influence relevant locations.
//
// Expectations are derived by hand from SiegeService.java:73-173,339-430,561-588 and Influence.java.

#include <gtest/gtest.h>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::siege::SiegeRace;

class SiegeServiceM5aTest : public PlayerEventsTest {};

TEST_F(SiegeServiceM5aTest, DisabledSiegesHaveNoLocationsAndNoSchedules) {
	AtomicConfigScope<bool> disabled(configs::main::SiegeConfig::SIEGE_ENABLED, false);
	runtime::resetUnportedHitsForTests();
	services::SiegeService& service = services::SiegeService::getInstance(); // logs "Sieges are disabled in config."
	service.initSieges();                                                    // returns before any spawn or cron job
	service.initSieges();                                                    // isInitialized: a second call returns as well

	EXPECT_EQ(service.getFortresses().size(), 0);
	EXPECT_EQ(service.getOutposts().size(), 0);
	EXPECT_EQ(service.getArtifacts().size(), 0);
	EXPECT_EQ(service.getSiegeLocations().size(), 0);
	EXPECT_FALSE(service.getAgentLocation()) << "Java leaves agent null while sieges are disabled";
	EXPECT_FALSE(service.getFortress(1011));
	EXPECT_FALSE(service.getOutpost(3111));
	EXPECT_FALSE(service.getArtifact(1));
	EXPECT_FALSE(service.getFortressArtifact(1));
	EXPECT_FALSE(service.getSiegeLocation(1011));
	EXPECT_TRUE(service.getSiegeLocations(400010000).empty());
	EXPECT_TRUE(service.getStandaloneArtifacts().empty());
	EXPECT_FALSE(service.findFortress(400010000, 1.0f, 2.0f, 3.0f));
	EXPECT_FALSE(service.getSiege(1011));
	EXPECT_FALSE(service.isSiegeInProgress(1011));
	EXPECT_EQ(service.getRemainingSiegeTimeInSeconds(1011), 0) << "no active siege";
	EXPECT_EQ(service.getSecondsUntilNextFortressState(), 0) << "nextStateUpdateTime is null while the siege service is deactivated";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeServiceM5aTest, LoginSendsNoSiegePacketsWhileDisabled) {
	AtomicConfigScope<bool> disabled(configs::main::SiegeConfig::SIEGE_ENABLED, false);
	PlayerFixture f = makePlayer(1, 100);
	runtime::resetUnportedHitsForTests();
	services::SiegeService::getInstance().onPlayerLogin(*f.player);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeServiceM5aTest, InfluenceWithoutFortressesIsZeroEverywhere) {
	AtomicConfigScope<bool> disabled(configs::main::SiegeConfig::SIEGE_ENABLED, false);
	model::siege::Influence& influence = model::siege::Influence::getInstance();
	EXPECT_EQ(influence.getElyosInfluenceRate(), 0.0f);
	EXPECT_EQ(influence.getAsmodianInfluenceRate(), 0.0f);
	EXPECT_EQ(influence.getBalaurInfluenceRate(), 0.0f);
	EXPECT_EQ(influence.getInfluence(SiegeRace::ELYOS), 0);
	EXPECT_EQ(influence.getInfluence(400010000, SiegeRace::BALAUR), 0);
	EXPECT_TRUE(influence.getInfluenceRelevantWorldIds().empty());
	// calculatePvpRaceBonusRatio(0, 0): neither threshold is reached
	EXPECT_EQ(influence.getPvpRaceBonusRatio(model::Race::ELYOS), 0);
	EXPECT_EQ(influence.getPvpRaceBonusRatio(model::Race::ASMODIANS), 0);
	EXPECT_EQ(influence.getPvpRaceBonusRatio(model::Race::NPC), 0) << "Java: default -> 0";
	influence.recalculateInfluence();
	EXPECT_TRUE(influence.getInfluenceRelevantWorldIds().empty());
}

} // namespace
} // namespace aion::gameserver::playerevents::test
