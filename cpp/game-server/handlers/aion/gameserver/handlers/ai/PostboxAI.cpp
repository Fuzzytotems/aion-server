#include "aion/gameserver/handlers/ai/PostboxAI.h"

#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"

namespace aion::gameserver::handlers::ai {

AION_AI(PostboxAI, "postbox");

// Java PostboxAI.java:22-26
void PostboxAI::handleDialogStart(Player& player) {
	player.getMailbox()->mailBoxState.set(PlayerMailboxState::REGULAR);
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::MAIL)));
}

// Java PostboxAI.java:28-30
void PostboxAI::handleDialogFinish(Player& player) {
	static_cast<void>(player);
}

} // namespace aion::gameserver::handlers::ai
