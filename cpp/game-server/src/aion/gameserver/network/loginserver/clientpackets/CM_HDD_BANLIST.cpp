#include "aion/gameserver/network/loginserver/clientpackets/CM_HDD_BANLIST.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/services/ban/HDDBanService.h"

namespace aion::gameserver::network::loginserver::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.clientpackets.CM_HDD_BANLIST");

CM_HDD_BANLIST::CM_HDD_BANLIST(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_HDD_BANLIST::readImpl() {
	count = readD();
	for (int32_t a = 0; a < count; a++) {
		std::string serial = readS(); // Java evaluates the arguments left to right
		int64_t banTime = readQ();
		services::ban::HDDBanService::getInstance().loadBan(serial, banTime);
	}
}

void CM_HDD_BANLIST::runImpl() {
	log.info("Loaded " + std::to_string(count) + " HDD ban entries.");
}

} // namespace aion::gameserver::network::loginserver::clientpackets
