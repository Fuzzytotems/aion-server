#include "aion/gameserver/handlers/consolecommands/Teleportto.h"

#include "aion/gameserver/handlers/admincommands/GoTo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Teleportto);

// Java Teleportto.java:17-19
Teleportto::Teleportto() : ConsoleCommand("teleportto", "Teleports you to regions by name.") {
}

// Java Teleportto.java:21-25
std::string Teleportto::getSyntaxInfo() {
	admincommands::GoTo* goTo = ChatProcessor::getInstance().getCommand<admincommands::GoTo>(); // parity: (getCommand(GoTo.class), below)
	if (goTo == nullptr) // parity: Java NPE of the dereference, explicit
		throw runtime::NullPointerException("ChatProcessor.getCommand(GoTo.class)"); // parity: (the same)
	std::string info = goTo->getSyntaxInfo(); // parity= GoTo goTo = ChatProcessor.getInstance().getCommand(GoTo.class);
	const std::string target = goTo->getAliasWithPrefix(); // parity= return goTo.getSyntaxInfo().replace(goTo.getAliasWithPrefix(), getAliasWithPrefix());
	const std::string replacement = getAliasWithPrefix(); // parity: (String.replace, above)
	// Java: String.replace(target, replacement) - every occurrence, left to right
	for (size_t at = info.find(target); at != std::string::npos; at = info.find(target, at + replacement.size())) // parity: (the same)
		info.replace(at, target.size(), replacement); // parity: (the same)
	return info; // parity: (the same)
}

// Java Teleportto.java:27-30
void Teleportto::execute(Player& admin, std::span<const std::string> params) {
	admincommands::GoTo* goTo = ChatProcessor::getInstance().getCommand<admincommands::GoTo>(); // parity: (getCommand(GoTo.class), below)
	if (goTo == nullptr) // parity: Java NPE of the dereference, explicit
		throw runtime::NullPointerException("ChatProcessor.getCommand(GoTo.class)"); // parity: (the same)
	goTo->execute(admin, params); // parity= ChatProcessor.getInstance().getCommand(GoTo.class).execute(admin, params);
}

} // namespace aion::gameserver::handlers::consolecommands
