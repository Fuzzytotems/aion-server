#pragma once

#include <string>
#include <vector>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::runtime {

/**
 * Text reports behind the admin commands of design §7.8. Each returns display lines; safe to call from any task.
 * - `//tasks <target>`: tasksFor(target) - pending tasks pinning the target with call sites
 * - `//debug tasks`: ThreadPoolManager stats and pending tasks by call site
 * - `//debug refs`: Reclaimer counters and live object counts per class (where tracked)
 * - `//debug leaks`: LeakCensus::getLeaks, including zombie-breaker cuts
 * - `//debug locks`: LockOrderValidator::describe (edges, reports, blocking under Monitors)
 * - `//debug reclaimer`: epoch, lag, oldest published scope (thread, TaskInfo, BlockingRegion), backlog objects/bytes
 */
namespace introspection {

std::vector<std::string> tasksFor(const RefCounted& target);
std::vector<std::string> debugTasks();
std::vector<std::string> debugRefs();
std::vector<std::string> debugLeaks();
std::vector<std::string> debugLocks();
std::vector<std::string> debugReclaimer();

} // namespace introspection

} // namespace aion::gameserver::runtime
