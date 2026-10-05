#include "aion/gameserver/handlers/admincommands/See.h"

#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(See);

See::See() : AdminCommand("see", "Lets you see hidden NPCs and players.") {
}

// Java See.java:19-30
void See::execute(Player& admin, std::span<const std::string> /*params*/) {
	if (admin.getSeeState() < 2) {
		admin.setSeeState(CreatureSeeState::SEARCH20);
		sendInfo(admin, ChatUtil::l10n(288645)); // Can see targets in advanced hide states.
	} else {
		admin.unsetSeeState(CreatureSeeState::SEARCH20);
		sendInfo(admin, "You lost vision.");
	}
	PacketSendUtility::broadcastPacket(admin, SM_PLAYER_STATE(admin), true);
	admin.updateKnownlist();
}

} // namespace aion::gameserver::handlers::admincommands
