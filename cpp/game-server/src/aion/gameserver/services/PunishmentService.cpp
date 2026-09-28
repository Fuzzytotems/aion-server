#include "aion/gameserver/services/PunishmentService.h"

#include <string>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/ban/ChatBanService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.PunishmentService@L109:46
//   com.aionemu.gameserver.services.PunishmentService@L121:90

void PunishmentService::unbanChar(int32_t playerId) {
	AION_UNPORTED();
}

void PunishmentService::banChar(int32_t playerId, int32_t dayCount, std::string_view reason) {
	AION_UNPORTED();
}

int64_t PunishmentService::calculateDuration(int32_t dayCount) {
	AION_UNPORTED();
}

void PunishmentService::setIsInPrison(model::gameobjects::player::Player& player, bool state, int64_t delayInMinutes, std::string_view reason) {
	AION_UNPORTED();
}

void PunishmentService::updatePrisonStatus(model::gameobjects::player::Player& player) {
	int32_t prisonDurationSeconds = player.getPrisonDurationSeconds();
	if (prisonDurationSeconds > 0) {
		schedulePrisonTask(player, static_cast<int64_t>(prisonDurationSeconds) * 1000);
		int32_t remainingMinutes = prisonDurationSeconds / 60;
		if (remainingMinutes <= 0)
			remainingMinutes = 1;

		ban::ChatBanService::banPlayer(player, remainingMinutes); // Java passes the minutes as durationMillis
		utils::PacketSendUtility::sendMessage(player,
			"You are still in prison for " + std::to_string(remainingMinutes) + " minute" + (remainingMinutes > 1 ? "s" : "") + ".");

		if (player.getWorldId() != world::getId(world::WorldMapType::DF_PRISON) && player.getWorldId() != world::getId(world::WorldMapType::LF_PRISON)) {
			utils::PacketSendUtility::sendMessage(player, "You will be teleported to prison in a moment!");
			utils::ThreadPoolManager::getInstance().schedule({&player}, [&player] { teleport::TeleportService::teleportToPrison(player); }, 10000);
		}
	}
}

void PunishmentService::schedulePrisonTask(model::gameobjects::player::Player& player, int64_t prisonTimer) {
	player.getController().addTask(model::TaskId::PRISON,
		utils::ThreadPoolManager::getInstance().schedule({&player}, [&player] { setIsInPrison(player, false, 0, ""); }, prisonTimer));
}

void PunishmentService::setIsNotGatherable(model::gameobjects::player::Player& player, int32_t captchaCount, bool state, int64_t delay) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
