#include "aion/gameserver/utils/chathandlers/AdminCommand.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/CommandsAccessService.h"

namespace aion::gameserver::utils::chathandlers {

static const auto log = commons::logging::LoggerFactory::getLogger("ADMINAUDIT_LOG");

AdminCommand::AdminCommand(std::string_view aliasValue) : AdminCommand(aliasValue, "", "") {
}

AdminCommand::AdminCommand(std::string_view aliasValue, std::string_view descriptionValue) : AdminCommand(aliasValue, descriptionValue, "") {
}

AdminCommand::AdminCommand(std::string_view aliasValue, std::string_view descriptionValue, std::string_view syntaxInfoValue)
	: ChatCommand(PREFIX, aliasValue, descriptionValue, syntaxInfoValue) {
}

// Java AdminCommand.java:37-43
bool AdminCommand::validateAccess(model::gameobjects::player::Player& player) {
	bool hasAccess = player.hasAccess(getLevel()) || services::CommandsAccessService::hasAccess(player.getObjectId(), getAliasForLevel());
	if (!hasAccess && player.isStaff())
		sendInfo(player, "<You need access level " + std::to_string(getLevel()) + " or higher to use " + getAliasWithPrefix() + ">");
	return hasAccess;
}

// Java AdminCommand.java:45-58
bool AdminCommand::process(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	if (!validateAccess(player))
		return player.isStaff(); // return false for regular players, so chat will send entered text (this way you can't guess commands without rights)

	if (configs::main::LoggingConfig::LOG_GMAUDIT.load()) {
		runtime::Ptr<model::gameobjects::VisibleObject> target = player.getTarget();
		log.info("[Admin Command] > [Player: " + player.getName() + "]" + (target ? "[Target: " + target->getName() + "]" : std::string()) + ": " +
				 getAliasWithPrefix() + " " + join(params, 0));
	}

	if (!run(player, params))
		sendInfo(player, "<Error while executing command>");

	return true;
}

} // namespace aion::gameserver::utils::chathandlers
