#pragma once

// Shared helpers of the sched tests: a RefCounted test object, backend fixtures (real pools and DeterministicExecutor) and time helpers that
// advance simulated time on the DeterministicExecutor and wait in real time (bounded) on the real pools.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sched/PoolBackends.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::runtime::schedtest {

using utils::ThreadPoolManager;

/** Game object stand-in: counts live instances so tests can check exact destruction and pin release. */
class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create() { return makeRef<Npc>(); }
	static inline std::atomic<int32_t> live{0};
	std::atomic<int32_t> runs{0};

protected:
	Npc() { live.fetch_add(1); }
	~Npc() override { live.fetch_sub(1); }
};

/** Destroys everything retired so far (no TaskScope may be open on the calling thread). */
inline void reclaimAll() {
	Reclaimer::getInstance().drain();
}

enum class BackendKind { REAL, DETERMINISTIC };

inline std::string backendName(const testing::TestParamInfo<BackendKind>& info) {
	return info.param == BackendKind::REAL ? "RealPools" : "Deterministic";
}

/** Default configuration of the tests (quiet slow-task threshold, Java coalescing values). */
inline ThreadPoolManager::Config testConfig() {
	ThreadPoolManager::Config config;
	config.maximumRuntimeInMillisecWithoutWarning = 5000;
	config.coalesceAfterPeriods = 10;
	config.coalesceMinimumLag = std::chrono::milliseconds(2000);
	return config;
}

/**
 * Installs a fresh backend for every test and removes it afterwards. Time helpers:
 * - pass(d): DETERMINISTIC advances the ManualClock by d running due tasks; REAL sleeps d.
 * - waitUntil(pred, limit): DETERMINISTIC advances in 1 ms steps up to `limit` simulated time; REAL polls up to `limit` + REAL_SLACK of real time.
 */
class SchedBackendTest : public testing::TestWithParam<BackendKind> {
protected:
	static constexpr std::chrono::milliseconds REAL_SLACK{10'000};

	void SetUp() override { install(GetParam(), testConfig()); }

	void TearDown() override {
		ThreadPoolManager::installBackend(nullptr);
		deterministic = nullptr;
		ForkJoinPool::commonPool().setSerial(false);
		reclaimAll();
	}

	void install(BackendKind kind, const ThreadPoolManager::Config& config, size_t instantQueueCapacity = 100'000, int32_t instantThreads = 4) {
		ThreadPoolManager::installBackend(nullptr);
		ThreadPoolManager::configure(config);
		if (kind == BackendKind::DETERMINISTIC) {
			auto executor = std::make_unique<DeterministicExecutor>(clock, 42);
			deterministic = executor.get();
			ThreadPoolManager::installBackend(std::move(executor));
		} else {
			ThreadPoolBackend::Options options;
			options.instantThreads = instantThreads;
			options.scheduledThreads = 4;
			options.instantQueueCapacity = instantQueueCapacity;
			ThreadPoolManager::installBackend(std::make_unique<ThreadPoolBackend>(options));
			deterministic = nullptr;
		}
	}

	bool isDeterministic() const { return deterministic != nullptr; }

	void pass(std::chrono::milliseconds duration) {
		if (deterministic != nullptr)
			deterministic->advance(duration);
		else
			std::this_thread::sleep_for(duration);
	}

	bool waitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds limit = std::chrono::milliseconds(5000)) {
		if (deterministic != nullptr) {
			deterministic->runReady();
			for (auto waited = std::chrono::milliseconds(0); !predicate(); waited += std::chrono::milliseconds(1)) {
				if (waited >= limit)
					return false;
				deterministic->advance(std::chrono::milliseconds(1));
			}
			return true;
		}
		auto deadline = std::chrono::steady_clock::now() + limit + REAL_SLACK;
		while (!predicate()) {
			if (std::chrono::steady_clock::now() >= deadline)
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return true;
	}

	ManualClock clock;
	DeterministicExecutor* deterministic = nullptr;
};

} // namespace aion::gameserver::runtime::schedtest
