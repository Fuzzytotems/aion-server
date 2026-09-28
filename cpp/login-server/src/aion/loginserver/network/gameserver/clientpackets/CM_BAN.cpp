#include "aion/loginserver/network/gameserver/clientpackets/CM_BAN.h"

#include <chrono>
#include <optional>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/controller/BannedIpController.h"
#include "aion/loginserver/dao/AccountDAO.h"
#include "aion/loginserver/dao/AccountTimeDAO.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_BAN_RESPONSE.h"

namespace aion::loginserver::network::gameserver::clientpackets {

using commons::database::Timestamp;
using commons::utils::currentTimeMillis;

void CM_BAN::readImpl() {
	type = readC();
	accountId = readD();
	ip = readS();
	time = readD();
	adminObjId = readD();
}

void CM_BAN::runImpl() {
	bool result = false;

	// Ban account
	if ((type == 1 || type == 3) && accountId != 0) {
		std::shared_ptr<model::Account> account;

		// Find account on GameServers
		std::shared_ptr<GameServerInfo> gsi = GameServerTable::findLoggedInAccountGs(accountId);
		if (gsi)
			account = gsi->getAccountFromGameServer(accountId);
		// Deviation: Java only looks for the account on the game servers and otherwise changes the database only. If the client is logged in on the
		// login server, the kick below makes LoginConnection.onDisconnect store the connection's account time without the penalty, and an account
		// waiting for its fast reconnect would store it on its next logout; both lift the ban again. So the penalty is also set on those accounts.
		// Like in Java, an account that moves between these places while the packet runs can still be missed.
		if (!account)
			account = controller::AccountController::getAccountOnLS(accountId);
		if (!account)
			account = controller::AccountController::getReconnectingAccount(accountId);

		if (time >= 0) {
			// 1000 is 'infinity' value
			Timestamp newTime{std::chrono::milliseconds(time == 0 ? 1000 : currentTimeMillis() + (time * int64_t{60000}))};

			if (account) {
				result = account->modifyAndStoreAccountTime([&](model::AccountTime& t) { t.setPenaltyEnd(newTime); },
					[&](const model::AccountTime& accTime) { return dao::AccountTimeDAO::updateAccountTime(accountId, accTime); });
			} else {
				std::optional<model::AccountTime> accTime = dao::AccountTimeDAO::getAccountTime(accountId);
				if (!accTime)
					throw commons::utils::IllegalStateException("Account time of account " + std::to_string(accountId) + " is null");
				accTime->setPenaltyEnd(newTime);
				result = dao::AccountTimeDAO::updateAccountTime(accountId, *accTime);
			}
		}
	}

	// Ban IP
	if (type == 2 || type == 3) {
		if (accountId != 0) // If we got account ID, then ban last IP
		{
			// Deviation: empty for a NULL last_ip (see AccountDAO::getLastIp), so the given ip is banned and answered; Java throws a NullPointerException
			// (no ban, no kick, no SM_BAN_RESPONSE)
			std::string newip = dao::AccountDAO::getLastIp(accountId);
			if (!newip.empty())
				ip = newip;
		}
		if (!ip.empty()) {
			// Unban first. For banning it needs to update time
			if (controller::BannedIpController::isBanned(ip)) {
				// Result set for unban request
				result = controller::BannedIpController::unbanIp(ip);
			}
			if (time >= 0) // Ban
			{
				// Java: time * 60000 is an int multiplication, which wraps
				std::optional<Timestamp> newTime;
				if (time != 0)
					newTime = Timestamp(std::chrono::milliseconds(currentTimeMillis() + static_cast<int32_t>(static_cast<uint32_t>(time) * 60000u)));
				result = controller::BannedIpController::banIp(ip, newTime);
			}
		}
	}

	// Now kick account
	if (accountId != 0) {
		controller::AccountController::kickAccount(accountId);
	}

	// Respond to GS
	sendPacket(std::make_shared<serverpackets::SM_BAN_RESPONSE>(type, accountId, ip, time, adminObjId, result));
}

} // namespace aion::loginserver::network::gameserver::clientpackets
