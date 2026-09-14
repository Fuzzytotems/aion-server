#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::utils::chathandlers {

const commons::logging::Logger ConsoleCommand::log = commons::logging::LoggerFactory::getLogger("ADMINAUDIT_LOG");

ConsoleCommand::ConsoleCommand(std::string_view aliasValue) : ConsoleCommand(aliasValue, "", "") {
}

ConsoleCommand::ConsoleCommand(std::string_view aliasValue, std::string_view descriptionValue) : ConsoleCommand(aliasValue, descriptionValue, "") {
}

ConsoleCommand::ConsoleCommand(std::string_view aliasValue, std::string_view descriptionValue, std::string_view syntaxInfoValue)
	: ChatCommand(PREFIX, aliasValue, descriptionValue, syntaxInfoValue) {
}

bool ConsoleCommand::validateAccess(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool ConsoleCommand::process(model::gameobjects::player::Player& player, std::span<const std::string> params) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::utils::chathandlers
