// P5-12a siege core (m5i-plan.md I-1): the counters, the location bodies, Siege's start/stop guards and the SiegeService steps that run
// without a database or spawned npcs - the preparation cron of SiegeService.initSieges, the next fortress states from the scheduled siege
// starts, the stop and the double-start guard over the active sieges, the abyss point relay, the door repair lookups and the RvR player set.
// The steps that spawn npcs, write siege_locations or start a FortressSiege are the gate's (I-6, gs.scenario.m5i).
//
// Expectations are derived by hand from SiegeService.java, Siege.java, SiegeCounter.java, SiegeRaceCounter.java, SiegeLocation.java,
// FortressLocation.java and OutpostLocation.java.

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/OutpostLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairStone.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeException.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/services/siege/SiegeStartRunnable.h"

namespace aion::gameserver::services {

/** The private steps of SiegeService (friend of the class) */
struct SiegeServiceTestAccess {
	static std::string preparationCron(SiegeService& service, std::string_view siegeTime, int32_t fortressId) {
		return service.getPreparationCronString(siegeTime, fortressId);
	}
	static std::unordered_map<int32_t, commons::database::Timestamp> collectNextSiegeStartDates(SiegeService& service) {
		return service.collectNextSiegeStartDates();
	}
	static void updateFortressNextState(SiegeService& service) { service.updateFortressNextState(); }
	static void setNextStateUpdateTime(SiegeService& service, std::optional<commons::database::Timestamp> time) {
		service.nextStateUpdateTime.set(time);
	}
	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<siege::Siege>>& activeSieges(SiegeService& service) { return service.activeSieges; }
};

} // namespace aion::gameserver::services

namespace aion::gameserver::playerevents::test {
namespace {

using model::siege::SiegeRace;
using model::templates::siegelocation::SiegeLocationTemplate;
using services::SiegeService;
using services::SiegeServiceTestAccess;
using services::siege::Siege;

/** A Siege that records its hooks */
class RecordingSiege final : public Siege {
	AION_MAKE_REF_FRIEND
public:
	explicit RecordingSiege(model::siege::SiegeLocation& location) : Siege(location) {}

	void onSiegeStart() override { starts++; }
	void onSiegeFinish() override { finishes++; }
	bool isEndless() override { return false; }
	void onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) override {
		abyssPointsAdded.emplace_back(player.getObjectId(), abyssPoints);
	}

	int32_t starts = 0;
	int32_t finishes = 0;
	std::vector<std::pair<int32_t, int32_t>> abyssPointsAdded;

protected:
	~RecordingSiege() override = default;
};

/** A Siege whose start looks for its boss, as every Java siege does */
class BossSiege final : public Siege {
	AION_MAKE_REF_FRIEND
public:
	explicit BossSiege(model::siege::SiegeLocation& location) : Siege(location) {}

