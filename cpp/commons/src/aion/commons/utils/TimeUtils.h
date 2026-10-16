#pragma once

#include <chrono>
#include <cstdint>

namespace aion::commons::utils {

/** Java: System.currentTimeMillis() - wall clock milliseconds since the Unix epoch. */
inline int64_t currentTimeMillis() noexcept {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/** Java: System.nanoTime() - monotonic clock for measuring elapsed time only. */
inline int64_t nanoTime() noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace aion::commons::utils
