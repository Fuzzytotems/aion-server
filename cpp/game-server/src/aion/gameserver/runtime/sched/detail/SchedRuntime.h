#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::runtime {

class Clock;
class ExecutorBackend;

/**
 * Internal glue between the sched sources (not part of the kernel API; include only from runtime/sched and utils/ThreadPoolManager.cpp).
 *
 * Future needs a few things that belong to ThreadPoolManager (which lives in the same library): the installed backend for helping get() in
 * single_executor / deterministic mode, the scheduler clock for getDelay(), and the configured slow-task threshold for explicit run().
 * These accessors never create the pools.
 */
namespace sched_detail {

/**
 * The backend installed in ThreadPoolManager (explicitly or lazily created), or nullptr. Never creates pools.
 * Lifetime: valid for the rest of the process (installBackend retires replaced backends and never destroys them).
 */
ExecutorBackend* installedBackend() noexcept;

/** Clock of the installed backend, or SystemClock if none is installed. */
const Clock& schedulerClock() noexcept;

/** ThreadPoolManager::Config::maximumRuntimeInMillisecWithoutWarning (Java ThreadConfig.MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING). */
int64_t maximumRuntimeWithoutWarningMillis() noexcept;

/** ThreadPoolManager::Config::coalesceAfterPeriods (gameserver.scheduler.coalesce_after; <= 0 disables coalescing). */
int32_t coalesceAfterPeriods() noexcept;
/** ThreadPoolManager::Config::coalesceMinimumLag. */
std::chrono::milliseconds coalesceMinimumLag() noexcept;

/** Java RunnableWrapper thresholds per pool: SCHEDULED/INSTANT use the config, LONG_RUNNING and NONE never warn. */
int64_t maximumRuntimeFor(PoolKind pool) noexcept;

/** "kind task file:line (function)" for log lines and introspection. */
std::string describeTask(const TaskInfo& info);

/** steady_clock time point + nanoseconds, saturating at time_point::max()/min(). */
std::chrono::steady_clock::time_point saturatingAdd(std::chrono::steady_clock::time_point base, int64_t nanos) noexcept;

/** Java priority of the calling thread as recorded by kernel thread factories (NORM_PRIORITY = 5 for other threads). */
int32_t currentThreadJavaPriority() noexcept;
/** Records the Java priority of the calling thread (pool threads). */
void setCurrentThreadJavaPriority(int32_t priority) noexcept;

/** Names the calling thread (commons setCurrentThreadName) and refreshes its ThreadContext name. */
void nameCurrentThread(const std::string& name);

} // namespace sched_detail

} // namespace aion::gameserver::runtime
