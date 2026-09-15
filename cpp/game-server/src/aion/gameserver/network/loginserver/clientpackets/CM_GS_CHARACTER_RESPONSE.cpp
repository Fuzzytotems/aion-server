#include "aion/gameserver/network/loginserver/clientpackets/CM_GS_CHARACTER_RESPONSE.h"

#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_GS_CHARACTER.h"

namespace aion::gameserver::network::loginserver::clientpackets {

CM_GS_CHARACTER_RESPONSE::CM_GS_CHARACTER_RESPONSE(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_GS_CHARACTER_RESPONSE::readImpl() {
	accountId = readD();
}

void CM_GS_CHARACTER_RESPONSE::runImpl() {
	int32_t characterCount = dao::PlayerDAO::getCharacterCountOnAccount(accountId);
	sendPacket(serverpackets::SM_GS_CHARACTER(accountId, characterCount));
}

} // namespace aion::gameserver::network::loginserver::clientpackets
