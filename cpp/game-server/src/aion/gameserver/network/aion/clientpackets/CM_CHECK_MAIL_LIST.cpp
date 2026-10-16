#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_MAIL_LIST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/mail/MailService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_CHECK_MAIL_LIST::CM_CHECK_MAIL_LIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHECK_MAIL_LIST.java:22-24
void CM_CHECK_MAIL_LIST::readImpl() {
	expressOnly = readC() == 1;
}

// Java CM_CHECK_MAIL_LIST.java:27-31
void CM_CHECK_MAIL_LIST::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player)
		services::mail::MailService::sendMailList(*player, expressOnly, false);
}

AION_CLIENT_PACKET(CM_CHECK_MAIL_LIST);

} // namespace aion::gameserver::network::aion::clientpackets
