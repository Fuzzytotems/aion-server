#include "aion/gameserver/network/loginserver/serverpackets/SM_GS_CHARACTER.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_GS_CHARACTER::SM_GS_CHARACTER(int32_t accountIdValue, int32_t characterCountValue)
	: LsServerPacket(0x08), accountId(accountIdValue), characterCount(characterCountValue) {
}

void SM_GS_CHARACTER::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, accountId);
	writeC(buf, characterCount);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
