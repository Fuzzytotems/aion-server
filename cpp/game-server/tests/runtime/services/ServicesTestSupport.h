#pragma once

// Shared helpers of the services tests: a DeterministicExecutor fixture (ManualClock, exact reclamation), test objects, log capture and
// bounded real-time waits.

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::runtime::servicestest {

using utils::ThreadPoolManager;
using utils::idfactory::IDFactory;

/** Game object stand-in: counts live instances; the destructor pushes its object id to the CleanerQueue like ~AionObject. */
class TestObject : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<TestObject> create(int32_t objectId = 0, bool autoRelease = false) { return makeRef<TestObject>(objectId, autoRelease); }
	static inline std::atomic<int32_t> live{0};
	const int32_t objectId;
	const bool autoRelease;

protected:
	TestObject(int32_t objectId, bool autoRelease) : objectId(objectId), autoRelease(autoRelease) { live.fetch_add(1); }
	~TestObject() override {
		if (autoRelease)
			CleanerQueue::push(objectId, "TestObject");
		live.fetch_sub(1);
	}
};

/** Captures the messages of one logger subtree ("level|message" lines) while it exists. */
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

	std::string str() const {
		std::string text = stream.str();
		std::erase(text, '\r');
		return text;
	}
	bool contains(std::string_view text) const { return str().find(text) != std::string::npos; }
	size_t count(std::string_view text) const {
		std::string all = str();
		size_t found = 0;
		for (size_t pos = all.find(text); pos != std::string::npos; pos = all.find(text, pos + text.size()))
			++found;
		return found;
	}

private:
	std::string name;
	std::ostringstream stream;
};

/** Polls `predicate` in real time for at most `limit`. */
inline bool waitRealTime(const std::function<bool()>& predicate, std::chrono::milliseconds limit = std::chrono::milliseconds(10'000)) {
	auto deadline = std::chrono::steady_clock::now() + limit;
	while (!predicate()) {
		if (std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return true;
}

/** 2024-01-01T00:00:00Z (a Monday) */
inline constexpr int64_t JAN_1_2024_MILLIS = 1'704'067'200'000;

/**
 * Installs a DeterministicExecutor (ManualClock at 2024-01-01T00:00:00Z) and resets every services singleton before and after each test.
 * No TaskScope may be open on the test thread when reclaim() or the executor runs.
 */
class DeterministicServicesTest : public testing::Test {
protected:
	void SetUp() override {
		resetServices();
		ThreadPoolManager::installBackend(nullptr);
		ThreadPoolManager::configure(ThreadPoolManager::Config{});
		auto backend = std::make_unique<DeterministicExecutor>(clock, 42);
		executor = backend.get();
		ThreadPoolManager::installBackend(std::move(backend));
		IDFactory::getInstance().resetForTests();
	}

	void TearDown() override {
		resetServices();
		ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		reclaim();
		IDFactory::getInstance().resetForTests();
	}

	static void resetServices() {
		services::cron::CronService::resetForTests();
		LeakCensus::getInstance().uninstall();
		LeakCensus::getInstance().configure(LeakCensus::Config{});
		CleanerQueue::uninstall();
		CleanerQueue::setCleanerAction([](int32_t, const char*) {}); // leftovers of a failed test are dropped silently
		(void)CleanerQueue::drainNow();
		CleanerQueue::setCleanerAction(nullptr);
	}

	/** destroys everything retired so far and runs the post-scan hooks */
	static void reclaim() { Reclaimer::getInstance().drain(); }

	/** advances simulated time, running due tasks (and a reclamation after each) */
	void pass(std::chrono::milliseconds duration) { executor->advance(duration); }

	ManualClock clock{JAN_1_2024_MILLIS};
	DeterministicExecutor* executor = nullptr;
};

} // namespace aion::gameserver::runtime::servicestest
