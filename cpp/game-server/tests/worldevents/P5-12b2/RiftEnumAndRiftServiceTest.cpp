// P5-12b2, M5i I-5 (m5i-plan.md 15.3, R-01): the RiftEnum companion (services/rift/RiftEnumInfo.h) that replaces P4-11b's riftEnumData stand-in,
// RVController's constant data read through it, RiftOpenRunnable as a cron job found by type, and RiftService's guards over unknown ids.
//
// Expectations are derived by hand from RiftEnum.java:10-126, RVController.java (the constructors), RiftOpenRunnable.java and
// RiftService.java:52-116. Opening rifts (spawns, RiftManager's master and slave npcs, the rift broadcast) is the gate's (I-11, a rift).

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/model/templates/rift/RiftTemplate.bind.h"
#include "aion/gameserver/model/templates/rift/RiftTemplate.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/rift/RiftEnumInfo.h"
#include "aion/gameserver/services/rift/RiftOpenRunnable.h"

namespace aion::gameserver::playerevents::test {
namespace {

using services::rift::RiftEnum;

TEST(RiftEnumTest, RiftsAreFoundByIdAndVortexesByDestination) {
	EXPECT_EQ(services::rift::getRift(2120), RiftEnum::ELTNEN_AM);
	EXPECT_EQ(services::rift::getRift(2291), RiftEnum::ENSHAR_VIL3M) << "the last constant";
	EXPECT_EQ(services::rift::getRift(1170), RiftEnum::KAISINEL_AM) << "the first constant";
	try {
		services::rift::getRift(1);
		FAIL() << "expected IllegalArgumentException";
	} catch (const commons::utils::IllegalArgumentException& e) {
		EXPECT_EQ(std::string(e.what()), "Unsupported rift id: 1");
	}
	EXPECT_EQ(services::rift::getVortex(model::Race::ASMODIANS), RiftEnum::KAISINEL_AM);
	EXPECT_EQ(services::rift::getVortex(model::Race::ELYOS), RiftEnum::MARCHUTAN_AM);
	EXPECT_THROW(services::rift::getVortex(model::Race::NPC), commons::utils::IllegalArgumentException);
}

TEST(RiftEnumTest, TheConstantsCarryTheirConstructorData) {
	EXPECT_EQ(services::rift::getMaster(RiftEnum::ELTNEN_BM), "ELTNEN_BM");
	EXPECT_EQ(services::rift::getSlave(RiftEnum::ELTNEN_BM), "MORHEIM_BS");
	EXPECT_EQ(services::rift::getEntries(RiftEnum::INGGISON_AM), 150);
	EXPECT_EQ(services::rift::getMinLevel(RiftEnum::CYGNEA_GM), 60);
	EXPECT_EQ(services::rift::getMaxLevel(RiftEnum::CYGNEA_GM), 65);
	EXPECT_EQ(services::rift::getDestination(RiftEnum::MORHEIM_AM), model::Race::ELYOS);
	EXPECT_TRUE(services::rift::isVortex(RiftEnum::KAISINEL_AM));
	EXPECT_FALSE(services::rift::isVortex(RiftEnum::ELTNEN_AM)) << "the 7-argument constructor: no vortex";
	EXPECT_TRUE(services::rift::canBeVolatile(RiftEnum::CYGNEA_GM));
	EXPECT_FALSE(services::rift::canBeVolatile(RiftEnum::CYGNEA_FM));
	EXPECT_TRUE(services::rift::isInvasionRift(RiftEnum::ENSHAR_VIL1M));
	EXPECT_FALSE(services::rift::isInvasionRift(RiftEnum::CYGNEA_GM));
	EXPECT_EQ(services::rift::getId(RiftEnum::BELUSLAN_CM), 2242);
}

class RiftServiceTest : public PlayerEventsTest {
protected:
	void TearDown() override {
		services::cron::CronService::resetForTests();
		PlayerEventsTest::TearDown();
	}
};

TEST_F(RiftServiceTest, ARiftControllerReadsItsConstantsData) {
	// a slave controller (no slave of its own): Java RVController(null, rift)
	// (a vortex controller asks VortexService.getDuration for its despawn time: I-10)
	controllers::RVController slave(nullptr, RiftEnum::ELTNEN_AM);
	EXPECT_FALSE(slave.isMaster());
	EXPECT_FALSE(slave.isVortex());
	EXPECT_FALSE(slave.isVolatile());
	EXPECT_FALSE(slave.isInvasion());
	controllers::RVController invasion(nullptr, RiftEnum::ENSHAR_VIL1M, true);
	EXPECT_TRUE(invasion.isInvasion());
	EXPECT_FALSE(invasion.isVolatile()) << "volatile needs a slave (a master controller)";
}

TEST_F(RiftServiceTest, RiftOpenJobsAreFoundByType) {
	services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
		services::cron::CronService::Driver::EXECUTOR);
	(void)services::cron::CronService::getInstance().schedule(
		services::cron::CronJob(services::rift::RiftOpenRunnable{.worldId = 210020000, .guards = true}), "0 0 12 ? * *");
	auto jobs = services::cron::CronService::getInstance().findJobs<services::rift::RiftOpenRunnable>();
	ASSERT_EQ(jobs.size(), 1u);
	const services::rift::RiftOpenRunnable* job = jobs.front()->getJob().target<services::rift::RiftOpenRunnable>();
	ASSERT_NE(job, nullptr);
	EXPECT_EQ(job->worldId, 210020000);
	EXPECT_TRUE(job->guards);
}

TEST_F(RiftServiceTest, UnknownRiftsAreNeitherOpenedNorClosed) {
	AtomicConfigScope<bool> rifts(configs::main::CustomConfig::RIFT_ENABLED, false);
	services::RiftService& service = services::RiftService::getInstance();
	service.initRiftLocations(); // disabled: no locations
	EXPECT_FALSE(service.openRifts(2120, true)) << "no such rift location";
	EXPECT_FALSE(service.openRifts(210020000, false)) << "no rift location on the map";
	EXPECT_FALSE(service.closeRifts(2120));
	EXPECT_FALSE(service.closeRifts(210020000));
	service.prepareRiftOpening(210070000, true); // no location of the map: nothing to open
	service.closeAutoCloseableRifts(210070000);
	EXPECT_FALSE(service.isRiftOpened(2120));
}


TEST_F(RiftServiceTest, ARiftLocationReadsItsTemplateAndReplacesOnlyItsSpawns) {
	// M5i I-11: RiftLocation.java:23-61 (reached by //rift open and every respawn in a rift world, RiftService.updateSpawned)
	xml::LoadContext context;
	std::unique_ptr<model::templates::rift::RiftTemplate> t = xml::bindString<model::templates::rift::RiftTemplate>(
		context, R"(<rift_location id="2120" world="210020000" has_spawns="true" auto_closeable="true"/>)");
	runtime::Ref<model::rift::RiftLocation> location = model::rift::RiftLocation::create(t.get());
	EXPECT_EQ(location->getId(), 2120);
	EXPECT_EQ(location->getWorldId(), 210020000);
	EXPECT_TRUE(location->hasSpawns());
	EXPECT_TRUE(location->isAutoCloseable());
	// the spawned map holds spawn templates (the gate's rift, C11, spawns real npcs): an object id that was never spawned is not replaced
	PlayerFixture first = makePlayer(71, 7100);
	EXPECT_FALSE(location->replaceSpawned(first.player->getObjectId(), *first.player));
	EXPECT_TRUE(location->getSpawned().isEmpty());
}

} // namespace
} // namespace aion::gameserver::playerevents::test
