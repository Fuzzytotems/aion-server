#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"

namespace aion::gameserver::handlers::ai {

std::string GeneralNpcAI::describe() const {
	return "general:" + getOwner().name;
}

AION_AI(GeneralNpcAI, "general");

} // namespace aion::gameserver::handlers::ai
