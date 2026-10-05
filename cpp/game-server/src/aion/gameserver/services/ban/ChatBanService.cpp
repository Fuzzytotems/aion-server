#include "aion/gameserver/services/ban/ChatBanService.h"

#include <algorithm>
#include <cmath>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services::ban {

// Java ChatBanService.java:26-30
void ChatBanService::banPlayer(model::gameobjects::player::Player& player, int64_t durationMillis) {
	network::chatserver::ChatServer::getInstance().sendPlayerGagPacket(player.getObjectId(), durationMillis);
	chatBans.put(player.getObjectId(), commons::utils::currentTimeMillis() + durationMillis);
	registerUnban(player, durationMillis);
}

// Java ChatBanService.java:32-37
void ChatBanService::unbanPlayer(model::gameobjects::player::Player& player) {
	player.getController().cancelTask(model::TaskId::GAG);
	network::chatserver::ChatServer::getInstance().sendPlayerGagPacket(player.getObjectId(), 0);
	if (chatBans.remove(player.getObjectId()).has_value() && player.isOnline())
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CAN_CHAT_NOW());
}

// Java ChatBanService.java:39-47. The anonymous Runnable (fieldmap ChatBanService$1, capturing the player) is a lambda stored as the player's
// GAG task, as Java stores its Future: it pins the player until it has run or unbanPlayer / cancelAllTasks cancelled it.
void ChatBanService::registerUnban(model::gameobjects::player::Player& player, int64_t delay) {
	player.getController().addTask(model::TaskId::GAG,
		utils::ThreadPoolManager::getInstance().schedule({&player}, [&player] { unbanPlayer(player); }, delay));
}

// Java ChatBanService.java:49-51
bool ChatBanService::isBanned(model::gameobjects::player::Player& player) {
	return getBanMinutes(player) > 0;
}

// Java ChatBanService.java:60-75
int32_t ChatBanService::getBanMinutes(model::gameobjects::player::Player& player) {
	std::optional<int64_t> expireTime = chatBans.get(player.getObjectId());
	if (!expireTime)
		return 0;

	int64_t millisLeft = *expireTime - commons::utils::currentTimeMillis();
	if (millisLeft <= 0) {
		unbanPlayer(player);
		return 0;
	}

	if (!player.getController().hasTask(model::TaskId::GAG))
		registerUnban(player, millisLeft);

	// Java: (int) Math.max(0, Math.ceil(millisLeft / 60000f)) - a float division, a double ceil, an int cast
	return static_cast<int32_t>(std::max(0.0, std::ceil(static_cast<double>(static_cast<float>(millisLeft) / 60000.0f))));
}

} // namespace aion::gameserver::services::ban
