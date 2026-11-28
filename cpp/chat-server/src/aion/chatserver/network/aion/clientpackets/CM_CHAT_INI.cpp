#include "aion/chatserver/network/aion/clientpackets/CM_CHAT_INI.h"

#include "aion/chatserver/network/aion/serverpackets/SM_CHAT_INI.h"
#include "aion/chatserver/network/aion/AionConnection.h"

namespace aion::chatserver::network::aion::clientpackets {

void CM_CHAT_INI::readImpl() {
	readC();
	readH();
	readD();
	readD();
	readD();
}

void CM_CHAT_INI::runImpl() {
	getConnection()->sendPacket(serverpackets::SM_CHAT_INI());
}

} // namespace aion::chatserver::network::aion::clientpackets
