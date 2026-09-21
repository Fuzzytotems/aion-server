// Instance services at M5a (P5-13, m5a-plan.md W-03): InstanceService lookups over the World of the world chunk's test maps, InstanceScaler's
// scaling rules, the startup singletons (PeriodicInstanceManager with auto groups disabled, PlayerTransferService, CustomInstanceService) and
// PvpMapService before its map exists (init is an AION_PARTIAL). Expectations are hand-derived from InstanceService.java, InstanceScaler.java,
// PeriodicInstanceManager.java, PlayerTransferService.java, CustomInstanceService.java and PvpMapService.java.

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/instance/InstanceScaler.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/instance/PeriodicInstanceManager.h"
#include "aion/gameserver/services/transfers/PlayerTransferService.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"

#include "../world/WorldTestSupport.h"

namespace aion::gameserver::services::instance {
namespace {

using world::test::DREDGION;
using world::test::ISHALGEN;
using world::test::POETA;

/** Captures the messages of one logger ("level|message" per line) while it exists */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::string text() const { return stream.str(); }

private:
	std::string name;
	std::ostringstream stream;
};

class InstanceServicesTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!world::test::publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
	}

	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
};

TEST_F(InstanceServicesTest, InstanceExistsAndRegisteredInstances) {
	world::World& world = world::World::getInstance();
	EXPECT_TRUE(InstanceService::instanceExists(DREDGION, 1));
	EXPECT_FALSE(InstanceService::instanceExists(DREDGION, 50)) << "instance maps accept any id";
	EXPECT_TRUE(InstanceService::instanceExists(ISHALGEN, 3));
	EXPECT_THROW(static_cast<void>(InstanceService::instanceExists(POETA, 2)), runtime::IllegalArgumentException)
		<< "Java: WorldMap.getWorldMapInstance throws for an id above the twin count of an open world map";

	EXPECT_FALSE(InstanceService::getRegisteredInstance(ISHALGEN, 700001));
	runtime::Ptr<world::WorldMapInstance> second = world.getWorldMap(ISHALGEN)->getWorldMapInstance(2);
	second->register_(700001);
	EXPECT_EQ(InstanceService::getRegisteredInstance(ISHALGEN, 700001).get(), second.get());
	EXPECT_FALSE(InstanceService::getRegisteredInstance(POETA, 700001));
}

TEST_F(InstanceServicesTest, DestroyDelayDependsOnSoloInstances) {
	configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(300);
	configs::main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.store(900);
	world::World& world = world::World::getInstance();
	runtime::Ref<world::WorldMapInstance> solo = world::WorldMapInstanceFactory::createWorldMapInstance(*world.getWorldMap(DREDGION), 1);
	runtime::Ref<world::WorldMapInstance> group = world::WorldMapInstanceFactory::createWorldMapInstance(*world.getWorldMap(DREDGION), 6);
	EXPECT_EQ(InstanceService::getDestroyDelaySeconds(*solo), 300);
	EXPECT_EQ(InstanceService::getDestroyDelaySeconds(*group), 900);
}

