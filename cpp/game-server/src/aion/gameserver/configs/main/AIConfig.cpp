#include "aion/gameserver/configs/main/AIConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void AIConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.ai.move.debug", MOVE_DEBUG, "true");
	AION_BIND(p, "gameserver.ai.event.debug", EVENT_DEBUG, "false");
	AION_BIND(p, "gameserver.ai.oncreate.debug", ONCREATE_DEBUG, "false");
	AION_BIND(p, "gameserver.npcmovement.enable", ACTIVE_NPC_MOVEMENT, "true");
	AION_BIND(p, "gameserver.npcmovement.delay.minimum", MINIMIMUM_DELAY, "3");
	AION_BIND(p, "gameserver.npcmovement.delay.maximum", MAXIMUM_DELAY, "15");
	AION_BIND(p, "gameserver.npcshouts.enable", SHOUTS_ENABLE, "false");
	AION_BIND(p, "gameserver.ai.handler_directory", HANDLER_DIRECTORY, "./data/handlers/ai");
	AION_BIND(p, "gameserver.dev.missing_ai_handlers", MISSING_AI_HANDLERS, "fail"); // C++ only (docs/deviations/P4-01.md)
}

} // namespace aion::gameserver::configs::main
