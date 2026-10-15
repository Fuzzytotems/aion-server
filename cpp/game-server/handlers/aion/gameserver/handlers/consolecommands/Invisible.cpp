#include "aion/gameserver/handlers/consolecommands/Invisible.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::consolecommands {

AION_CONSOLE_COMMAND(Invisible);

// Java Invisible.java:19-21
Invisible::Invisible() : ConsoleCommand("invisible", "Sets advanced invisibility.") {
}

// Java Invisible.java:24-34
void Invisible::execute(Player& player, std::span<const std::string> /*params*/) {
	if (!player.isInVisualState(CreatureVisualState::HIDE20)) {
		player.getEffectController()->setAbnormal(AbnormalState::HIDE);
		player.setVisualState(CreatureVisualState::HIDE20);
		player.getController().onHide();
		PacketSendUtility::broadcastPacket(player, SM_PLAYER_STATE(player), true); // parity= PacketSendUtility.broadcastPacket(player, new SM_PLAYER_STATE(player), true);
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_BEGIN());
	// required because without a skill this isn't sent automatically (outdated abnormals can cause issues when opening a private store for example)
	PacketSendUtility::sendPacket(player, SM_ABNORMAL_STATE({}, player.getEffectController()->getAbnormals(), 0)); // parity= PacketSendUtility.sendPacket(player, new SM_ABNORMAL_STATE(Collections.emptyList(), player.getEffectController().getAbnormals(), 0));
}

} // namespace aion::gameserver::handlers::consolecommands
