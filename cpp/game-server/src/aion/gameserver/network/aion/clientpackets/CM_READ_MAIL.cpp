#include "aion/gameserver/network/aion/clientpackets/CM_READ_MAIL.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/mail/MailService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_READ_MAIL::CM_READ_MAIL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_READ_MAIL.java:22-24
void CM_READ_MAIL::readImpl() {
	mailObjId = readD();
}

// Java CM_READ_MAIL.java:27-30
void CM_READ_MAIL::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	services::mail::MailService::readMail(*player, mailObjId);
}

AION_CLIENT_PACKET(CM_READ_MAIL);

} // namespace aion::gameserver::network::aion::clientpackets
