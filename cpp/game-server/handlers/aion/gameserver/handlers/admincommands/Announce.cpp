#include "aion/gameserver/handlers/admincommands/Announce.h"

#include <optional>

#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Announce);

Announce::Announce()
	: AdminCommand("announce", "Sends a server-wide notice.",
		  "n <message> - Sends the message with your name.\n"
		  "a <message> - Sends the message anonymously.\n"
		  "ely <message> - Sends an anonymous message to all Elyos players.\n"
		  "asmo <message> - Sends an anonymous message to all Asmodian players.\n") {
}

// Java Announce.java:24-49
void Announce::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	if (params.size() <= 1) {
		sendInfo(admin);
		return;
	}
	std::string message;
	std::optional<Race> allowedRace; // Java: Race allowedRace = null
	if (equalsIgnoreCase("n", params[0])) {
		message = name(admin) + ": ";
	} else if (equalsIgnoreCase("a", params[0])) {
		message = "Announce: ";
	} else if (equalsIgnoreCase("ely", params[0])) {
		message = "Elyos: ";
		allowedRace = Race::ELYOS;
	} else if (equalsIgnoreCase("asmo", params[0])) {
		message = "Asmodians: ";
		allowedRace = Race::ASMODIANS;
	} else {
		sendInfo(admin);
		return;
	}
	message += join(params, 1);
	for (const runtime::Ptr<Player>& player : World::getInstance().getAllPlayers())
		if (!allowedRace || player->getRace() == *allowedRace || validateAccess(*player))
			PacketSendUtility::sendMessage(*player, message, ChatType::BRIGHT_YELLOW_CENTER);
}

} // namespace aion::gameserver::handlers::admincommands
