#pragma once

#include <source_location>

namespace aion::gameserver::runtime {

/** Well-known values for TaskInfo::kind. Entry points may use other string literals. */
struct TaskKind {
	static constexpr const char* UNKNOWN = "unknown";
	static constexpr const char* MAIN = "main";
	static constexpr const char* STARTUP = "startup";
	static constexpr const char* SHUTDOWN = "shutdown";
	static constexpr const char* PACKET = "packet";
	static constexpr const char* IO = "io";
	static constexpr const char* SCHEDULED = "scheduled";
	static constexpr const char* INSTANT = "instant";
	static constexpr const char* LONG_RUNNING = "long-running";
	static constexpr const char* CRON = "cron";
	static constexpr const char* FORK_JOIN = "fork-join";
	static constexpr const char* SERIAL = "serial";
	static constexpr const char* CLEANER = "cleaner";
	static constexpr const char* CALLBACK = "callback";
	static constexpr const char* RECLAIMER = "reclaimer";
	static constexpr const char* WATCHDOG = "watchdog";
	static constexpr const char* TEST = "test";
};

/**
 * Identifies a unit of work for diagnostics (design §1.2): where it was created (schedule/execute call site, packet class, cron job...) and
 * what kind of entry point runs it. Trivially copyable and cheap: `where` points to static strings and `kind` must be a string literal (or
 * another string with static storage duration), so a TaskInfo may be copied into per-thread records that other threads read (watchdog,
 * Reclaimer stats, leak census) without lifetime concerns.
 */
struct TaskInfo {
	std::source_location where;
	const char* kind = TaskKind::UNKNOWN;
};

/** TaskInfo for the current source location: `TaskScope scope(AION_TASK_INFO(TaskKind::TEST));` */
#define AION_TASK_INFO(kind) (::aion::gameserver::runtime::TaskInfo{std::source_location::current(), kind})

} // namespace aion::gameserver::runtime
