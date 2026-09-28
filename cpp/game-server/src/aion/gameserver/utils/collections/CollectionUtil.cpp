#include "aion/gameserver/utils/collections/CollectionUtil.h"

#include <exception>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::utils::collections {

namespace {
const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.collections.CollectionUtil"));
	return *logger;
}
} // namespace

void CollectionUtil::logError(const std::string& object, const std::function<std::string()>& exceptionLogDetailsSupplier) {
	std::string details = exceptionLogDetailsSupplier ? " (" + exceptionLogDetailsSupplier() + ")" : "";
	log().errorCurrentException("Could not perform operation on " + object + details);
}

} // namespace aion::gameserver::utils::collections
