#include "aion/gameserver/network/aion/clientpackets/CM_SEND_MAIL.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/LetterTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/mail/MailService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_SEND_MAIL::CM_SEND_MAIL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SEND_MAIL.java:29-37
void CM_SEND_MAIL::readImpl() {
	recipientName = readS();
	title = readS();
	message = readS();
	itemObjId = readD();
	itemCount = readQ();
	kinahCount = readQ();
	idLetterType = readUC();
}

// Java CM_SEND_MAIL.java:40-43
void CM_SEND_MAIL::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	// Java evaluates the arguments before MailService.sendMail runs: an unknown letter type throws IllegalArgumentException first
	const model::gameobjects::LetterType letterType = model::gameobjects::getLetterTypeById(idLetterType);
	services::mail::MailService::sendMail(*player, recipientName, title, message, itemObjId, itemCount, kinahCount, letterType);
}

AION_CLIENT_PACKET(CM_SEND_MAIL);

} // namespace aion::gameserver::network::aion::clientpackets
