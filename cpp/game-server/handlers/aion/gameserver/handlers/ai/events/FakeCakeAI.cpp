#include "aion/gameserver/handlers/ai/events/FakeCakeAI.h"

#include <format>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(FakeCakeAI, "pandoras_box");

// Java FakeCakeAI.java:20-23
void FakeCakeAI::handleUseItemFinish(Player& player) {
	punishment(player);
}

// Java FakeCakeAI.java:25-28 (String.format of the player and his position: their toString)
void FakeCakeAI::punishment(Player& player) {
	utils::audit::AuditLogger::log(player, std::format("{} used the fake cake at {}.", player.toString(), player.getPosition()->toString()));
	getOwner().getController().die();
}

} // namespace aion::gameserver::handlers::ai::events
