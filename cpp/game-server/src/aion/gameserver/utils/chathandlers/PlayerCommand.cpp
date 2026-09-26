#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::utils::chathandlers {

PlayerCommand::PlayerCommand(std::string_view aliasValue, std::string_view descriptionValue) : PlayerCommand(aliasValue, descriptionValue, "") {
}

PlayerCommand::PlayerCommand(std::string_view aliasValue, std::string_view descriptionValue, std::string_view syntaxInfoValue)
	: ChatCommand(PREFIX, aliasValue, descriptionValue, syntaxInfoValue) {
}

bool PlayerCommand::validateAccess(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool PlayerCommand::process(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::utils::chathandlers
