#include "aion/gameserver/handlers/admincommands/Invis.h"

#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Invis);

Invis::Invis() : AdminCommand("invis", "Sets/unsets advanced invisibility.") {
}

// Java Invis.java:23-38
void Invis::execute(Player& player, std::span<const std::string> /*params*/) {
	if (!player.isInVisualState(CreatureVisualState::HIDE20)) {
		player.getEffectController()->setAbnormal(AbnormalState::HIDE);
		player.setVisualState(CreatureVisualState::HIDE20);
		player.getController().onHide();
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_BEGIN());
	} else {
		player.getEffectController()->unsetAbnormal(AbnormalState::HIDE);
		player.unsetVisualState(CreatureVisualState::HIDE20);
		player.getController().onHideEnd();
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_EFFECT_INVISIBLE_END());
	}
	PacketSendUtility::broadcastPacket(player, SM_PLAYER_STATE(player), true);
	// required because without a skill this isn't sent automatically (outdated abnormals can cause issues when opening a private store for example)
	PacketSendUtility::sendPacket(player, SM_ABNORMAL_STATE({}, player.getEffectController()->getAbnormals(), 0)); // parity= PacketSendUtility.sendPacket(player, new SM_ABNORMAL_STATE(Collections.emptyList(), player.getEffectController().getAbnormals(), 0));
}

} // namespace aion::gameserver::handlers::admincommands
