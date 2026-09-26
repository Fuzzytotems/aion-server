// Text reports behind //tasks and //debug (design §7.8). See Introspection.h.

#include "aion/gameserver/runtime/services/Introspection.h"

#include <format>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/runtime/services/detail/ServicesSupport.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::runtime::introspection {

std::vector<std::string> tasksFor(const RefCounted& target) {
	std::vector<TaskInfo> tasks = utils::ThreadPoolManager::getInstance().tasksPinning(target);
	std::vector<std::string> lines;
	lines.push_back(std::format("{} pending task(s) pin the target", tasks.size()));
	for (const TaskInfo& info : tasks)
		lines.push_back(services_detail::describeTask(info));
	return lines;
}

std::vector<std::string> debugTasks() {
	utils::ThreadPoolManager& pools = utils::ThreadPoolManager::getInstance();
	std::vector<std::string> lines = pools.getStats();
	for (std::string& line : pools.getPendingTaskSummary())
		lines.push_back(std::move(line));
	return lines;
}

std::vector<std::string> debugRefs() {
	Reclaimer::Stats stats = Reclaimer::getInstance().stats();
	return {
		std::format("destroyed objects: {}, reclamation backlog: {} ({} bytes), scans: {}", stats.destroyedTotal, stats.backlog, stats.backlogBytes,
			stats.scans),
		std::format("removed from the world and not yet destroyed: {}, cleaner queue: {} id(s)", LeakCensus::getInstance().trackedCount(),
			CleanerQueue::size()),
	};
}

std::vector<std::string> debugLeaks() {
	LeakCensus& census = LeakCensus::getInstance();
	std::vector<LeakCensus::LeakReport> leaks = census.getLeaks();
	std::vector<std::string> lines;
	lines.push_back(std::format("{} leak(s), {} tracked object(s), {} zombie breaker cut(s){}", leaks.size(), census.trackedCount(), census.zombieCutCount(),
		census.isInstalled() ? "" : " (leak census not installed)"));
	for (const LeakCensus::LeakReport& leak : leaks) {
		std::string line = std::format("{} (id {}): refcount {}, removed {} min ago", leak.className, leak.objectId, leak.refCount,
			std::chrono::duration_cast<std::chrono::minutes>(leak.removedFor).count());
		for (const char* edge : leak.cutEdges)
			line += std::format(", cut {}", edge);
		lines.push_back(std::move(line));
		for (const TaskInfo& info : leak.pinningTasks)
			lines.push_back("  pinned by " + services_detail::describeTask(info));
	}
	return lines;
}

std::vector<std::string> debugLocks() {
	return LockOrderValidator::getInstance().describe();
}

std::vector<std::string> debugReclaimer() {
	Reclaimer::Stats stats = Reclaimer::getInstance().stats();
	std::vector<std::string> lines;
	lines.push_back(std::format("epoch {}, min active {}, lag {} ms, scans {}, thread {}", stats.epoch, stats.minActive, stats.lag.count(), stats.scans,
		Reclaimer::getInstance().isRunning() ? "running" : "stopped"));
	lines.push_back(std::format("backlog {} entries, {} bytes, destroyed {}", stats.backlog, stats.backlogBytes, stats.destroyedTotal));
	if (stats.oldestPublishedThreadId == 0) {
		lines.push_back("oldest published scope: none");
		return lines;
	}
	lines.push_back(std::format("oldest published scope: thread {} {}", stats.oldestPublishedThreadId, services_detail::describeTask(stats.oldestPublishedTask)));
	int64_t now = commons::utils::nanoTime();
	ThreadContext::forEach([&](const ThreadContext& context) {
		if (context.threadId() != stats.oldestPublishedThreadId)
			return;
		lines.back() += std::format(" on \"{}\"", context.threadName());
		ThreadContext::BlockingSnapshot blocking = context.blocking();
		if (blocking.active)
			lines.push_back(std::format("  blocking in {} at {}:{} for {} ms", blocking.what != nullptr ? blocking.what : "?", blocking.where.file_name(),
				blocking.where.line(), (now - blocking.sinceNanos) / 1'000'000));
	});
	return lines;
}

} // namespace aion::gameserver::runtime::introspection
