#include "aion/gameserver/services/teleport/BindPointTeleportService.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HotspotData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/hotspot/HotspotTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_TELEPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::teleport {

// Java: LoggerFactory.getLogger(BindPointTeleportService.class), looked up in calculateTeleportationPrice
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.teleport.BindPointTeleportService");

namespace {

/** Java `"" + race` of the nullable HotspotTemplate.race (an absent race attribute prints "null") */
std::string raceToString(std::optional<model::Race> race) {
	return race ? std::string(xml::enumName(*race)) : "null";
}

} // namespace

BindPointTeleportService::Cooldown::Cooldown(int32_t value, int64_t cdEndValue) : locId(value), cdEnd(cdEndValue) {
}

runtime::Ref<BindPointTeleportService::Cooldown> BindPointTeleportService::Cooldown::create(int32_t value, int64_t cdEndValue) {
	return runtime::makeRef<BindPointTeleportService::Cooldown>(value, cdEndValue);
}

int32_t BindPointTeleportService::Cooldown::getTimeLeft() {
	int32_t estimated = static_cast<int32_t>((cdEnd.get() - commons::utils::currentTimeMillis()) / 1000);
	if (estimated > 0)
		return estimated;
	else
		return 0;
}

BindPointTeleportService::Cooldown::~Cooldown() = default;

void BindPointTeleportService::onLogin(model::gameobjects::player::Player& player) {
	runtime::Ptr<Cooldown> cooldown = getCooldown(player);
	if (cooldown && cooldown->getTimeLeft() > 0)
		utils::PacketSendUtility::broadcastPacketAndReceive(player, network::aion::serverpackets::SM_BIND_POINT_TELEPORT(3, player.getObjectId(),
			cooldown->getLocId(), cooldown->getTimeLeft()));
}

// Java BindPointTeleportService.java:39-72. The two anonymous Runnables are lambdas (fieldmap BindPointTeleportService$1 and $2: K3 tasks with
// the captures fieldmap.py prints - the player, the price, the loc id and the hotspot template; the player and the template). $1 is stored as
// the player's SKILL_USE task, as Java stores its Future, and pins the player until it has run or cancelTeleport / cancelAllTasks cancelled
// it; $2 is a one-shot task stored nowhere that pins the player for its second. The template is static data, captured by reference and pinned
// (a template pin is only checked, design §7.1).
void BindPointTeleportService::teleport(model::gameobjects::player::Player& player, int32_t locId, int64_t kinah) {
	const model::templates::hotspot::HotspotTemplate* hotspot = dataholders::DataManager::HOTSPOT_DATA->getHotspotTemplateById(locId);
	if (hotspot == nullptr) {
		utils::audit::AuditLogger::log(player, "Tried to use invalid hotspot teleport to locId " + std::to_string(locId));
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
		return;
	}
	int64_t price = calculateTeleportationPrice(player, hotspot, kinah);

	if (!checkRequirements(player, hotspot, price))
		return;

	utils::PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_BIND_POINT_TELEPORT(1, player.getObjectId(), locId, 0), true);

	const model::templates::hotspot::HotspotTemplate& hotspotTemplate = *hotspot;
	player.getController().addTask(model::TaskId::SKILL_USE,
		utils::ThreadPoolManager::getInstance().schedule({&player, &hotspotTemplate}, [&player, price, locId, &hotspotTemplate] {
		// Java BindPointTeleportService.java:56-70
		if (!player.getInventory().tryDecreaseKinah(price, item::ItemPacketService_ItemUpdateType::DEC_KINAH_FLY)) {
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NOT_ENOUGH_FEE());
			return;
		}
		addCooldown(player, locId);
		utils::PacketSendUtility::broadcastPacket(player,
			network::aion::serverpackets::SM_BIND_POINT_TELEPORT(3, player.getObjectId(), locId, COOLDOWN_IN_SECONDS), true);
		utils::ThreadPoolManager::getInstance().schedule({&player, &hotspotTemplate}, [&player, &hotspotTemplate] {
			// Java BindPointTeleportService.java:65-68
			if (!player.getLifeStats()->isAboutToDie() && !player.isDead())
				TeleportService::teleportTo(player, hotspotTemplate.getWorldId(), hotspotTemplate.getX(), hotspotTemplate.getY(), hotspotTemplate.getZ());
		}, 1000);
	}, 10000));
}

