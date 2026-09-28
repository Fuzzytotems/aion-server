#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <regex>
#include <sstream>
#include <thread>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/concurrent/ExecuteWrapper.h"
#include "aion/commons/utils/concurrent/RunnableStatsManager.h"
#include "aion/commons/utils/concurrent/RunnableWrapper.h"

using namespace aion::commons;
using namespace aion::commons::utils::concurrent;

namespace {

class CapturedLog {
public:
	CapturedLog() {
		sink->set_pattern("%l|%v");
		logging::LoggerFactory::configure("com.aionemu.commons.utils.concurrent.ExecuteWrapper", {.sinks = {sink}, .additive = false});
	}
	~CapturedLog() { logging::LoggerFactory::removeConfig("com.aionemu.commons.utils.concurrent.ExecuteWrapper"); }
	std::string str() const { return stream.str(); }

private:
	std::ostringstream stream;
	std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
};

struct BasePacket {
	virtual ~BasePacket() = default;
	virtual void run() = 0;
};

struct CM_SLOW_PACKET : BasePacket {
	int runs = 0;
	void run() override {
		runs++;
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
	}
};

struct StatsEnabled {
	StatsEnabled() { configs::CommonsConfig::RUNNABLESTATS_ENABLE = true; }
	~StatsEnabled() { configs::CommonsConfig::RUNNABLESTATS_ENABLE = false; }
};

bool statsContain(std::string_view text) {
	for (const auto& line : RunnableStatsManager::getClassStatsLines(std::nullopt)) {
		if (line.find(text) != std::string::npos)
			return true;
	}
	return false;
}

} // namespace

TEST(ExecuteWrapperTest, RunsCallablesAndRunMethods) {
	CapturedLog log;
	int calls = 0;
	ExecuteWrapper wrapper(1000);
	wrapper.execute([&] { calls++; });
	std::function<void()> function = [&] { calls++; };
	wrapper.execute(function);
	CM_SLOW_PACKET packet;
	wrapper.execute(packet);
	BasePacket& base = packet;
	wrapper(base);
	auto owned = std::make_unique<CM_SLOW_PACKET>();
	wrapper.execute(owned);
	EXPECT_EQ(calls, 2);
	EXPECT_EQ(packet.runs, 2);
	EXPECT_EQ(owned->runs, 1);
	EXPECT_EQ(log.str(), "");
}

TEST(ExecuteWrapperTest, SlowExecutionWarningUsesSimpleClassName) {
	CapturedLog log;
	CM_SLOW_PACKET packet;
	BasePacket& base = packet;
	ExecuteWrapper(10).execute(base); // the dynamic type is reported
	std::string out = log.str();
	EXPECT_TRUE(out.starts_with("warning|CM_SLOW_PACKET - execution time: ")) << out;
	EXPECT_NE(out.find("ms"), std::string::npos);
}

TEST(ExecuteWrapperTest, SlowLambdaWarningUsesFullName) {
	CapturedLog log;
	ExecuteWrapper::execute([] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); }, 10, true);
	std::string out = log.str();
	EXPECT_NE(out.find("lambda"), std::string::npos) << out;
	EXPECT_NE(out.find("SlowLambdaWarningUsesFullName"), std::string::npos) << out; // the enclosing function is part of a lambda's name
}

TEST(ExecuteWrapperTest, NoWarningWithinExpectedTime) {
	CapturedLog log;
	// a generous limit, so preemption of the test thread cannot cause a warning
	ExecuteWrapper::execute([] {}, 60'000, true);
	ExecuteWrapper(60'000).execute([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
	EXPECT_EQ(log.str(), "");
}

TEST(ExecuteWrapperTest, TaskMayReplaceItsOwnHolder) {
	struct SelfReplacingTask {};
	struct ReplacementTask {
		void operator()() const {}
	};
	StatsEnabled enabled;
	std::function<void()> slot;
	slot = [&slot] { slot = ReplacementTask{}; }; // destroys the running lambda's holder state, like a task that cancels its own map entry
	ExecuteWrapper(60'000).execute(slot);
	EXPECT_TRUE(slot.target_type() == typeid(ReplacementTask));
	// the stats are recorded for the task that ran (its type was read before running it), not for the replacement
	EXPECT_FALSE(statsContain("ReplacementTask"));
	EXPECT_TRUE(statsContain("TaskMayReplaceItsOwnHolder"));
}

TEST(ExecuteWrapperTest, ExceptionsAreLoggedOrRethrown) {
	CapturedLog log;
	ExecuteWrapper(100).execute([] { throw utils::IllegalStateException("broken task"); });
	std::string out = log.str();
	EXPECT_TRUE(out.starts_with("error|Exception in a Runnable execution:\naion::commons::utils::IllegalStateException: broken task")) << out;

	EXPECT_THROW(ExecuteWrapper::execute([] { throw utils::IllegalStateException("x"); }, 100, false), utils::IllegalStateException);
	EXPECT_THROW(RunnableWrapper([] { throw std::runtime_error("y"); }, 100, false).run(), std::runtime_error);
}

TEST(ExecuteWrapperTest, RecordsStatsWhenEnabled) {
	RunnableStatsManager::clear(); // statistics are global and survive --gtest_repeat
	struct StatsTask {
		void operator()() const {}
	};
	struct UnrecordedTask {
		void operator()() const {}
	};
	ExecuteWrapper(100).execute(UnrecordedTask{});
	{
		StatsEnabled enabled;
		ExecuteWrapper(100).execute(StatsTask{});
		std::function<void()> wrapped = StatsTask{};
		ExecuteWrapper(100).execute(wrapped); // std::function: the stored target type
	}
	EXPECT_TRUE(statsContain("StatsTask\" "));
	EXPECT_FALSE(statsContain("UnrecordedTask\""));
	bool countOfTwo = false;
	for (const auto& line : RunnableStatsManager::getClassStatsLines(std::nullopt)) // numbers are right aligned to the longest value
		countOfTwo |= line.find("StatsTask\"") != std::string::npos && std::regex_search(line, std::regex(R"(count= *"2")"));
	EXPECT_TRUE(countOfTwo);
}

TEST(ExecuteWrapperTest, RunnableWrapperDefaults) {
	CapturedLog log;
	int calls = 0;
	RunnableWrapper wrapper([&] {
		calls++;
		throw std::runtime_error("logged");
	});
	std::function<void()> asFunction = wrapper;
	asFunction();
	wrapper();
	EXPECT_EQ(calls, 2);
	EXPECT_NE(log.str().find("std::runtime_error: logged"), std::string::npos);
}