	void onSiegeStart() override { initSiegeBoss(); }
	void onSiegeFinish() override {}
	bool isEndless() override { return true; }
	void onAbyssPointsAdded(model::gameobjects::player::Player&, int32_t) override {}

protected:
	~BossSiege() override = default;
};

commons::database::Timestamp utc(int y, unsigned mo, unsigned d, int h, int mi = 0) {
	return std::chrono::time_point_cast<std::chrono::milliseconds>(
		std::chrono::sys_days(std::chrono::year(y) / std::chrono::month(mo) / std::chrono::day(d)) + std::chrono::hours(h) + std::chrono::minutes(mi));
}

class SiegeCoreTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		// World (getPlayer, getLocalSiegeNpcs) needs the world maps, zones, shields and materials: none, published once for the whole executable and never reset (World
		// keeps its maps)
		static const bool worldMapsPublished = [] {
			static xml::LoadContext worldMapsContext;
			dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(worldMapsContext, "<world_maps/>"));
			dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(worldMapsContext, "<zones/>"));
			dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(worldMapsContext, "<shields/>"));
			dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(worldMapsContext, "<material_templates/>"));
			return true;
		}();
		static_cast<void>(worldMapsPublished);
		savedAutoAssault = configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.load();
		savedLogSiege = configs::main::LoggingConfig::LOG_SIEGE.load();
		configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.store(false);
		configs::main::LoggingConfig::LOG_SIEGE.store(false);
	}

	void TearDown() override {
		SiegeService& service = SiegeService::getInstance();
		for (int32_t id : addedLocations) {
			service.getSiegeLocations().remove(id);
			service.getFortresses().remove(id);
			service.getOutposts().remove(id);
			SiegeServiceTestAccess::activeSieges(service).remove(id);
		}
		SiegeServiceTestAccess::setNextStateUpdateTime(service, std::nullopt);
		service.clearRvrEventPlayers();
		services::cron::CronService::resetForTests();
		configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.store(savedAutoAssault);
		configs::main::LoggingConfig::LOG_SIEGE.store(savedLogSiege);
		PlayerEventsTest::TearDown();
	}

	const SiegeLocationTemplate& bindTemplate(const std::string& row) {
		templates.push_back(xml::bindString<SiegeLocationTemplate>(context, row));
		return *templates.back();
	}

	/** a FortressLocation of the row, in the service's fortresses and locations (removed in TearDown) */
	runtime::Ref<model::siege::FortressLocation> addFortress(const std::string& row) {
		const SiegeLocationTemplate& t = bindTemplate(row);
		runtime::Ref<model::siege::FortressLocation> fortress = model::siege::FortressLocation::create(&t);
		SiegeService::getInstance().getFortresses().put(t.getId(), fortress);
		SiegeService::getInstance().getSiegeLocations().put(t.getId(), fortress);
		addedLocations.push_back(t.getId());
		return fortress;
	}

	runtime::Ref<RecordingSiege> addActiveSiege(model::siege::SiegeLocation& location) {
		runtime::Ref<RecordingSiege> siege = runtime::makeRef<RecordingSiege>(location);
		SiegeServiceTestAccess::activeSieges(SiegeService::getInstance()).put(location.getLocationId(), siege);
		addedLocations.push_back(location.getLocationId());
		return siege;
	}

	xml::LoadContext context;
	std::vector<std::unique_ptr<SiegeLocationTemplate>> templates;
	std::vector<int32_t> addedLocations;
	bool savedAutoAssault = false;
	bool savedLogSiege = false;
};

// ---------------------------------------------------------------------------------------------------------- the preparation cron (697-715)

TEST_F(SiegeCoreTest, PreparationStartsFiveMinutesBeforeAFortressSiege) {
	SiegeService& service = SiegeService::getInstance();
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 0 20 ? * *", 1011), "0 55 19 ? * *");
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 30 22 ? * MON,THU", 1221), "0 25 22 ? * MON,THU");
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 5 18 ? * SUN", 1131), "0 0 18 ? * SUN") << "minute 0 is not negative";
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 4 1 ? * *", 1131), "0 59 0 ? * *") << "the hour before";
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 0 20 ? * *", 9999), "0 55 19 ? * *") << "fortressId < 10000";
}

TEST_F(SiegeCoreTest, PreparationStartsTenMinutesBeforeAPanesterraSiege) {
	SiegeService& service = SiegeService::getInstance();
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 0 20 ? * *", 10111), "0 50 19 ? * *");
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 15 20 ? * *", 10000), "0 5 20 ? * *") << "10000 is not < 10000";
}

TEST_F(SiegeCoreTest, PreparationOverMidnightIsNotSupported) {
	SiegeService& service = SiegeService::getInstance();
	try {
		SiegeServiceTestAccess::preparationCron(service, "0 3 0 ? * *", 1011);
		FAIL() << "expected UnsupportedOperationException";
	} catch (const commons::utils::UnsupportedOperationException& e) {
		EXPECT_EQ(std::string(e.what()), "Failed converting cron expression: 0 3 0 ? * *\nPreparation over midnight not supported.");
		EXPECT_FALSE(e.cause()) << "thrown directly, not from the NumberFormatException catch";
	}
	EXPECT_EQ(SiegeServiceTestAccess::preparationCron(service, "0 5 0 ? * *", 1011), "0 0 0 ? * *") << "midnight itself is fine";
}

