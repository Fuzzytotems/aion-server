#include "aion/loginserver/network/gameserver/serverpackets/SM_HDDBAN_LIST.h"

#include <string>
#include <unordered_map>

#include "aion/loginserver/controller/BannedHDDController.h"

namespace aion::loginserver::network::gameserver::serverpackets {

SM_HDDBAN_LIST::SM_HDDBAN_LIST() {
	controller::BannedHDDController::getInstance();
}

void SM_HDDBAN_LIST::writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const {
	std::unordered_map<std::string, commons::database::Timestamp> bannedList = controller::BannedHDDController::getInstance().getMap();
	writeC(buf, 10);
	writeD(buf, static_cast<int32_t>(bannedList.size()));

	for (const auto& [serial, time] : bannedList) {
		writeS(buf, serial);
		writeQ(buf, time.time_since_epoch().count());
	}
}

} // namespace aion::loginserver::network::gameserver::serverpackets
