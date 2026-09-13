#pragma once

#include <spdlog/common.h>

namespace aion::commons::logging {

/**
 * Java: ch.qos.logback.classic.filter.LevelFilter (with onMatch=ACCEPT, onMismatch=DENY) and ThresholdFilter - decides whether an appender
 * accepts a message of a given level. spdlog's critical level counts as ERROR, like in the log output.
 */
struct LevelFilter {
	spdlog::level::level_enum level = spdlog::level::trace;
	/** true: only messages of exactly this level (LevelFilter), false: this level and above (ThresholdFilter) */
	bool exactMatch = false;

	/** Java: LevelFilter with onMatch=ACCEPT and onMismatch=DENY */
	static constexpr LevelFilter exactly(spdlog::level::level_enum level) noexcept { return {normalize(level), true}; }

	/** Java: ThresholdFilter */
	static constexpr LevelFilter threshold(spdlog::level::level_enum level) noexcept { return {normalize(level), false}; }

	constexpr bool accepts(spdlog::level::level_enum messageLevel) const noexcept {
		spdlog::level::level_enum normalized = normalize(messageLevel);
		return exactMatch ? normalized == level : normalized >= level;
	}

	constexpr bool operator==(const LevelFilter&) const noexcept = default;

private:
	static constexpr spdlog::level::level_enum normalize(spdlog::level::level_enum level) noexcept {
		return level == spdlog::level::critical ? spdlog::level::err : level;
	}
};

} // namespace aion::commons::logging