TEST_F(SiegeCoreTest, PreparationOfAnUnparsableExpressionWrapsTheNumberFormatException) {
	SiegeService& service = SiegeService::getInstance();
	for (const char* siegeTime : {"0 */5 20 ? * *", "0 0 * ? * *", "0 0 200 ? * *" /* Byte.parseByte: out of range */, "0  0 20 ? * *" /* "" */}) {
		try {
			SiegeServiceTestAccess::preparationCron(service, siegeTime, 1011);
			ADD_FAILURE() << siegeTime << ": expected UnsupportedOperationException";
		} catch (const commons::utils::UnsupportedOperationException& e) {
			EXPECT_EQ(std::string(e.what()), std::string("Failed converting cron expression: ") + siegeTime);
			ASSERT_TRUE(e.cause()) << siegeTime;
			EXPECT_THROW(std::rethrow_exception(e.cause()), commons::utils::NumberFormatException) << siegeTime;
		}
	}
}

TEST_F(SiegeCoreTest, PreparationOfATooShortExpressionThrowsTheIndexError) {
	// Java catches only the NumberFormatException: cronParts[1] or [2] of a short expression throws ArrayIndexOutOfBoundsException through
	SiegeService& service = SiegeService::getInstance();
	EXPECT_THROW(SiegeServiceTestAccess::preparationCron(service, "0 5", 1011), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_THROW(SiegeServiceTestAccess::preparationCron(service, "0", 1011), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_THROW(SiegeServiceTestAccess::preparationCron(service, "0 x", 1011), commons::utils::UnsupportedOperationException)
		<< "cronParts[1] is parsed before cronParts[2] is read";
}

// ------------------------------------------------------------------------------------------- the next fortress states (309-334)

TEST_F(SiegeCoreTest, NextSiegeStartDatesKeepTheEarliestJobPerLocation) {
	clock.setCurrentTimeMillis(utc(2024, 1, 1, 10, 30).time_since_epoch().count()); // a Monday
	services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
		services::cron::CronService::Driver::EXECUTOR);
	services::cron::CronService& cron = services::cron::CronService::getInstance();
	(void)cron.schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1011}), "0 0 20 ? * *");
	(void)cron.schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1011}), "0 0 11 ? * *");
	(void)cron.schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1011}), "0 0 21 ? * *");
	(void)cron.schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1131}), "0 0 12 ? * MON");
	(void)cron.schedule(services::cron::CronJob([] {}), "0 0 * ? * *"); // not a SiegeStartRunnable

	std::unordered_map<int32_t, commons::database::Timestamp> dates = SiegeServiceTestAccess::collectNextSiegeStartDates(SiegeService::getInstance());
	ASSERT_EQ(dates.size(), 2u);
	EXPECT_EQ(dates.at(1011), utc(2024, 1, 1, 11)) << "the earliest of the three jobs";
	EXPECT_EQ(dates.at(1131), utc(2024, 1, 1, 12));
}

TEST_F(SiegeCoreTest, FortressesStartingUntilTheNextStateUpdateTurnVulnerableNext) {
	runtime::Ref<model::siege::FortressLocation> early = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<model::siege::FortressLocation> onTime = addFortress(R"(<siege_location id="1131" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<model::siege::FortressLocation> late = addFortress(R"(<siege_location id="1221" type="FORTRESS" world="400010000"/>)");
	clock.setCurrentTimeMillis(utc(2024, 1, 1, 10, 30).time_since_epoch().count());
	services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
		services::cron::CronService::Driver::EXECUTOR);
	services::cron::CronService& cron = services::cron::CronService::getInstance();
	(void)cron.schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1011}), "0 55 10 ? * *");
	(void)cron.schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1131}), "0 0 11 ? * *");
	(void)cron.schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1221}), "1 0 11 ? * *");
	late->setNextState(model::siege::SiegeLocation::STATE_VULNERABLE);

	SiegeService& service = SiegeService::getInstance();
	SiegeServiceTestAccess::setNextStateUpdateTime(service, utc(2024, 1, 1, 11));
	SiegeServiceTestAccess::updateFortressNextState(service);
	EXPECT_EQ(early->getNextState(), model::siege::SiegeLocation::STATE_VULNERABLE);
	EXPECT_EQ(onTime->getNextState(), model::siege::SiegeLocation::STATE_VULNERABLE) << "!date.after(nextStateUpdateTime): equal is vulnerable";
	EXPECT_EQ(late->getNextState(), model::siege::SiegeLocation::STATE_INVULNERABLE) << "one second after the update";
}

