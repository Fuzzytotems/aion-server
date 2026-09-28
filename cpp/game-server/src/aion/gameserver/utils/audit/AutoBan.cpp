#include "aion/gameserver/utils/audit/AutoBan.h"

#include <memory>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/BannedMacManager.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::utils::audit {

void AutoBan::punishment(model::gameobjects::player::Player& player) {
	std::string reason = "You have been punished due to illegal actions";
	std::string accountIp = player.getClientConnection()->getIP();
	int32_t accountId = player.getClientConnection()->getAccount()->getId();
	int32_t time = configs::main::PunishmentConfig::PUNISHMENT_TIME.load();
	int32_t minInDay = 1440;
	int32_t dayCount = time / minInDay; // Java: (int) (Math.floor(time / minInDay)) of an int division
	switch (configs::main::PunishmentConfig::PUNISHMENT_TYPE.load()) {
		case 1:
			player.getClientConnection()->close(network::aion::serverpackets::SM_QUIT_RESPONSE());
			break;
		case 2:
			services::PunishmentService::banChar(player.getObjectId(), dayCount, reason);
			break;
		case 3:
			network::loginserver::LoginServer::getInstance().sendBanPacket(1, accountId, accountIp, time, 0);
			break;
		case 4:
			network::loginserver::LoginServer::getInstance().sendBanPacket(2, accountId, accountIp, time, 0);
			break;
		case 5: {
			player.getClientConnection()->close();
			// Java: System.currentTimeMillis() + time * 60000 with an int multiplication that wraps before the long addition
			int32_t banMillis = static_cast<int32_t>(static_cast<uint32_t>(time) * 60000u);
			network::BannedMacManager::getInstance().banAddress(player.getClientConnection()->getMacAddress(),
				commons::utils::currentTimeMillis() + banMillis, reason);
			break;
		}
	}
}

} // namespace aion::gameserver::utils::audit
