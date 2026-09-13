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
		writeH(buf, 0); // unk, always 0
		writeC(buf, 0); // age limit (?)
		writeC(buf, 0); // pvp=1 (?)
		writeH(buf, gsi->getCurrentPlayers());
		writeH(buf, gsi->getMaxPlayers());
		writeC(buf, gsi->isOnline() ? 1 : 0);
		writeC(buf, 1); // server type (1 = normal, 4 = test server)
		writeC(buf, 0); // hide server from list (beginner or panesterra server) ? 1 : 0
		writeH(buf, 0); // unk, always 0
		writeC(buf, 0); // server.brackets ? 1 : 0
	}
	writeH(buf, maxIdWithChars + 1);
	writeC(buf, 1); // enable "last server" button & auto-connection ? 1 : 0
	// Deviation: int loop counter (Java's byte counter overflows and never ends if maxIdWithChars is 127)
	for (int32_t gsId = 1; gsId <= maxIdWithChars; gsId++) {
		auto count = charCountOnServer.find(static_cast<int8_t>(gsId));
		writeC(buf, count != charCountOnServer.end() ? count->second : 0);
	}
	writeB(buf, std::array<uint8_t, 13>{}); // unk (44 77 90 A8 5D 75 C9 98 6D 20 53 2A 97)
}

} // namespace aion::loginserver::network::aion::serverpackets
