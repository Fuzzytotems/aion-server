#include "aion/gameserver/utils/chathandlers/AdminCommand.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::utils::chathandlers {

static const auto log = commons::logging::LoggerFactory::getLogger("ADMINAUDIT_LOG");

AdminCommand::AdminCommand(std::string_view aliasValue) : AdminCommand(aliasValue, "", "") {
}

AdminCommand::AdminCommand(std::string_view aliasValue, std::string_view descriptionValue) : AdminCommand(aliasValue, descriptionValue, "") {
}

AdminCommand::AdminCommand(std::string_view aliasValue, std::string_view descriptionValue, std::string_view syntaxInfoValue)
	: ChatCommand(PREFIX, aliasValue, descriptionValue, syntaxInfoValue) {
}

bool AdminCommand::validateAccess(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AdminCommand::process(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::utils::chathandlers
