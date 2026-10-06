#include "aion/gameserver/handlers/consolecommands/Levelup.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Levelup);

Levelup::Levelup()
	: ConsoleCommand("levelup", "Levels a player up.",
		  "<value> - Levels your target up by the specified number of levels (defaults to your character, if no player is targeted).\n") {
}

// Java Levelup.java:20-37
void Levelup::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		sendInfo(admin);
		return;
	}

	runtime::Ptr<Player> target = runtime::as<Player>(admin.getTarget());
	Player& player = target != nullptr ? *target : admin;
	int32_t newLevel = player.getLevel() + commons::utils::parseInt(params[0]);
	if (newLevel < 1 || newLevel > GSConfig::PLAYER_MAX_LEVEL.load()) {
		sendInfo(admin, "Invalid level.");
		return;
	}
	player.getCommonData()->setLevel(newLevel);
	sendInfo(admin, "Set " + name(player) + "'s level to " + std::to_string(player.getLevel()));
}

} // namespace aion::gameserver::handlers::consolecommands
