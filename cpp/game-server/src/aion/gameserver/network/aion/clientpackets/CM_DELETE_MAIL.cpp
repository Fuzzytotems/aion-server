#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_MAIL.h"

#include <cstddef>

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/mail/MailService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_DELETE_MAIL::CM_DELETE_MAIL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_DELETE_MAIL.java:22-28
void CM_DELETE_MAIL::readImpl() {
	mailObjIds = std::vector<int32_t>(static_cast<size_t>(readUH()));
	for (size_t i = 0; i < mailObjIds.size(); i++) {
		mailObjIds[i] = readD();
		readC(); // unk
	}
}

// Java CM_DELETE_MAIL.java:31-34
void CM_DELETE_MAIL::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	services::mail::MailService::deleteMail(*player, mailObjIds);
}

AION_CLIENT_PACKET(CM_DELETE_MAIL);

} // namespace aion::gameserver::network::aion::clientpackets