// Java BindPointTeleportService.java:74-79
void BindPointTeleportService::cancelTeleport(model::gameobjects::player::Player& player, int32_t locId) {
	if (player.getController().hasTask(model::TaskId::SKILL_USE)) {
		player.getController().cancelTask(model::TaskId::SKILL_USE);
		utils::PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_BIND_POINT_TELEPORT(2, player.getObjectId(), locId, 0), true);
	}
}

// Java BindPointTeleportService.java:81-90
int64_t BindPointTeleportService::calculateTeleportationPrice(model::gameobjects::player::Player& player,
	const model::templates::hotspot::HotspotTemplate* hotspot, int64_t priceSentByGameClient) {
	double distance = utils::PositionUtil::getDistance(player, hotspot->getX(), hotspot->getY(), hotspot->getZ());
	int64_t basePrice = hotspot->getPrice();
	int64_t distanceCost = static_cast<int64_t>(static_cast<double>(basePrice) * distance / 1000.0); // Java: (long) (basePrice * distance / 1000d)
	int64_t price = std::max<int64_t>(1, basePrice + distanceCost);
	int64_t priceDifference = std::abs(price - priceSentByGameClient);
	if (priceDifference > 1) // only warn about unexpected differences (minimal discrepancies from floating-point calculations can be ignored)
		log.warn("Hotspot teleport {} prices don't match: {} vs. {}", hotspot->getId(), price, priceSentByGameClient);
	return std::max(price, priceSentByGameClient);
}

// Java BindPointTeleportService.java:92-114
bool BindPointTeleportService::checkRequirements(model::gameobjects::player::Player& player,
	const model::templates::hotspot::HotspotTemplate* hotspot, int64_t price) {
	if (player.getWorldId() != hotspot->getWorldId()) {
		utils::audit::AuditLogger::log(player, "tried to use hotspot teleport " + std::to_string(hotspot->getId()) + " from invalid start world "
												   + std::to_string(player.getWorldId()) + ", expected " + std::to_string(hotspot->getWorldId()));
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
		return false;
	}
	// Java `player.getRace() != hotspot.getRace()` compares with the nullable race: a hotspot without one refuses every player but PC_ALL
	if (!(player.getRace() == model::Race::PC_ALL) && (!hotspot->getRace() || player.getRace() != *hotspot->getRace())) {
		utils::audit::AuditLogger::log(player, "tried to use hotspot teleport " + std::to_string(hotspot->getId()) + " for invalid race "
												   + std::string(xml::enumName(player.getRace())) + ", expected " + raceToString(hotspot->getRace()));
		return false;
	}
	if (player.getInventory().getKinah() < price) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NOT_ENOUGH_FEE());
		return false;
	}
	runtime::Ptr<Cooldown> cooldown = getCooldown(player);
	if (cooldown && cooldown->getTimeLeft() > 0) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_FLYING_TIME_NOT_READY());
		return false;
	}

	return true;
}

void BindPointTeleportService::addCooldown(model::gameobjects::player::Player& player, int32_t locId) {
	int64_t cooldown = commons::utils::currentTimeMillis() + COOLDOWN_IN_SECONDS * 1000;
	cooldowns.put(player.getObjectId(), Cooldown::create(locId, cooldown));
}

runtime::Ptr<BindPointTeleportService::Cooldown> BindPointTeleportService::getCooldown(model::gameobjects::player::Player& player) {
	return cooldowns.get(player.getObjectId());
}

} // namespace aion::gameserver::services::teleport
