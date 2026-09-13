#include "aion/loginserver/network/gameserver/clientpackets/CM_MACBAN_CONTROL.h"

#include "aion/loginserver/controller/BannedMacManager.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_MACBAN_CONTROL::readImpl() {
	type = readC();
	address = readS();
	details = readS();
	time = readQ();
}

void CM_MACBAN_CONTROL::runImpl() {
	controller::BannedMacManager& bmm = controller::BannedMacManager::getInstance();
	switch (type) {
		case 0: // unban
			bmm.unban(address, details);
			break;
		case 1: // ban
			bmm.ban(address, time, details);
			break;
	}
}

} // namespace aion::loginserver::network::gameserver::clientpackets