TEST_F(SiegeCoreTest, NextFortressStatesNeedTheNextStateUpdateTime) {
	addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	services::cron::CronService::initSingleton(std::make_unique<services::cron::CurrentThreadRunnableRunner>(), std::chrono::locate_zone("UTC"),
		services::cron::CronService::Driver::EXECUTOR);
	SiegeService& service = SiegeService::getInstance();
	SiegeServiceTestAccess::updateFortressNextState(service); // no jobs: nothing to compare
	(void)services::cron::CronService::getInstance().schedule(services::cron::CronJob(services::siege::SiegeStartRunnable{.locationId = 1011}),
		"0 0 20 ? * *");
	EXPECT_THROW(SiegeServiceTestAccess::updateFortressNextState(service), runtime::NullPointerException) << "Java: Date.after(null)";
}

// ------------------------------------------------------------------------------------------------- start, stop and capture guards

TEST_F(SiegeCoreTest, StopSiegeRemovesTheSiegeAndStopsItOnce) {
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<RecordingSiege> siege = addActiveSiege(*fortress);
	SiegeService& service = SiegeService::getInstance();
	ASSERT_TRUE(service.isSiegeInProgress(1011));

	service.stopSiege(1011);
	EXPECT_FALSE(service.isSiegeInProgress(1011));
	EXPECT_FALSE(service.getSiege(1011));
	EXPECT_EQ(siege->finishes, 1);
	EXPECT_TRUE(siege->isFinished());

	service.stopSiege(1011); // not in progress any more: a debug line
	EXPECT_EQ(siege->finishes, 1);
}

TEST_F(SiegeCoreTest, StopSiegeOfAFinishedSiegeOnlyRemovesIt) {
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<RecordingSiege> siege = addActiveSiege(*fortress);
	siege->stopSiege(); // e.g. the boss died
	ASSERT_EQ(siege->finishes, 1);

	SiegeService::getInstance().stopSiege(1011);
	EXPECT_FALSE(SiegeService::getInstance().isSiegeInProgress(1011));
	EXPECT_EQ(siege->finishes, 1) << "siege.isFinished(): no second stop";
}

TEST_F(SiegeCoreTest, ASiegeIsNotStartedTwice) {
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<RecordingSiege> siege = addActiveSiege(*fortress);
	SiegeService& service = SiegeService::getInstance();
	service.startSiege(1011); // logs "Attempt to start siege twice" and returns before newSiege
	EXPECT_EQ(service.getSiege(1011).get(), siege.get());
	EXPECT_EQ(siege->starts, 0);
	EXPECT_EQ(service.getRemainingSiegeTimeInSeconds(1011), 0) << "an active siege that has not started";
}

TEST_F(SiegeCoreTest, AnUnknownLocationHasNoSiegeHandler) {
	SiegeService& service = SiegeService::getInstance();
	try {
		service.startSiege(4242);
		FAIL() << "expected SiegeException";
	} catch (const services::siege::SiegeException& e) {
		EXPECT_EQ(std::string(e.what()), "Unknown siege handler for siege location: 4242");
	}
	EXPECT_FALSE(service.isSiegeInProgress(4242));
}

