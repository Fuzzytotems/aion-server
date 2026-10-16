#include "aion/gameserver/handlers/ai/portals/OccupiedRentusBasePortalAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(OccupiedRentusBasePortalAI, "occupy_rentus_portal");

// Java OccupiedRentusBasePortalAI.java:20-37
void OccupiedRentusBasePortalAI::checkDialog(Player& player) {
	if (getOwner().getNpcId() == 832991) {
		if (player.getRace() == Race::ASMODIANS) {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
		} else {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
		}
	} else if (getOwner().getNpcId() == 832992) {
		if (player.getRace() == Race::ASMODIANS) {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
		} else {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
		}
	} else {
		PortalDialogAI::checkDialog(player);
	}
}

} // namespace aion::gameserver::handlers::ai::portals
