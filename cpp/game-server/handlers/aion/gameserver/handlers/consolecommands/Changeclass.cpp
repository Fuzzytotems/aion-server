#include "aion/gameserver/handlers/consolecommands/Changeclass.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/services/ClassChangeService.h"
#include "aion/gameserver/utils/EnumValueOf.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Changeclass);

// Java Changeclass.java:13-17
Changeclass::Changeclass()
	: ConsoleCommand("changeclass", "Changes a player's class.",
		  "<class> - Changes your target's class to the one specified (defaults to your character, if no player is targeted).\n") {
}

// Java Changeclass.java:19-29
void Changeclass::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		sendInfo(admin);
		return;
	}
	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget()); // parity= Player player = admin.getTarget() instanceof Player target ? target : admin;
	Player& player = target != nullptr ? *target : admin; // parity: (the same)
	PlayerClass playerClass = parsePlayerClass(params[0]);
	ClassChangeService::setClass(player, playerClass, false, true);
	sendInfo(admin, "You have changed " + player.getName() + "'s class to " + // parity= sendInfo(admin, "You have changed " + player.getName() + "'s class to " + playerClass.toString().toLowerCase() +  ".");
		commons::utils::StringUtils::toLowerCase(xml::enumName(playerClass)) + "."); // parity: (the same)
}

// Java Changeclass.java:31-39
PlayerClass Changeclass::parsePlayerClass(const std::string& param) {
	std::string newClass = commons::utils::StringUtils::toUpperCase(param); // parity= return switch (param.toUpperCase()) {
	if (newClass == "FIGHTER") // parity= case "FIGHTER" -> PlayerClass.GLADIATOR;
		return PlayerClass::GLADIATOR; // parity: (the same)
	if (newClass == "KNIGHT") // parity= case "KNIGHT" -> PlayerClass.TEMPLAR;
		return PlayerClass::TEMPLAR; // parity: (the same)
	if (newClass == "WIZARD") // parity= case "WIZARD" -> PlayerClass.SORCERER;
		return PlayerClass::SORCERER; // parity: (the same)
	if (newClass == "ELEMENTALIST") // parity= case "ELEMENTALIST" -> PlayerClass.SPIRIT_MASTER;
		return PlayerClass::SPIRIT_MASTER; // parity: (the same)
	return utils::enumValueOf<PlayerClass>(newClass); // parity= case String newClass -> PlayerClass.valueOf(newClass);
}

} // namespace aion::gameserver::handlers::consolecommands