TEST_F(SiegeCoreTest, AbyssPointsOfAPlayerKillGoToEveryActiveSiege) {
	runtime::Ref<model::siege::FortressLocation> f1 = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<model::siege::FortressLocation> f2 = addFortress(R"(<siege_location id="1131" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<RecordingSiege> s1 = addActiveSiege(*f1);
	runtime::Ref<RecordingSiege> s2 = addActiveSiege(*f2);
	PlayerFixture killer = makePlayer(11, 100);
	PlayerFixture victim = makePlayer(12, 101, model::Race::ASMODIANS);

	SiegeService::getInstance().onAbyssPointsAdded(*killer.player, *victim.player, 250);
	EXPECT_EQ(s1->abyssPointsAdded, (std::vector<std::pair<int32_t, int32_t>>{{11, 250}}));
	EXPECT_EQ(s2->abyssPointsAdded, (std::vector<std::pair<int32_t, int32_t>>{{11, 250}}));
}

// ------------------------------------------------------------------------------------------------------------- lookups and RvR players

TEST_F(SiegeCoreTest, DoorRepairDataAndStonesOfAFortress) {
	addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000">)"
				R"(<door_repair_data item_id="186000236" count="2" cd="600"><door_repair_stone static_id="7" door_id="70"/>)"
				R"(<door_repair_stone static_id="9" door_id="90"/></door_repair_data></siege_location>)");
	addFortress(R"(<siege_location id="1131" type="FORTRESS" world="400010000"/>)");
	SiegeService& service = SiegeService::getInstance();

	ASSERT_NE(service.getDoorRepairData(1011), nullptr);
	ASSERT_NE(service.getRepairStone(1011, 9), nullptr);
	EXPECT_EQ(service.getRepairStone(1011, 9)->getDoorId(), 90);
	EXPECT_EQ(service.getRepairStone(1011, 7)->getDoorId(), 70);
	EXPECT_EQ(service.getRepairStone(1011, 8), nullptr);
	EXPECT_EQ(service.getDoorRepairData(1131), nullptr) << "a fortress without door_repair_data";
	EXPECT_EQ(service.getRepairStone(1131, 9), nullptr);
	EXPECT_EQ(service.getDoorRepairData(4242), nullptr) << "no fortress";
	EXPECT_EQ(service.getRepairStone(4242, 9), nullptr);
}

TEST_F(SiegeCoreTest, RvrEventPlayersAreASetUntilCleared) {
	SiegeService& service = SiegeService::getInstance();
	PlayerFixture a = makePlayer(21, 200);
	PlayerFixture b = makePlayer(22, 201);
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::gameobjects::player::Player>>> before = service.getRvrEventPlayers();
	ASSERT_TRUE(before);
	service.checkRvrEventPlayer(runtime::Ptr<model::gameobjects::player::Player>(a.player));
	service.checkRvrEventPlayer(runtime::Ptr<model::gameobjects::player::Player>(a.player));
	service.checkRvrEventPlayer(runtime::Ptr<model::gameobjects::player::Player>(b.player));
	service.checkRvrEventPlayer(nullptr);
	EXPECT_EQ(service.getRvrEventPlayers()->size(), 2);

	service.clearRvrEventPlayers();
	EXPECT_EQ(service.getRvrEventPlayers()->size(), 0);
	EXPECT_NE(service.getRvrEventPlayers().get(), before.get()) << "Java assigns a new HashSet";
	EXPECT_EQ(before->size(), 2) << "the old set is left as it was";
}

TEST_F(SiegeCoreTest, OutpostsDependOnTheirTemplatesFortresses) {
	const SiegeLocationTemplate& t = bindTemplate(R"(<siege_location id="2111" type="OUTPOST" world="210050000" fortress_dependency="2011 2021"/>)");
	runtime::Ref<model::siege::OutpostLocation> outpost = model::siege::OutpostLocation::create(&t);
	EXPECT_EQ(outpost->getFortressDependency(), (std::vector<int32_t>{2011, 2021}));
	EXPECT_TRUE(outpost->isSilenteraAllowed());
	EXPECT_EQ(outpost->getNextState(), model::siege::SiegeLocation::STATE_VULNERABLE) << "the opposite of the current state";
	outpost->setVulnerable(true);
	EXPECT_EQ(outpost->getNextState(), model::siege::SiegeLocation::STATE_INVULNERABLE);
}

