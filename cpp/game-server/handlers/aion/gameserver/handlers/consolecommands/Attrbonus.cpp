#include "aion/gameserver/handlers/consolecommands/Attrbonus.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/handlers/admincommands/Stat.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Attrbonus);

// Java Attrbonus.java:14-22
Attrbonus::Attrbonus() : ConsoleCommand("attrbonus", "Modifies your stats.",
	"list - Lists all stats.\n"
	"<stat> - Shows active stat functions for the given stat.\n"
	"<stat> <value> - Sets the given stat to the given value.\n"
	"cancel - Cancels all active stat overrides.\n"
	"Stat parameters accept lowercase and abbreviated formats, such as flytime or flyt instead of FLY_TIME.\n") {
}

// Java Attrbonus.java:25-38
void Attrbonus::execute(Player& admin, std::span<const std::string> params) {
	admincommands::Stat* statCommand = ChatProcessor::getInstance().getCommand<admincommands::Stat>(); // parity= Stat statCommand = ChatProcessor.getInstance().getCommand(Stat.class);
	if (statCommand == nullptr) // parity: Java's NullPointerException of the dereferences below, explicit
		throw runtime::NullPointerException("ChatProcessor.getCommand(Stat.class)"); // parity: (the same)
	if (params.size() == 1 && commons::utils::StringUtils::equalsIgnoreCase("list", params[0])) { // parity= if (params.length == 1 && "list".equalsIgnoreCase(params[0])) {
		statCommand->listStats(admin);
	} else if (params.size() == 1 && commons::utils::StringUtils::equalsIgnoreCase("cancel", params[0])) { // parity= } else if (params.length == 1 && "cancel".equalsIgnoreCase(params[0])) {
		statCommand->cancelStatOverrides(admin, admin);
	} else if (params.size() == 1) {
		statCommand->showStatFunctions(admin, admin, params[0]);
	} else if (params.size() == 2) {
		statCommand->setStat(admin, admin, params[0], commons::utils::parseInt(params[1])); // parity= statCommand.setStat(admin, admin, params[0], Integer.parseInt(params[1]));
	} else {
		sendInfo(admin);
	}
}

} // namespace aion::gameserver::handlers::consolecommands
