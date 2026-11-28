#include "aion/loginserver/network/aion/serverpackets/SM_SERVER_LIST.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/aion/LoginConnection.h"

namespace aion::loginserver::network::aion::serverpackets {

void SM_SERVER_LIST::writeImpl(LoginConnection& con, commons::utils::ByteBuffer& buf) const {
	std::shared_ptr<model::Account> account = con.getAccount();
	if (!account)
		throw commons::utils::IllegalStateException("SM_SERVER_LIST for " + con.toString() + " without account");
	std::vector<std::shared_ptr<GameServerInfo>> servers = GameServerTable::getGameServers();
	// Deviation: Java gets the live map (null if missing, which throws a NullPointerException), here a copy (empty if missing)
	std::map<int8_t, int32_t> charCountOnServer = controller::AccountController::getGSCharacterCountsFor(account->getId().value()).value_or(std::map<int8_t, int32_t>{});
	int32_t maxIdWithChars = 0;

	writeC(buf, static_cast<int32_t>(servers.size())); // loop size
	writeC(buf, account->getLastServer());
	for (const std::shared_ptr<GameServerInfo>& gsi : servers) {
		int8_t gsId = gsi->getId();
		if (gsId > maxIdWithChars && charCountOnServer.contains(gsId))
			maxIdWithChars = gsi->getId();
		writeC(buf, gsId);
		writeB(buf, gsi->getIp());
		writeH(buf, gsi->getPort());
		writeH(buf, 0); // the client reads it with the port as one int and uses only the port
		writeC(buf, 0); // age limit in the Lineage 2 protocol this one derives from; the 4.8 client copies it and never reads it
		writeC(buf, 0); // PvP flag in the Lineage 2 protocol; the 4.8 client copies it and never reads it
		writeH(buf, gsi->getCurrentPlayers());
		writeH(buf, gsi->getMaxPlayers());
		writeC(buf, gsi->isOnline() ? 1 : 0);
		// type mask: the client lists the server if bit 0 is set (bit 2 for a client in test server mode);
		// bit 8 keeps it out of the "Recommended server is ..." prompt that a player without characters gets
		writeD(buf, 1);
		writeC(buf, 0); // country code: with country code 5 in the client's cc.ini, servers of other countries are hidden; 0 shows it to all
	}
	writeH(buf, maxIdWithChars + 1); // size of the following block
	writeC(buf, 1); // 1: the number of characters per server follows
	// Deviation: int loop counter (Java's byte counter overflows and never ends if maxIdWithChars is 127)
	for (int32_t gsId = 1; gsId <= maxIdWithChars; gsId++) {
		auto count = charCountOnServer.find(static_cast<int8_t>(gsId));
		writeC(buf, count != charCountOnServer.end() ? count->second : 0);
	}
}

} // namespace aion::loginserver::network::aion::serverpackets
