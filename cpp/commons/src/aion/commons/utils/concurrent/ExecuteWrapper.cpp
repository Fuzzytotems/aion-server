#include "aion/commons/utils/concurrent/ExecuteWrapper.h"

#include "aion/commons/configs/CommonsConfig.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/ClassName.h"
#include "aion/commons/utils/concurrent/RunnableStatsManager.h"

namespace aion::commons::utils::concurrent::detail {

namespace {

const logging::Logger& log() {
	static const logging::Logger instance = logging::LoggerFactory::getLogger("com.aionemu.commons.utils.concurrent.ExecuteWrapper");
	return instance;
}

} // namespace

void afterExecution(const std::type_info& type, int64_t durationNanos, int64_t expectedMaxExecutionTimeMillis) {
	if (configs::CommonsConfig::RUNNABLESTATS_ENABLE)
		RunnableStatsManager::handleStats(type, durationNanos);

	int64_t durationMillis = durationNanos / 1'000'000; // TimeUnit.NANOSECONDS.toMillis truncates
	if (durationMillis > expectedMaxExecutionTimeMillis) {
		std::string name = isAnonymousClass(type) ? getClassName(type) : getSimpleClassName(type);
		log().warn("{} - execution time: {}ms", name, durationMillis);
	}
}

void logExecutionException() noexcept {
	try {
		log().errorCurrentException("Exception in a Runnable execution:");
	} catch (...) {
		// logging failed (e.g. out of memory), nothing else we can do
	}
}

} // namespace aion::commons::utils::concurrent::detail
