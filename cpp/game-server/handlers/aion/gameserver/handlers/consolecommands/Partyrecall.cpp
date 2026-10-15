#include "aion/gameserver/handlers/consolecommands/Partyrecall.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Partyrecall);

// Java Partyrecall.java:12-14
Partyrecall::Partyrecall() : ConsoleCommand("partyrecall") {
}

// Java Partyrecall.java:17-20
void Partyrecall::execute(Player& admin, std::span<const std::string> /*params*/) {
	PacketSendUtility::sendMessage(admin, "Command not implemented.");
	return;
}

} // namespace aion::gameserver::handlers::consolecommands
