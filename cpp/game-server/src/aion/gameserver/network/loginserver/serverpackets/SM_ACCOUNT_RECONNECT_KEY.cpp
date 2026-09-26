#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_RECONNECT_KEY.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_ACCOUNT_RECONNECT_KEY::SM_ACCOUNT_RECONNECT_KEY(int32_t accountIdValue) : LsServerPacket(0x02), accountId(accountIdValue) {
}

void SM_ACCOUNT_RECONNECT_KEY::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, accountId);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
