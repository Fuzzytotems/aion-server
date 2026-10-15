#include "aion/gameserver/handlers/consolecommands/Leveldown.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Leveldown);

Leveldown::Leveldown()
	: ConsoleCommand("leveldown", "Levels a player down.",
		  "<value> - Levels your target down by the specified number of levels (defaults to your character, if no player is targeted).\n") {
}

// Java Leveldown.java:20-37
void Leveldown::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget());
	Player& player = target != nullptr ? *target : admin;
	int32_t newLevel = player.getLevel() - commons::utils::parseInt(params[0]);
	if (newLevel < 1 || newLevel > GSConfig::PLAYER_MAX_LEVEL.load()) { // parity= if (newLevel < 1 || newLevel > GSConfig.PLAYER_MAX_LEVEL) {
		sendInfo(admin, "Invalid level.");
		return;
	}
	player.getCommonData()->setLevel(newLevel);
	sendInfo(admin, "Set " + name(player) + "'s level to " + std::to_string(player.getLevel()));
}

} // namespace aion::gameserver::handlers::consolecommands
