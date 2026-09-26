#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_AUTH.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_ACCOUNT_AUTH::SM_ACCOUNT_AUTH(int32_t accountIdValue, int32_t loginOkValue, int32_t playOk1Value, int32_t playOk2Value)
	: LsServerPacket(0x01), accountId(accountIdValue), loginOk(loginOkValue), playOk1(playOk1Value), playOk2(playOk2Value) {
}

void SM_ACCOUNT_AUTH::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, accountId);
	writeD(buf, loginOk);
	writeD(buf, playOk1);
	writeD(buf, playOk2);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
