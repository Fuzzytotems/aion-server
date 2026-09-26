#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace aion::gameserver::runtime {

/**
 * Time source of the scheduler, cron and time-dependent kernel services (design §7.7). Production uses SystemClock; tests install a
 * DeterministicExecutor with a ManualClock. Implementations are thread-safe.
 */
class Clock {
public:
	virtual ~Clock() = default;
	/** monotonic time for delays, periods and timeouts */
	virtual std::chrono::steady_clock::time_point now() const noexcept = 0;
	/** wall clock (Java System.currentTimeMillis) for cron fire times and ID quarantine timestamps */
	virtual int64_t currentTimeMillis() const noexcept = 0;
};

/** std::chrono::steady_clock / system_clock. */
class SystemClock final : public Clock {
public:
	static const SystemClock& getInstance() noexcept;
	std::chrono::steady_clock::time_point now() const noexcept override { return std::chrono::steady_clock::now(); }
	int64_t currentTimeMillis() const noexcept override;
};

/**
 * Manually advanced clock for deterministic tests (design §7.7). Both time lines advance together. Thread-safe (atomics); advancing does not
 * run anything by itself (DeterministicExecutor::advance does).
 */
class ManualClock final : public Clock {
public:
	/** Default wall time: 2024-01-01T00:00:00Z. */
	explicit ManualClock(int64_t wallMillis = 1'704'067'200'000) noexcept : wallMillis(wallMillis) {}

	std::chrono::steady_clock::time_point now() const noexcept override {
		return std::chrono::steady_clock::time_point(std::chrono::nanoseconds(steadyNanos.load(std::memory_order_acquire)));
	}
	int64_t currentTimeMillis() const noexcept override { return wallMillis.load(std::memory_order_acquire); }

	void advance(std::chrono::nanoseconds delta) noexcept {
		steadyNanos.fetch_add(delta.count(), std::memory_order_acq_rel);
		wallMillis.fetch_add(std::chrono::duration_cast<std::chrono::milliseconds>(delta).count(), std::memory_order_acq_rel);
	}
	/** Sets the wall clock only (e.g. to test cron across a DST change); the steady clock is unchanged. */
	void setCurrentTimeMillis(int64_t millis) noexcept { wallMillis.store(millis, std::memory_order_release); }

private:
	std::atomic<int64_t> steadyNanos{1'000'000'000};
	std::atomic<int64_t> wallMillis;
};

} // namespace aion::gameserver::runtime
