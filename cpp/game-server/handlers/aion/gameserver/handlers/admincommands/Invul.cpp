#include "aion/gameserver/handlers/admincommands/Invul.h"

#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Invul);

Invul::Invul() : AdminCommand("invul", "Enables/disables invulnerability.") {
}

// Java Invul.java:18-27
void Invul::execute(Player& player, std::span<const std::string> /*params*/) {
	if (player.isInvulnerable()) {
		player.unsetCustomState(CustomPlayerState::INVULNERABLE);
		PacketSendUtility::sendMessage(player, "You are now mortal.");
	} else {
		player.setCustomState(CustomPlayerState::INVULNERABLE);
		sendInfo(player, ChatUtil::l10n(293440)); // Immune to all damage.
	}
}

} // namespace aion::gameserver::handlers::admincommands