// -------------------------------------------------------------------------------------------------------------------- the counters

TEST_F(SiegeCoreTest, TheRaceWithTheMostDamageWins) {
	runtime::Ref<services::siege::SiegeCounter> counter = services::siege::SiegeCounter::create();
	EXPECT_EQ(counter->getWinnerRaceCounter(SiegeRace::BALAUR)->getSiegeRace(), SiegeRace::BALAUR) << "no damage: the fallback";
	counter->addRaceDamage(SiegeRace::ELYOS, 100);
	counter->addRaceDamage(SiegeRace::ASMODIANS, 150);
	counter->addRaceDamage(SiegeRace::ELYOS, 40);
	EXPECT_EQ(counter->getRaceCounter(SiegeRace::ELYOS)->getTotalDamage(), 140);
	EXPECT_EQ(counter->getWinnerRaceCounter(SiegeRace::BALAUR)->getSiegeRace(), SiegeRace::ASMODIANS);
	counter->addRaceDamage(SiegeRace::ELYOS, 10);
	EXPECT_EQ(counter->getWinnerRaceCounter(SiegeRace::BALAUR)->getSiegeRace(), SiegeRace::ELYOS)
		<< "150 against 150: the stable sort keeps the EnumMap's ELYOS first";
	counter->addRaceDamage(SiegeRace::BALAUR, 151);
	EXPECT_EQ(counter->getWinnerRaceCounter(SiegeRace::ELYOS)->getSiegeRace(), SiegeRace::BALAUR);
}

TEST_F(SiegeCoreTest, PlayerDamageAndAbyssPointsGoToThePlayersRaceInDescendingOrder) {
	runtime::Ref<services::siege::SiegeCounter> counter = services::siege::SiegeCounter::create();
	PlayerFixture e1 = makePlayer(31, 300);
	PlayerFixture e2 = makePlayer(32, 301);
	PlayerFixture a1 = makePlayer(33, 302, model::Race::ASMODIANS);
	counter->addDamage(*e1.player, 50);
	counter->addDamage(*e2.player, 70);
	counter->addDamage(*e1.player, 30);
	counter->addDamage(*a1.player, 5);
	counter->addAbyssPoints(*e2.player, 0);
	counter->addAbyssPoints(*e1.player, 12);

	runtime::Ptr<services::siege::SiegeRaceCounter> elyos = counter->getRaceCounter(SiegeRace::ELYOS);
	EXPECT_EQ(elyos->getTotalDamage(), 150);
	EXPECT_EQ(elyos->getPlayerDamageCounter(), (std::vector<std::pair<int32_t, int64_t>>{{31, 80}, {32, 70}}));
	EXPECT_EQ(elyos->getPlayerAbyssPoints(), (std::vector<std::pair<int32_t, int64_t>>{{31, 12}})) << "a zero counter is left out";
	EXPECT_EQ(counter->getRaceCounter(SiegeRace::ASMODIANS)->getPlayerDamageCounter(), (std::vector<std::pair<int32_t, int64_t>>{{33, 5}}));
	EXPECT_TRUE(counter->getRaceCounter(SiegeRace::BALAUR)->getPlayerDamageCounter().empty());
	EXPECT_EQ(elyos->compareTo(*counter->getRaceCounter(SiegeRace::ASMODIANS)), -1) << "Long.compare(o, this): more damage sorts first";
	EXPECT_EQ(counter->getRaceCounter(SiegeRace::ASMODIANS)->compareTo(*elyos), 1);
	EXPECT_EQ(elyos->compareTo(*elyos), 0);
	EXPECT_FALSE(elyos->getWinnerLegionId()) << "no player online: no team damage";
}

// ------------------------------------------------------------------------------------------------------------------ locations, Siege

