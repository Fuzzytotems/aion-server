#include "aion/gameserver/network/loginserver/serverpackets/SM_HDDBAN_CONTROL.h"

namespace aion::gameserver::network::loginserver::serverpackets {

namespace {

/** Java: BanAction.getId() (constructor data UNBAN(0), BAN(1); stands in for the BanAction companion of the ban services chunk) */
int32_t banActionId(services::ban::BanAction action) {
	return action == services::ban::BanAction::BAN ? 1 : 0;
}

} // namespace

SM_HDDBAN_CONTROL::SM_HDDBAN_CONTROL(services::ban::BanAction actionValue, std::string_view address, int64_t timeValue)
	: LsServerPacket(10), action(actionValue), serial(address), time(timeValue) {
}

void SM_HDDBAN_CONTROL::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeC(buf, banActionId(action));
	writeS(buf, serial);
	writeQ(buf, time);
}

} // namespace aion::gameserver::network::loginserver::serverpackets
