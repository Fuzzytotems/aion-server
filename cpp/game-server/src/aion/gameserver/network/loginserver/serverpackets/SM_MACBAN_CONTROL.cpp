#include "aion/gameserver/network/loginserver/serverpackets/SM_MACBAN_CONTROL.h"

namespace aion::gameserver::network::loginserver::serverpackets {

SM_MACBAN_CONTROL::SM_MACBAN_CONTROL(int8_t typeValue, std::string_view addressValue, int64_t timeValue, std::string_view detailsValue)
	: LsServerPacket(9), type(typeValue), address(addressValue), details(detailsValue), time(timeValue) {
}

void SM_MACBAN_CONTROL::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeC(buf, type);
	writeS(buf, address);
	writeS(buf, details);
	writeQ(buf, time);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
