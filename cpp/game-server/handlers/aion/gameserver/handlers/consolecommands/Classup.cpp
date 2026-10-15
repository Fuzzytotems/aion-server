#include "aion/gameserver/handlers/consolecommands/Classup.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/handlers/consolecommands/Changeclass.h"
#include "aion/gameserver/services/ClassChangeService.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Classup);

// Java Classup.java:13-17
Classup::Classup()
	: ConsoleCommand("classup", "Promotes a player's class.",
		  "<class> - Promotes your target's class to the one specified (defaults to your character, if no player is targeted).\n") {
}

// Java Classup.java:19-29
void Classup::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		sendInfo(admin);
		return;
	}
	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)
	PlayerClass playerClass = Changeclass::parsePlayerClass(params[0]);
	ClassChangeService::setClass(player, playerClass, false, true);
	sendInfo(admin, "You have promoted " + player.getName() + "'s class to " + // parity= sendInfo(admin, "You have promoted " + player.getName() + "'s class to " + playerClass.toString().toLowerCase() + ".");
		commons::utils::StringUtils::toLowerCase(xml::enumName(playerClass)) + "."); // parity: (the same)
}

} // namespace aion::gameserver::handlers::consolecommands
