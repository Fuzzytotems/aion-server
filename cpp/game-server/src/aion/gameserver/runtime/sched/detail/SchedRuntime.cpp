#include "aion/gameserver/runtime/sched/detail/SchedRuntime.h"

#include <format>
#include <limits>
#include <string_view>

#include "aion/commons/utils/concurrent/PriorityThreadFactory.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"

namespace aion::gameserver::runtime::sched_detail {

namespace {

thread_local int32_t javaPriority = commons::utils::concurrent::NORM_PRIORITY;

std::string_view fileName(const char* path) noexcept {
	std::string_view view(path != nullptr ? path : "");
	size_t slash = view.find_last_of("/\\");
	return slash == std::string_view::npos ? view : view.substr(slash + 1);
}

} // namespace

int64_t maximumRuntimeFor(PoolKind pool) noexcept {
	switch (pool) {
		case PoolKind::SCHEDULED:
		case PoolKind::INSTANT:
			return maximumRuntimeWithoutWarningMillis();
		case PoolKind::LONG_RUNNING:
		case PoolKind::NONE:
			break;
	}
	return std::numeric_limits<int64_t>::max();
}

std::string describeTask(const TaskInfo& info) {
	return std::format("{} task {}:{} ({})", info.kind != nullptr ? info.kind : TaskKind::UNKNOWN, fileName(info.where.file_name()), info.where.line(),
		info.where.function_name());
}

std::chrono::steady_clock::time_point saturatingAdd(std::chrono::steady_clock::time_point base, int64_t nanos) noexcept {
	using TimePoint = std::chrono::steady_clock::time_point;
	int64_t baseNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(base.time_since_epoch()).count();
	if (nanos > 0 && baseNanos > std::numeric_limits<int64_t>::max() - nanos)
		return TimePoint::max();
	if (nanos < 0 && baseNanos < std::numeric_limits<int64_t>::min() - nanos)
		return TimePoint::min();
	return TimePoint(std::chrono::nanoseconds(baseNanos + nanos));
}

int32_t currentThreadJavaPriority() noexcept {
	return javaPriority;
}

void setCurrentThreadJavaPriority(int32_t priority) noexcept {
	javaPriority = priority;
}

void nameCurrentThread(const std::string& name) {
	commons::utils::concurrent::setCurrentThreadName(name);
	ThreadContext::current().refreshName();
}

} // namespace aion::gameserver::runtime::sched_detail
