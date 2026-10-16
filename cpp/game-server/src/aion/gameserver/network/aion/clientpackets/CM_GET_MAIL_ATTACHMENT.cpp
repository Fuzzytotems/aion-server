#include "aion/gameserver/network/aion/clientpackets/CM_GET_MAIL_ATTACHMENT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/mail/MailService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_GET_MAIL_ATTACHMENT::CM_GET_MAIL_ATTACHMENT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_GET_MAIL_ATTACHMENT.java:23-26
void CM_GET_MAIL_ATTACHMENT::readImpl() {
	mailObjId = readD();
	attachmentType = readC(); // 0 - item , 1 - kinah
}

// Java CM_GET_MAIL_ATTACHMENT.java:29-32
void CM_GET_MAIL_ATTACHMENT::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	services::mail::MailService::getAttachments(*player, mailObjId, attachmentType);
}

AION_CLIENT_PACKET(CM_GET_MAIL_ATTACHMENT);

} // namespace aion::gameserver::network::aion::clientpackets
