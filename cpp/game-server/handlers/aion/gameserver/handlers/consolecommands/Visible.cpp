#include "aion/gameserver/handlers/consolecommands/Visible.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Visible);

// Java Visible.java:19-21
Visible::Visible() : ConsoleCommand("visible", "Unsets advanced invisibility.") {
}

// Java Visible.java:24-34
void Visible::execute(Player& player, std::span<const std::string> /*params*/) {
	if (player.isInVisualState(CreatureVisualState::HIDE20)) {
		player.getEffectController()->unsetAbnormal(AbnormalState::HIDE);
		player.unsetVisualState(CreatureVisualState::HIDE20);
		player.getController().onHideEnd();
		PacketSendUtility::broadcastPacket(player, SM_PLAYER_STATE(player), true); // parity= PacketSendUtility.broadcastPacket(player, new SM_PLAYER_STATE(player), true);
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_END());
	// required because without a skill this isn't sent automatically (outdated abnormals can cause issues when opening a private store for example)
	PacketSendUtility::sendPacket(player, SM_ABNORMAL_STATE({}, player.getEffectController()->getAbnormals(), 0)); // parity= PacketSendUtility.sendPacket(player, new SM_ABNORMAL_STATE(Collections.emptyList(), player.getEffectController().getAbnormals(), 0));
}

} // namespace aion::gameserver::handlers::consolecommands
