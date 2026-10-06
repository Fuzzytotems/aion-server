#include "aion/gameserver/handlers/admincommands/Enemy.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#include "aion/gameserver/controllers/PlayerController.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Enemy);

Enemy::Enemy()
	: AdminCommand("enemy", "Modifies your enmity towards others.",
		  "all [players|npcs] - Sets your enmity (default: you're everyone's enemy, optional: you're an enemy to any player, or any NPC).\n"
		  "none [players|npcs] - Disables your enmity (default: you're nobody's enemy, optional: you're not an enemy to any player, or any NPC).\n"
		  "cancel - Resets your enmity to the default.\n") {
}

/** Java String.equalsIgnoreCase (ASCII); a private static member: handler files have no anonymous namespace (unity builds) */
bool Enemy::equalsIgnoreCase(std::string_view a, std::string_view b) {
	return a.size() == b.size() &&
		   std::ranges::equal(a, b, [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
}

// Java Enemy.java:22-74
void Enemy::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}

	if (equalsIgnoreCase(params[0], "all")) {
		if (params.size() == 1) {
			player.unsetCustomState(CustomPlayerState::NEUTRAL_TO_EVERYONE);
			player.setCustomState(CustomPlayerState::ENEMY_OF_EVERYONE);
			sendInfo(player, "You are now an enemy to all.");
		} else if (equalsIgnoreCase(params[1], "npcs")) {
			player.unsetCustomState(CustomPlayerState::ENEMY_OF_EVERYONE);
			player.unsetCustomState(CustomPlayerState::NEUTRAL_TO_ALL_NPCS);
			player.setCustomState(CustomPlayerState::ENEMY_OF_ALL_NPCS);
			sendInfo(player, "You are now an enemy to all NPCs.");
		} else if (equalsIgnoreCase(params[1], "players")) {
			player.unsetCustomState(CustomPlayerState::ENEMY_OF_EVERYONE);
			player.unsetCustomState(CustomPlayerState::NEUTRAL_TO_ALL_PLAYERS);
			player.setCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
			sendInfo(player, "You are now an enemy to all players.");
		} else {
			sendInfo(player);
			return;
		}
	} else if (equalsIgnoreCase(params[0], "none")) {
		if (params.size() == 1) {
			player.unsetCustomState(CustomPlayerState::ENEMY_OF_EVERYONE);
			player.setCustomState(CustomPlayerState::NEUTRAL_TO_EVERYONE);
			sendInfo(player, "You are now neutral to everyone.");
		} else if (equalsIgnoreCase(params[1], "npcs")) {
			player.unsetCustomState(CustomPlayerState::NEUTRAL_TO_EVERYONE);
			player.unsetCustomState(CustomPlayerState::ENEMY_OF_ALL_NPCS);
			player.setCustomState(CustomPlayerState::NEUTRAL_TO_ALL_NPCS);
			sendInfo(player, "You are now neutral to all NPCs.");
		} else if (equalsIgnoreCase(params[1], "players")) {
			player.unsetCustomState(CustomPlayerState::NEUTRAL_TO_EVERYONE);
			player.unsetCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS);
			player.setCustomState(CustomPlayerState::NEUTRAL_TO_ALL_PLAYERS);
			sendInfo(player, "You are now neutral to all players.");
		} else {
			sendInfo(player);
			return;
		}
	} else if (equalsIgnoreCase(params[0], "cancel")) {
		player.unsetCustomState(CustomPlayerState::ENEMY_OF_EVERYONE);
		player.unsetCustomState(CustomPlayerState::NEUTRAL_TO_EVERYONE);
		sendInfo(player, "You appear regular to everyone again.");
	} else {
		sendInfo(player);
		return;
	}
	player.getController().onChangedPlayerAttributes();
}

} // namespace aion::gameserver::handlers::admincommands
