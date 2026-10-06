#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/CommandsAccessService.h"

namespace aion::gameserver::utils::chathandlers {

const commons::logging::Logger ConsoleCommand::log = commons::logging::LoggerFactory::getLogger("ADMINAUDIT_LOG");

ConsoleCommand::ConsoleCommand(std::string_view aliasValue) : ConsoleCommand(aliasValue, "", "") {
}

ConsoleCommand::ConsoleCommand(std::string_view aliasValue, std::string_view descriptionValue) : ConsoleCommand(aliasValue, descriptionValue, "") {
}

ConsoleCommand::ConsoleCommand(std::string_view aliasValue, std::string_view descriptionValue, std::string_view syntaxInfoValue)
	: ChatCommand(PREFIX, aliasValue, descriptionValue, syntaxInfoValue) {
}

// Java ConsoleCommand.java:41-47
bool ConsoleCommand::validateAccess(model::gameobjects::player::Player& player) {
	bool hasAccess = player.hasAccess(getLevel()) || services::CommandsAccessService::hasAccess(player.getObjectId(), getAliasForLevel());
	if (!hasAccess && player.isStaff())
		sendInfo(player, "<You need access level " + std::to_string(getLevel()) + " or higher to use " + getAliasWithPrefix() + ">");
	return hasAccess;
}

// Java ConsoleCommand.java:49-62
bool ConsoleCommand::process(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	if (!validateAccess(player))
		return player.isStaff(); // return false for regular players, so chat will send entered text (this way you can't guess commands without rights)

	if (configs::main::LoggingConfig::LOG_GMAUDIT.load()) {
		runtime::Ptr<model::gameobjects::VisibleObject> target = player.getTarget();
		log.info("[Console Command] > [Player: " + player.getName() + "]" + (target ? "[Target: " + target->getName() + "]" : std::string()) +
				 ": " + getAliasWithPrefix() + " " + join(params, 0));
	}

	if (!run(player, params))
		sendInfo(player, "<Error while executing command>");

	return true;
}

} // namespace aion::gameserver::utils::chathandlers
