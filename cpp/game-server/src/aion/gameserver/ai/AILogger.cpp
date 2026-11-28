#include "aion/gameserver/ai/AILogger.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::ai {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.ai.AILogger");

void AILogger::info(AbstractAI& ai, std::string_view message) {
	if (ai.isLogging()) {
		log.info("[AI] {} - {}", ai.getOwner().getObjectId(), message);
	}
}

void AILogger::moveinfo(model::gameobjects::Creature& owner, std::string_view message) {
	if (configs::main::AIConfig::MOVE_DEBUG.load() && owner.getAi().isLogging()) {
		log.info("[AI] {} - {}", owner.getObjectId(), message);
	}
}

} // namespace aion::gameserver::ai