TEST_F(InstanceServicesTest, ScalingNeedsTheSettingAGroupInstanceAndAnInstanceMap) {
	world::World& world = world::World::getInstance();
	runtime::Ref<world::WorldMapInstance> group = world::WorldMapInstanceFactory::createWorldMapInstance(*world.getWorldMap(DREDGION), 6);
	runtime::Ref<world::WorldMapInstance> solo = world::WorldMapInstanceFactory::createWorldMapInstance(*world.getWorldMap(DREDGION), 1);
	configs::main::InstanceConfig::INSTANCE_SCALING_ENABLE.store(false);
	EXPECT_FALSE(InstanceScaler::canScale(*group));
	configs::main::InstanceConfig::INSTANCE_SCALING_ENABLE.store(true);
	configs::main::InstanceConfig::INSTANCE_SCALING_EXCLUDED_MAPS.set({});
	EXPECT_TRUE(InstanceScaler::canScale(*group));
	EXPECT_FALSE(InstanceScaler::canScale(*solo)) << "max players 1";
	EXPECT_FALSE(InstanceScaler::canScale(*world.getWorldMap(POETA)->getMainWorldMapInstance())) << "not an instance map";
	configs::main::InstanceConfig::INSTANCE_SCALING_EXCLUDED_MAPS.set({DREDGION});
	EXPECT_FALSE(InstanceScaler::canScale(*group)) << "excluded map";
	configs::main::InstanceConfig::INSTANCE_SCALING_EXCLUDED_MAPS.set({});
	configs::main::InstanceConfig::INSTANCE_SCALING_ENABLE.store(false);

	// multi = min(players, max) / max; 1 - (1 - multi) * scaleFactor; at least floor
	EXPECT_FLOAT_EQ(InstanceScaler::calculateMultiplier(*group, 0.5f, 1.0f, 3), 0.5f);
	EXPECT_FLOAT_EQ(InstanceScaler::calculateMultiplier(*group, 0.3f, 0.5f, 1), 1.0f - (1.0f - 1.0f / 6.0f) * 0.5f);
	EXPECT_FLOAT_EQ(InstanceScaler::calculateMultiplier(*group, 0.9f, 1.0f, 1), 0.9f) << "the floor";
	EXPECT_FLOAT_EQ(InstanceScaler::calculateMultiplier(*group, 0.5f, 1.0f, 12), 1.0f) << "capped at the max players";
}

TEST(InstanceStartupSingletonsTest, PeriodicInstanceManagerSchedulesNothingWithoutAutoGroups) {
	configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.store(false);
	EXPECT_NO_THROW(static_cast<void>(PeriodicInstanceManager::getInstance()));
}

TEST(InstanceStartupSingletonsTest, PlayerTransferServiceLogsItsRestrictedSkills) {
	configs::main::PlayerTransferConfig::REMOVE_SKILL_LIST.set("1,2,3");
	LogCapture capture("com.aionemu.gameserver.services.transfers.PlayerTransferService");
	EXPECT_NO_THROW(static_cast<void>(transfers::PlayerTransferService::getInstance()));
	EXPECT_NE(capture.text().find("info|PlayerTransferService loaded. With 3 restricted skills."), std::string::npos) << capture.text();
	configs::main::PlayerTransferConfig::REMOVE_SKILL_LIST.set("*");
}

TEST(InstanceStartupSingletonsTest, CustomInstanceServiceTakesItsWindowIdOnce) {
	utils::idfactory::IDFactory::getInstance().resetForTests();
	int32_t before = utils::idfactory::IDFactory::getInstance().nextId();
	static_cast<void>(custom::instance::CustomInstanceService::getInstance());
	static_cast<void>(custom::instance::CustomInstanceService::getInstance());
	int32_t after = utils::idfactory::IDFactory::getInstance().nextId();
	EXPECT_EQ(after, before + 2) << "exactly one id between the two allocations (Java: LEADERBOARD_WINDOW_OBJECT_ID)";
	utils::idfactory::IDFactory::getInstance().resetForTests();
}

TEST(PvpMapServiceTest, WithoutTheMapEveryQueryIsEmptyAndInitIsAPartialSite) {
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
	custom::pvpmap::PvpMapService& service = custom::pvpmap::PvpMapService::getInstance();
	EXPECT_EQ(service.getParticipantsSize(), 0);
	uint64_t before = runtime::partialHitCount();
	EXPECT_NO_THROW(service.init());
	EXPECT_EQ(runtime::partialHitCount(), before + 1);
	EXPECT_EQ(service.getParticipantsSize(), 0) << "no handler after the partial init";
	EXPECT_NO_THROW(service.onInstanceDestroy());
}

} // namespace
} // namespace aion::gameserver::services::instance