TEST_F(SiegeCoreTest, LocationBodiesReadTheirTemplate) {
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(
		R"(<siege_location id="1011" type="FORTRESS" world="400010000" siege_duration="3600" legion_gp="500">)"
		R"(<siege_reward top="1" gp_win="100"/><siege_reward top="5" gp_win="50"/></siege_location>)");
	EXPECT_EQ(fortress->getSiegeDuration(), 3600);
	EXPECT_EQ(fortress->getLegionGp(), 500);
	ASSERT_EQ(fortress->getRewards().size(), 2u);
	EXPECT_EQ(fortress->getRewards()[1]->getTop(), 5);
	EXPECT_TRUE(fortress->getLegionRewards().empty());
	EXPECT_TRUE(fortress->getSiegeMercenaryZones().empty());

	fortress->increaseOccupiedCount();
	fortress->increaseOccupiedCount();
	EXPECT_EQ(fortress->getOccupiedCount(), 2);

	for (int i = 0; i < 12; i++)
		fortress->adjustFactionBalance(7);
	EXPECT_EQ(fortress->getFactionBalance(), 9) << "capped at 9, signum(7) = 1 a step";
	fortress->adjustFactionBalance(-3);
	EXPECT_EQ(fortress->getFactionBalance(), 8);
	fortress->adjustFactionBalance(0);
	EXPECT_EQ(fortress->getFactionBalance(), 8);
	fortress->setFactionBalance(-9);
	fortress->adjustFactionBalance(-1);
	EXPECT_EQ(fortress->getFactionBalance(), -9);
}

TEST_F(SiegeCoreTest, OnlyTheOwningRaceTeleportsToALocation) {
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	PlayerFixture elyos = makePlayer(41, 400);
	PlayerFixture asmo = makePlayer(42, 401, model::Race::ASMODIANS);
	fortress->setRace(SiegeRace::ELYOS);
	fortress->setCanTeleport(true);
	EXPECT_TRUE(fortress->isCanTeleport(runtime::Ptr<model::gameobjects::player::Player>(elyos.player)));
	EXPECT_FALSE(fortress->isCanTeleport(runtime::Ptr<model::gameobjects::player::Player>(asmo.player)));
	EXPECT_TRUE(fortress->isCanTeleport(nullptr)) << "no player: the flag alone";
	fortress->setCanTeleport(false);
	EXPECT_FALSE(fortress->isCanTeleport(runtime::Ptr<model::gameobjects::player::Player>(elyos.player)));
	EXPECT_FALSE(fortress->isCanTeleport(nullptr));
}

TEST_F(SiegeCoreTest, ASiegeStartsAndStopsOnce) {
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<RecordingSiege> siege = runtime::makeRef<RecordingSiege>(*fortress);
	EXPECT_EQ(siege->getSiegeLocationId(), 1011);
	EXPECT_EQ(siege->toString(), "RecordingSiege [locationId=1011]");
	EXPECT_FALSE(siege->isStarted());

	const int64_t before = commons::utils::currentTimeMillis();
	siege->startSiege();
	siege->startSiege(); // logs "Attempt to start ... twice"
	const int64_t after = commons::utils::currentTimeMillis();
	EXPECT_EQ(siege->starts, 1);
	EXPECT_TRUE(siege->isStarted());
	EXPECT_GE(siege->getStartTime(), before) << "System.currentTimeMillis() of the first start";
	EXPECT_LE(siege->getStartTime(), after);

	siege->stopSiege();
	siege->stopSiege(); // logs "Attempt to stop ... twice"
	EXPECT_EQ(siege->finishes, 1);
	EXPECT_TRUE(siege->isFinished());
}

TEST_F(SiegeCoreTest, ASiegeWithoutABossCannotStart) {
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	runtime::Ref<BossSiege> siege = runtime::makeRef<BossSiege>(*fortress);
	try {
		siege->startSiege();
		FAIL() << "expected SiegeException";
	} catch (const services::siege::SiegeException& e) {
		EXPECT_EQ(std::string(e.what()), "Siege Boss not found for siege 1011");
	}
}

} // namespace
} // namespace aion::gameserver::playerevents::test
