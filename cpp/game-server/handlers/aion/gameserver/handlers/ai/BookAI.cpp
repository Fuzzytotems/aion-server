#include "aion/gameserver/handlers/ai/BookAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"


namespace aion::gameserver::handlers::ai {

AION_AI(BookAI, "book");

// Java BookAI.java:21-24
void BookAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai
