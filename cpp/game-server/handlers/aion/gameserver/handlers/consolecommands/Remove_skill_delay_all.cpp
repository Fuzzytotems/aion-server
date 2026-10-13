#include "aion/gameserver/handlers/consolecommands/Remove_skill_delay_all.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Remove_skill_delay_all);

// Java Remove_skill_delay_all.java:12-17
Remove_skill_delay_all::Remove_skill_delay_all() : ConsoleCommand("remove_skill_delay_all", "Enables/disables your skill cooldowns.",
	"1 - Enables your skill cooldowns.\n"
	"0 - Disables your skill cooldowns.\n") {
}

// Java Remove_skill_delay_all.java:20-32
void Remove_skill_delay_all::execute(Player& player, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(player);
		return;
	}
	if (params[0] == "1" && player.isInCustomState(CustomPlayerState::NO_SKILL_COOLDOWN_MODE)) { // parity= if (params[0].equals("1") && player.isInCustomState(CustomPlayerState.NO_SKILL_COOLDOWN_MODE)) {
		sendInfo(player, "Cooldown times of all skills have been recovered.");
		player.unsetCustomState(CustomPlayerState::NO_SKILL_COOLDOWN_MODE);
	} else if (params[0] == "0" && !player.isInCustomState(CustomPlayerState::NO_SKILL_COOLDOWN_MODE)) { // parity= } else if (params[0].equals("0") && !player.isInCustomState(CustomPlayerState.NO_SKILL_COOLDOWN_MODE)) {
		sendInfo(player, "Cooldown times of all skills have been disabled.");
		player.setCustomState(CustomPlayerState::NO_SKILL_COOLDOWN_MODE);
	}
}

} // namespace aion::gameserver::handlers::consolecommands
