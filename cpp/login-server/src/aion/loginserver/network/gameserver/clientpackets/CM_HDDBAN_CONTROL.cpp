#include "aion/loginserver/network/gameserver/clientpackets/CM_HDDBAN_CONTROL.h"

#include "aion/loginserver/controller/BannedHDDController.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_HDDBAN_CONTROL::readImpl() {
	type = readC();
	address = readS();
	time = readQ();
}

void CM_HDDBAN_CONTROL::runImpl() {
	switch (type) {
		case 0: // unban
			controller::BannedHDDController::getInstance().unban(address);
			break;
		case 1: // ban
			controller::BannedHDDController::getInstance().ban(address, time);
			break;
	}
}

} // namespace aion::loginserver::network::gameserver::clientpackets
