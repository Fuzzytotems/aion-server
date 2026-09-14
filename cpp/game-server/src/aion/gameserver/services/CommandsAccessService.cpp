#include "aion/gameserver/services/CommandsAccessService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

void CommandsAccessService::loadAccesses() {
	AION_UNPORTED();
}

void CommandsAccessService::giveTemporaryAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command) {
	AION_UNPORTED();
}

void CommandsAccessService::giveAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command) {
	AION_UNPORTED();
}

void CommandsAccessService::giveAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command, bool isTemporary) {
	AION_UNPORTED();
}

void CommandsAccessService::removeAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command) {
	AION_UNPORTED();
}

bool CommandsAccessService::removeAllAccesses(int32_t playerId) {
	AION_UNPORTED();
}

bool CommandsAccessService::hasAccess(int32_t playerId, std::string_view command) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
