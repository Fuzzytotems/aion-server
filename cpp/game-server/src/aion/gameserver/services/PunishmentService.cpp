#include "aion/gameserver/services/PunishmentService.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CAPTCHA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUIT_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/world/World.h"

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

namespace {
// Java: the logger of com.aionemu.commons.network.Dispatcher, which logs an exception of a packet's writeImpl (Dispatcher.java:81-91)
const auto dispatcherLog = commons::logging::LoggerFactory::getLogger("com.aionemu.commons.network.Dispatcher");
} // namespace

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.PunishmentService@L109:46
//   com.aionemu.gameserver.services.PunishmentService@L121:90

// Java PunishmentService.java:30-32
void PunishmentService::unbanChar(int32_t playerId) {
	dao::PlayerPunishmentsDAO::unpunishPlayer(playerId, PunishmentType::CHARBAN);
}

// Java PunishmentService.java:41-48
void PunishmentService::banChar(int32_t playerId, int32_t dayCount, std::string_view reason) {
	dao::PlayerPunishmentsDAO::punishPlayer(playerId, PunishmentType::CHARBAN, calculateDuration(dayCount), reason);

	// if player is online - kick him
	runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(playerId);
	if (player != nullptr) {
		std::shared_ptr<network::aion::AionConnection> connection = player->getClientConnection();
		if (connection == nullptr) // Java: player.getClientConnection().close(...) on null
			throw runtime::NullPointerException("Player.getClientConnection()");
		connection->close(network::aion::serverpackets::SM_QUIT_RESPONSE());
	}
}

// Java PunishmentService.java:56-60
int64_t PunishmentService::calculateDuration(int32_t dayCount) {
	if (dayCount == 0)
		return std::numeric_limits<int32_t>::max(); // int because client handles this with seconds timestamp in int
	return static_cast<int64_t>(dayCount) * 86'400; // Java: TimeUnit.DAYS.toSeconds(dayCount)
}

// Java PunishmentService.java:69-89
void PunishmentService::setIsInPrison(model::gameobjects::player::Player& player, bool state, int64_t delayInMinutes, std::string_view reason) {
	if (state) {
		if (delayInMinutes > 0) {
			int64_t duration = delayInMinutes * 60'000; // Java: TimeUnit.MINUTES.toMillis(delayInMinutes)
			schedulePrisonTask(player, duration);
			ban::ChatBanService::banPlayer(player, delayInMinutes); // Java passes the minutes as durationMillis
			player.setPrisonEndTimeMillis(commons::utils::currentTimeMillis() + duration);
			teleport::TeleportService::teleportToPrison(player);
			dao::PlayerPunishmentsDAO::punishPlayer(player, PunishmentType::PRISON, reason);
			utils::PacketSendUtility::sendMessage(player, "You have been teleported to prison for a time of " + std::to_string(delayInMinutes) +
				" minutes.\n If you disconnect the time stops and the timer of the prison'll see at your next login.");
		}
	} else {
		player.getController().cancelTask(model::TaskId::PRISON);
		player.setPrisonEndTimeMillis(0);
		ban::ChatBanService::unbanPlayer(player);
		teleport::TeleportService::moveToBindLocation(player);
		dao::PlayerPunishmentsDAO::unpunishPlayer(player.getObjectId(), PunishmentType::PRISON);
		utils::PacketSendUtility::sendMessage(player, "You come out of prison.");
	}
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

// Java PunishmentService.java:133-150
void PunishmentService::setIsNotGatherable(model::gameobjects::player::Player& player, int32_t captchaCount, bool state, int64_t delay) {
	if (state) {
		if (captchaCount < 3) {
			runtime::Ptr<runtime::Array<int8_t>> image = player.getCaptchaImage();
			if (image == nullptr) {
				// Java: SM_CAPTCHA.writeImpl reads data.length when the dispatcher thread writes the packet (AionConnection.writeData): the NPE
				// ends at Dispatcher.run's log.error and the packet is lost, while this method goes on. C++ serializes at the send, so the packet
				// is not sent and the dispatcher's error logged here
				dispatcherLog.error("NullPointerException: SM_CAPTCHA.writeImpl: the captcha image of " + player.getName() + " is null");
			} else {
				std::vector<uint8_t> data;
				for (int8_t b : image->snapshot())
					data.push_back(static_cast<uint8_t>(b));
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CAPTCHA(captchaCount + 1, data));
			}
		} else {
			player.setCaptchaWord(std::nullopt);
			player.setCaptchaImage(nullptr);
		}
		player.setGatherRestrictionExpirationTime(commons::utils::currentTimeMillis() + delay);
		dao::PlayerPunishmentsDAO::punishPlayer(player, PunishmentType::GATHER, "Possible gatherbot");
	} else {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_RECOVERED());
		player.setCaptchaWord(std::nullopt);
		player.setCaptchaImage(nullptr);
		player.setGatherRestrictionExpirationTime(0);
		dao::PlayerPunishmentsDAO::unpunishPlayer(player.getObjectId(), PunishmentType::GATHER);
	}
}

} // namespace aion::gameserver::services
