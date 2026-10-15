#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"

namespace aion::gameserver::questEngine::handlers {

HandlerResult fromBoolean(std::optional<bool> value) {
	if (!value)
		return HandlerResult::UNKNOWN;
	else if (*value)
		return HandlerResult::SUCCESS;
	return HandlerResult::FAILED;
}

} // namespace aion::gameserver::questEngine::handlers
