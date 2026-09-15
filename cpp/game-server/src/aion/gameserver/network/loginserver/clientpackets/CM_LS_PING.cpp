#include "aion/gameserver/network/loginserver/clientpackets/CM_LS_PING.h"

#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_LS_PONG.h"

namespace aion::gameserver::network::loginserver::clientpackets {

CM_LS_PING::CM_LS_PING(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_LS_PING::runImpl() {
	LoginServer::getInstance().sendPacket(serverpackets::SM_LS_PONG());
}

} // namespace aion::gameserver::network::loginserver::clientpackets
