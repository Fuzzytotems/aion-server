#include "aion/gameserver/network/loginserver/serverpackets/SM_ACCOUNT_CONNECTION_INFO.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_ACCOUNT_CONNECTION_INFO::SM_ACCOUNT_CONNECTION_INFO(int32_t accountIdValue, int64_t timeValue, std::string_view ipValue, std::string_view macValue,
	std::string_view hddSerialValue)
	: LsServerPacket(7), accountId(accountIdValue), time(timeValue), ip(ipValue), mac(macValue), hddSerial(hddSerialValue) {
}

void SM_ACCOUNT_CONNECTION_INFO::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeD(buf, accountId);
	writeQ(buf, time);
	writeS(buf, ip);
	writeS(buf, mac);
	writeS(buf, hddSerial);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
