#include "aion/gameserver/network/loginserver/clientpackets/CM_MACBAN_LIST.h"

#include <string>

#include "aion/gameserver/network/BannedMacManager.h"

namespace aion::gameserver::network::loginserver::clientpackets {

CM_MACBAN_LIST::CM_MACBAN_LIST(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_MACBAN_LIST::readImpl() {
	BannedMacManager& bmm = BannedMacManager::getInstance();
	int32_t cnt = readD();
	for (int32_t a = 0; a < cnt; a++) {
		std::string address = readS(); // Java evaluates the arguments left to right
		int64_t time = readQ();
		std::string details = readS();
		bmm.dbLoad(address, time, details);
	}

	bmm.onEnd();
}

} // namespace aion::gameserver::network::loginserver::clientpackets
