#pragma once

#include <chrono>
#include <string>

#include "aion/gameserver/runtime/base/TaskInfo.h"

namespace aion::gameserver::runtime::services_detail {

/**
 * Internal helpers of the services library (runtime/services, services/cron, utils/idfactory). Not part of the kernel API.
 */

/** steady time of the kernel Clock of ThreadPoolManager's installed backend (ManualClock under DeterministicExecutor, SystemClock if none). Never
 *  creates the default pools. */
std::chrono::steady_clock::time_point clockNow();

/** clockNow(), falling back to std::chrono::steady_clock if the backend cannot be obtained (noexcept paths such as census events). */
std::chrono::steady_clock::time_point clockNowNoexcept() noexcept;

/** "kind task file:line (function)" */
std::string describeTask(const TaskInfo& info);

} // namespace aion::gameserver::runtime::services_detail
