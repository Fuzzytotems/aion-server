#include "aion/gameserver/network/loginserver/serverpackets/SM_CHANGE_ALLOWED_HDD_SERIAL.h"

#include "aion/gameserver/model/account/Account.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_CHANGE_ALLOWED_HDD_SERIAL::SM_CHANGE_ALLOWED_HDD_SERIAL(model::account::Account& playerAccount)
	: LsServerPacket(11), accountId(playerAccount.getId()), hddSerial(playerAccount.getAllowedHddSerial().value_or("")) {
}

void SM_CHANGE_ALLOWED_HDD_SERIAL::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, accountId);
	writeS(buf, hddSerial); // Java null writes the same bytes as ""
}

} // namespace aion::gameserver::network::loginserver::serverpackets
