#include "aion/gameserver/runtime/services/detail/ServicesSupport.h"

#include <format>
#include <string_view>

#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::runtime::services_detail {

std::chrono::steady_clock::time_point clockNow() {
	return utils::ThreadPoolManager::clock().now(); // never creates the default pools
}

std::chrono::steady_clock::time_point clockNowNoexcept() noexcept {
	try {
		return clockNow();
	} catch (...) {
		return std::chrono::steady_clock::now();
	}
}

std::string describeTask(const TaskInfo& info) {
	std::string_view file = info.where.file_name();
	if (size_t slash = file.find_last_of("/\\"); slash != std::string_view::npos)
		file = file.substr(slash + 1);
	return std::format("{} task {}:{} ({})", info.kind != nullptr ? info.kind : TaskKind::UNKNOWN, file, info.where.line(), info.where.function_name());
}

} // namespace aion::gameserver::runtime::services_detail
