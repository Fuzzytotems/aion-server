#include "aion/gameserver/network/loginserver/serverpackets/SM_BAN.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_BAN::SM_BAN(int8_t typeValue, int32_t accountIdValue, std::string_view ipValue, int32_t timeValue, int32_t adminObjIdValue)
	: LsServerPacket(0x06), type(typeValue), accountId(accountIdValue), ip(ipValue), time(timeValue), adminObjId(adminObjIdValue) {
}

void SM_BAN::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeC(buf, type);
	writeD(buf, accountId);
	writeS(buf, ip);
	writeD(buf, time);
	writeD(buf, adminObjId);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
