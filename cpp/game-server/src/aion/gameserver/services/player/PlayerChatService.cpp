#include "aion/gameserver/services/player/PlayerChatService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::player {

static const auto playerLog = commons::logging::LoggerFactory::getLogger("CHAT_LOG");
static const auto gmLog = commons::logging::LoggerFactory::getLogger("ADMINAUDIT_LOG");

bool PlayerChatService::isFlooding(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerChatService::logWhisper(model::gameobjects::player::Player& sender, model::gameobjects::player::Player& receiver,
	std::string_view message) {
	AION_UNPORTED();
}

void PlayerChatService::logMessage(model::gameobjects::player::Player& sender, model::ChatType type, std::string_view message) {
	AION_UNPORTED();
}

void PlayerChatService::logMessage(model::gameobjects::player::Player& sender, model::ChatType type, std::string_view message,
	runtime::Ptr<model::gameobjects::player::Player> receiver) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::player
