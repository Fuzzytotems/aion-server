#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"

#include <string>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/CommandsAccessService.h"

namespace aion::gameserver::utils::chathandlers {

PlayerCommand::PlayerCommand(std::string_view aliasValue, std::string_view descriptionValue) : PlayerCommand(aliasValue, descriptionValue, "") {
}

PlayerCommand::PlayerCommand(std::string_view aliasValue, std::string_view descriptionValue, std::string_view syntaxInfoValue)
	: ChatCommand(PREFIX, aliasValue, descriptionValue, syntaxInfoValue) {
}

// Java PlayerCommand.java:28-34
bool PlayerCommand::validateAccess(model::gameobjects::player::Player& player) {
	bool hasAccess = player.hasPermission(getLevel()) || services::CommandsAccessService::hasAccess(player.getObjectId(), getAliasForLevel());
	if (!hasAccess && player.isStaff())
		sendInfo(player, "<You need membership level " + std::to_string(getLevel()) + " or higher to use " + getAliasWithPrefix() + ">");
	return hasAccess;
}

// Java PlayerCommand.java:36-44
bool PlayerCommand::process(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	if (!validateAccess(player))
		return player.isStaff(); // return false for regular players, so chat will send entered text (this way you can't guess commands without rights)

	if (!run(player, params))
		sendInfo(player, "<Error while executing command>");

	return true;
}

} // namespace aion::gameserver::utils::chathandlers
