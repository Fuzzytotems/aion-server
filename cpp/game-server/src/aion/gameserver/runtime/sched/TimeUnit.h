#pragma once

#include <chrono>
#include <cstdint>
#include <limits>

namespace aion::gameserver::runtime {

/** Java: java.util.concurrent.TimeUnit (design §7.1). Also exported as aion::gameserver::utils::TimeUnit by ThreadPoolManager.h. */
enum class TimeUnit : uint8_t { NANOSECONDS, MICROSECONDS, MILLISECONDS, SECONDS, MINUTES, HOURS, DAYS };

/** Java TimeUnit.toNanos: saturates at the int64 limits like Java. */
constexpr int64_t toNanos(int64_t duration, TimeUnit unit) noexcept {
	int64_t factor = 1;
	switch (unit) {
		case TimeUnit::NANOSECONDS:
			factor = 1;
			break;
		case TimeUnit::MICROSECONDS:
			factor = 1'000;
			break;
		case TimeUnit::MILLISECONDS:
			factor = 1'000'000;
			break;
		case TimeUnit::SECONDS:
			factor = 1'000'000'000;
			break;
		case TimeUnit::MINUTES:
			factor = 60'000'000'000;
			break;
		case TimeUnit::HOURS:
			factor = 3'600'000'000'000;
			break;
		case TimeUnit::DAYS:
			factor = 86'400'000'000'000;
			break;
	}
	if (duration > std::numeric_limits<int64_t>::max() / factor)
		return std::numeric_limits<int64_t>::max();
	if (duration < std::numeric_limits<int64_t>::min() / factor)
		return std::numeric_limits<int64_t>::min();
	return duration * factor;
}

/** Java TimeUnit.convert(sourceDuration in nanoseconds, target unit): truncates toward zero like Java. */
constexpr int64_t fromNanos(int64_t nanos, TimeUnit unit) noexcept {
	return nanos / toNanos(1, unit);
}

inline std::chrono::nanoseconds toDuration(int64_t duration, TimeUnit unit) noexcept {
	return std::chrono::nanoseconds(toNanos(duration, unit));
}

} // namespace aion::gameserver::runtime
