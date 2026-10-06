#include "aion/gameserver/handlers/playercommands/Help.h"

#include <algorithm>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"
#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"

namespace aion::gameserver::handlers::playercommands {

AION_PLAYER_COMMAND(Help);

Help::Help() : PlayerCommand("help", "Lists all commands you are allowed to use.") {
}

// Java Help.java:28-45
void Help::execute(Player& player, std::span<const std::string> /*params*/) {
	using utils::JavaColor;
	std::vector<ChatCommand*> allowedCommands = findAllowedCommands(player);
	if (!allowedCommands.empty() && !(allowedCommands.size() == 1 && std::ranges::find(allowedCommands, this) != allowedCommands.end())) {
		// Java: List.sort (stable) by the lower-cased alias with prefix
		std::ranges::stable_sort(allowedCommands, {}, [](ChatCommand* cmd) { return commons::utils::StringUtils::toLowerCase(cmd->getAliasWithPrefix()); });
		std::string sb = "List of available commands (" + std::to_string(allowedCommands.size()) + "):";
		for (ChatCommand* cmd : allowedCommands) {
			std::string desc = cmd->getDescription().empty() ? std::string("No description available.") : cmd->getDescription();
			sb += "\n\t" + ChatUtil::color(cmd->getAliasWithPrefix(), JavaColor::WHITE) + " - " + desc;
		}
		sb += "\nType <" + ChatUtil::color("command", JavaColor::WHITE) + "> " + ChatUtil::color("help", JavaColor::WHITE) +
			" to get further information about a command.";
		sendInfo(player, sb);
	} else {
		sendInfo(player, "You are not allowed to use any chat commands other than " + ChatUtil::color(getAliasWithPrefix(), JavaColor::WHITE) + ".");
	}
}

// Java Help.java:47-55
std::vector<ChatCommand*> Help::findAllowedCommands(Player& player) {
	ChatProcessor& cp = ChatProcessor::getInstance();
	std::vector<ChatCommand*> cmds;
	for (ChatCommand* cmd : cp.getCommandList()) {
		if (cp.isCommandAllowed(player, cmd) && dynamic_cast<ConsoleCommand*>(cmd) == nullptr)
			cmds.push_back(cmd);
	}
	return cmds;
}

} // namespace aion::gameserver::handlers::playercommands
