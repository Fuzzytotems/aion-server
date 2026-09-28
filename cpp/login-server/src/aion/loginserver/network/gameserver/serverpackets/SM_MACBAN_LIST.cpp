#include "aion/loginserver/network/gameserver/serverpackets/SM_MACBAN_LIST.h"

#include <string>
#include <unordered_map>

#include "aion/loginserver/controller/BannedMacManager.h"

namespace aion::loginserver::network::gameserver::serverpackets {

SM_MACBAN_LIST::SM_MACBAN_LIST() {
	controller::BannedMacManager::getInstance();
}

void SM_MACBAN_LIST::writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const {
	std::unordered_map<std::string, model::base::BannedMacEntry> bannedList = controller::BannedMacManager::getInstance().getMap();
	writeC(buf, 9);
	writeD(buf, static_cast<int32_t>(bannedList.size()));

	for (const auto& [address, entry] : bannedList) {
		writeS(buf, entry.getMac());
		writeQ(buf, entry.getTime().value().time_since_epoch().count());
		writeS(buf, entry.getDetails());
	}
}

} // namespace aion::loginserver::network::gameserver::serverpackets
