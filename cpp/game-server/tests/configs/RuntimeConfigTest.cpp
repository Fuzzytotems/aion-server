#include "aion/gameserver/configs/main/RuntimeConfig.h"

#include <chrono>

#include <gtest/gtest.h>

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/gameserver/configs/main/ThreadConfig.h"

using namespace aion::gameserver;
using aion::commons::configuration::ConfigurableProcessor;
using aion::commons::configuration::Properties;
using configs::main::RuntimeConfig;
using configs::main::ThreadConfig;

TEST(RuntimeConfigTest, DefaultsEqualKernelDefaults) {
	Properties empty;
	ConfigurableProcessor::process(empty, {&RuntimeConfig::bind, &ThreadConfig::bind});

	runtime::Reclaimer::Config reclaimerDefaults;
	EXPECT_EQ(RuntimeConfig::reclaimerConfig().period, reclaimerDefaults.period);
	EXPECT_EQ(RuntimeConfig::reclaimerConfig().backlogDumpObjects, reclaimerDefaults.backlogDumpObjects);

	runtime::LeakCensus::Config censusDefaults;
	EXPECT_EQ(RuntimeConfig::leakCensusConfig().censusAfter, censusDefaults.censusAfter);
	EXPECT_EQ(RuntimeConfig::leakCensusConfig().zombieBreakAfter, censusDefaults.zombieBreakAfter);

	utils::idfactory::IDFactory::Config idDefaults;
	EXPECT_EQ(RuntimeConfig::idFactoryConfig().wrapAt, idDefaults.wrapAt);
	EXPECT_EQ(RuntimeConfig::idFactoryConfig().releaseDelay, idDefaults.releaseDelay);

	runtime::Watchdog::Config watchdogDefaults;
	EXPECT_EQ(RuntimeConfig::watchdogConfig().slowTaskWarning, watchdogDefaults.slowTaskWarning);
	EXPECT_EQ(RuntimeConfig::watchdogConfig().stall, watchdogDefaults.stall);
	EXPECT_EQ(RuntimeConfig::watchdogConfig().restartOnDeadlock, watchdogDefaults.restartOnDeadlock);

	utils::ThreadPoolManager::Config poolDefaults;
	utils::ThreadPoolManager::Config pool = RuntimeConfig::threadPoolManagerConfig();
	EXPECT_EQ(pool.baseThreadPoolSize, poolDefaults.baseThreadPoolSize);
	EXPECT_EQ(pool.scheduledThreadPoolSize, poolDefaults.scheduledThreadPoolSize);
	EXPECT_EQ(pool.usePriorities, poolDefaults.usePriorities);
	EXPECT_EQ(pool.maximumRuntimeInMillisecWithoutWarning, poolDefaults.maximumRuntimeInMillisecWithoutWarning);
	EXPECT_EQ(pool.coalesceAfterPeriods, poolDefaults.coalesceAfterPeriods);
	EXPECT_EQ(pool.singleExecutor, poolDefaults.singleExecutor);
	EXPECT_EQ(pool.serialMovement, poolDefaults.serialMovement);
}

TEST(RuntimeConfigTest, PropertiesReachKernelConfigs) {
	Properties properties;
	properties.setProperty("gameserver.runtime.reclaim_period_ms", "5");
	properties.setProperty("gameserver.runtime.backlog_dump_objects", "5000000000");
	properties.setProperty("gameserver.runtime.zombie_break_minutes", "45");
	properties.setProperty("gameserver.debug.leak_census_minutes", "3");
	properties.setProperty("gameserver.idfactory.wrap_at", "0x100000");
	properties.setProperty("gameserver.idfactory.release_delay", "60");
	properties.setProperty("gameserver.watchdog.stall_seconds", "120");
	properties.setProperty("gameserver.watchdog.restart_on_deadlock", "true");
	properties.setProperty("gameserver.scheduler.coalesce_after", "0");
	properties.setProperty("gameserver.debug.single_executor", "true");
	properties.setProperty("gameserver.debug.serial_movement", "true");
	properties.setProperty("gameserver.thread.base_pool_size", "3");
	properties.setProperty("gameserver.thread.scheduled_pool_size", "2");
	properties.setProperty("gameserver.thread.runtime", "7000");
	properties.setProperty("gameserver.thread.usepriority", "true");
	EXPECT_TRUE(ConfigurableProcessor::process(properties, {&RuntimeConfig::bind, &ThreadConfig::bind}).empty());

	EXPECT_EQ(RuntimeConfig::reclaimerConfig().period, std::chrono::milliseconds(5));
	EXPECT_EQ(RuntimeConfig::reclaimerConfig().backlogDumpObjects, 5'000'000'000u);
	EXPECT_EQ(RuntimeConfig::leakCensusConfig().zombieBreakAfter, std::chrono::minutes(45));
	EXPECT_EQ(RuntimeConfig::leakCensusConfig().censusAfter, std::chrono::minutes(3));
	EXPECT_EQ(RuntimeConfig::idFactoryConfig().wrapAt, 0x100000);
	EXPECT_EQ(RuntimeConfig::idFactoryConfig().releaseDelay, std::chrono::seconds(60));
	EXPECT_EQ(RuntimeConfig::watchdogConfig().stall, std::chrono::seconds(120));
	EXPECT_EQ(RuntimeConfig::watchdogConfig().slowTaskWarning, std::chrono::milliseconds(7000));
	EXPECT_TRUE(RuntimeConfig::watchdogConfig().restartOnDeadlock);
	utils::ThreadPoolManager::Config pool = RuntimeConfig::threadPoolManagerConfig();
	EXPECT_EQ(pool.baseThreadPoolSize, 3);
	EXPECT_EQ(pool.scheduledThreadPoolSize, 2);
	EXPECT_TRUE(pool.usePriorities);
	EXPECT_EQ(pool.maximumRuntimeInMillisecWithoutWarning, 7000);
	EXPECT_EQ(pool.coalesceAfterPeriods, 0);
	EXPECT_TRUE(pool.singleExecutor);
	EXPECT_TRUE(pool.serialMovement);

	ConfigurableProcessor::process(Properties{}, {&RuntimeConfig::bind, &ThreadConfig::bind}); // restore the defaults for other tests
}
