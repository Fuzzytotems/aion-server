#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/cp/CPType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CONQUEROR_PROTECTOR.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/ConquerorAndProtectorData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/legionDominion/LegionDominionLocation.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/cp/CPRank.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/LegionDominionService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPBuff.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

ConquerorAndProtectorService::ConquerorAndProtectorService() = default;

ConquerorAndProtectorService::~ConquerorAndProtectorService() = default;

ConquerorAndProtectorService& ConquerorAndProtectorService::getInstance() {
	static ConquerorAndProtectorService instance; // Java SingletonHolder
	return instance;
}

// callback at ConquerorAndProtectorService.java:56 (fieldmap key ConquerorAndProtectorService@L56:55)
void ConquerorAndProtectorService::init() {
	using configs::main::CustomConfig;
	std::shared_ptr<const std::unordered_set<int32_t>> worlds = CustomConfig::CONQUEROR_AND_PROTECTOR_WORLDS.get();
	if (!CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED.load() || !worlds || worlds->empty())
		return;

	for (int32_t worldId : *worlds) {
		int32_t worldType = worldId / 10000000 % 10; // the second digit in the map ID denotes the world type (0 = all, 1 = elyos, 2 = asmodians)
		if (worldType == 1)
			handledWorlds.put(worldId, model::Race::ELYOS);
		else if (worldType == 2)
			handledWorlds.put(worldId, model::Race::ASMODIANS);
		else
			throw commons::utils::IllegalArgumentException(
				"Map " + std::to_string(worldId) + " is not supported for conqueror and protector system (not race specific).");
	}

	// kills remove timer; Java multiplies two ints (the interval in milliseconds wraps like an int)
	const int64_t interval = static_cast<int32_t>(static_cast<uint32_t>(CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_INTERVAL.load()) * 60000u);
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		int64_t nowMillis = commons::utils::currentTimeMillis();
		intruderScanCooldowns.values().removeIf([nowMillis](int64_t cd) { return nowMillis >= cd; });
		std::vector<runtime::Ptr<CPInfo>> infos = conquerors.values().toVector();
		for (const runtime::Ptr<CPInfo>& info : protectors.values().toVector())
			infos.push_back(info);
		for (const runtime::Ptr<CPInfo>& info : infos) {
			if (info->getVictims() > 0)
				addVictims(nullptr, *info, -CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_DECREASE_COUNT.load());
		}
	}, interval, interval);
}

runtime::Ptr<CPInfo> ConquerorAndProtectorService::getCPInfoForCurrentMap(model::gameobjects::player::Player& player) {
	return getCPInfoForCurrentMap(player, false);
}

runtime::Ptr<CPInfo> ConquerorAndProtectorService::getCPInfoForCurrentMap(model::gameobjects::player::Player& player, bool createIfNotExists) {
	using model::templates::cp::CPType;
	runtime::Ptr<CPInfo> cpInfo = nullptr;
	std::optional<CPType> cpTypeForCurrentMap = getCPTypeForCurrentMap(player);
	if (cpTypeForCurrentMap) {
		CPType type = *cpTypeForCurrentMap;
		if (createIfNotExists)
			cpInfo = type == CPType::PROTECTOR
				? protectors.computeIfAbsent(player.getObjectId(), [type, &player] { return CPInfo::create(type, player); })
				: conquerors.computeIfAbsent(player.getObjectId(), [type, &player] { return CPInfo::create(type, player); });
		else
			cpInfo = type == CPType::PROTECTOR ? protectors.get(player.getObjectId()) : conquerors.get(player.getObjectId());
	}
	return cpInfo;
}

void ConquerorAndProtectorService::onEnterMap(model::gameobjects::player::Player& player) {
	using model::templates::cp::CPType;
	if (!configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED.load())
		return;
	runtime::Ptr<CPInfo> cpInfo = getCPInfoForCurrentMap(player);
	if (cpInfo && cpInfo->getLDRank() == 0) {
		int32_t type = cpInfo->getType() == CPType::CONQUEROR ? 0 : 7;
		int32_t intruderScanCd = cpInfo->getType() == CPType::CONQUEROR ? 0 : getOrRemoveCooldown(player.getObjectId());
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CONQUEROR_PROTECTOR(type, cpInfo->getRank(), intruderScanCd));
		updateBuffAndNotifyNearbyPlayers(player, *cpInfo);
	}
}

void ConquerorAndProtectorService::onLeaveMap(model::gameobjects::player::Player& player) {
	runtime::Ptr<CPInfo> info = getCPInfoForCurrentMap(player);
	if (info)
		info->getBuff()->endEffect(player);
}

void ConquerorAndProtectorService::onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	if (!configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED.load())
		return;
	if (zone.isDominionZone() && isOccupiedLegionDominionZone(player, zone)) {
		// Java ConquerorAndProtectorService.java:107-112
		using model::templates::cp::CPType;
		runtime::Ptr<CPInfo> cpInfo = protectors.computeIfAbsent(player.getObjectId(), [&player] { return CPInfo::create(CPType::PROTECTOR, player); });
		if (cpInfo->getLDRank() == 0) {
			cpInfo->setLDRank(3);
			cpInfo->getBuff()->applyEffect(player, cpInfo->getType(), 3);
			utils::PacketSendUtility::sendPacket(player,
				network::aion::serverpackets::SM_CONQUEROR_PROTECTOR(7, cpInfo->getLDRank(), getOrRemoveCooldown(player.getObjectId())));
		}
	}
}

void ConquerorAndProtectorService::onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	if (!configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED.load())
		return;
	if (zone.isDominionZone() && isOccupiedLegionDominionZone(player, zone)) {
		resetLegionDominionRank(player);
	}
}

void ConquerorAndProtectorService::onLeaveLegion(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::resetLegionDominionRank(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// Java ConquerorAndProtectorService.java:139-144
bool ConquerorAndProtectorService::isOccupiedLegionDominionZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();
	if (legion == nullptr)
		return false;
	runtime::Ptr<model::legionDominion::LegionDominionLocation> loc =
		LegionDominionService::getInstance().getLegionDominionLoc(legion->getOccupiedLegionDominion());
	if (loc == nullptr)
		return false;
	const world::zone::ZoneName* zoneName = zone.getAreaTemplate()->getZoneName();
	if (zoneName == nullptr) // Java: getZoneName().name() on null
		throw runtime::NullPointerException("Area.getZoneName()");
	return commons::utils::StringUtils::equalsIgnoreCase(loc->getZoneNameAsString(), zoneName->name());
}

// Java ConquerorAndProtectorService.java:146-163
void ConquerorAndProtectorService::onKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim) {
	using model::templates::cp::CPType;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	if (!configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_SYSTEM_ENABLED.load())
		return;
	if (handledWorlds.containsKey(victim.getWorldId())) {
		if (killer.getLevel() - victim.getLevel() <= configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_LEVEL_DIFF.load()) {
			runtime::Ptr<CPInfo> killerInfo = getCPInfoForCurrentMap(killer, true);
			runtime::Ptr<CPInfo> victimInfo = getCPInfoForCurrentMap(victim);

			if (victimInfo != nullptr && victimInfo->getType() == CPType::CONQUEROR && victimInfo->getRank() == 3) {
				SM_SYSTEM_MESSAGE msg = killer.getRace() == model::Race::ASMODIANS
					? SM_SYSTEM_MESSAGE::STR_MSG_SLAYER_LIGHT_DEATH_TO_B(killer.getName(), victim.getName())
					: SM_SYSTEM_MESSAGE::STR_MSG_SLAYER_DARK_DEATH_TO_B(killer.getName(), victim.getName());
				utils::PacketSendUtility::broadcastToMap(victim, msg);
			}
			if (killerInfo == nullptr) // Java: addVictims(killer, null, 1) reads info.getVictims() (a killer on a map without a type)
				throw runtime::NullPointerException("CPInfo");
			addVictims(runtime::Ptr<model::gameobjects::player::Player>(killer), *killerInfo, 1);
		}
	}
}

// Java ConquerorAndProtectorService.java:165-170
void ConquerorAndProtectorService::sendDetectCooldown(model::gameobjects::player::Player& player) {
	runtime::Ptr<CPInfo> cpInfo = getCPInfoForCurrentMap(player);
	if (cpInfo == nullptr)
		return;
	utils::PacketSendUtility::sendPacket(player,
		network::aion::serverpackets::SM_CONQUEROR_PROTECTOR(7, cpInfo->getLDRank(), getOrRemoveCooldown(player.getObjectId())));
}

// Java ConquerorAndProtectorService.java:172-185
void ConquerorAndProtectorService::addVictims(runtime::Ptr<model::gameobjects::player::Player> player, CPInfo& info, int32_t count) {
	using model::templates::cp::CPType;
	int32_t newVictims = std::min(std::max(info.getVictims() + count, 0), configs::main::CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK3.load());
	info.setVictims(newVictims);
	int32_t newRank = getRank(info.getVictims());
	if (info.getRank() != newRank) {
		info.setRank(newRank);
		if (info.getRank() == 0 && info.getLDRank() == 0)
			(info.getType() == CPType::CONQUEROR ? conquerors : protectors).remove(info.getPlayerId());
		if (player == nullptr)
			player = world::World::getInstance().getPlayer(info.getPlayerId());
		if (player != nullptr)
			updateBuffAndNotifyNearbyPlayers(*player, info);
	}
}

// Java ConquerorAndProtectorService.java:187-196
void ConquerorAndProtectorService::updateBuffAndNotifyNearbyPlayers(model::gameobjects::player::Player& player, CPInfo& cpInfo) {
	using model::templates::cp::CPType;
	if (cpInfo.getLDRank() == 0) { // not inside a legion dominion zone
		if (getCPTypeForCurrentMap(player) == cpInfo.getType())
			cpInfo.getBuff()->applyEffect(player, cpInfo.getType(), cpInfo.getRank());
		else
			cpInfo.getBuff()->endEffect(player);
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_CONQUEROR_PROTECTOR(cpInfo.getType() == CPType::CONQUEROR ? 1 : 8, cpInfo.getRank()));
		utils::PacketSendUtility::broadcastPacket(player,
			network::aion::serverpackets::SM_CONQUEROR_PROTECTOR(cpInfo.getType() == CPType::CONQUEROR ? 6 : 9, player));
	}
}

// Java ConquerorAndProtectorService.java:198-205
void ConquerorAndProtectorService::intruderScan(model::gameobjects::player::Player& player) {
	if (getCPTypeForCurrentMap(player) != model::templates::cp::CPType::PROTECTOR)
		return;
	if (getOrRemoveCooldown(player.getObjectId()) > 0)
		return;
	intruderScanCooldowns.put(player.getObjectId(), commons::utils::currentTimeMillis() + 180 * 1000);
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CONQUEROR_PROTECTOR(findIntruders(player), true));
}

int32_t ConquerorAndProtectorService::getOrRemoveCooldown(int32_t objectId) {
	std::optional<int64_t> cd = intruderScanCooldowns.get(objectId);
	if (cd) {
		int64_t remainingMillis = *cd - commons::utils::currentTimeMillis();
		if (remainingMillis > 0) {
			return static_cast<int32_t>(remainingMillis / 1000);
		} else {
			intruderScanCooldowns.remove(objectId);
		}
	}
	return 0;
}

// Java ConquerorAndProtectorService.java:220-233
std::vector<runtime::Ptr<model::gameobjects::player::Player>> ConquerorAndProtectorService::findIntruders(model::gameobjects::player::Player& player) {
	runtime::Ptr<CPInfo> protector = protectors.get(player.getObjectId());
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> intruders;
	if (protector != nullptr) {
		for (const runtime::Ptr<CPInfo>& conqueror : conquerors.values()) {
			if (canSee(*protector, *conqueror)) {
				runtime::Ptr<model::gameobjects::player::Player> intruder = world::World::getInstance().getPlayer(conqueror->getPlayerId());
				if (intruder != nullptr && intruder->getRace() != player.getRace() && utils::PositionUtil::isInRange(*intruder, player, 500))
					intruders.push_back(intruder);
			}
		}
	}
	return intruders;
}

// Java ConquerorAndProtectorService.java:238-242: true if the protector has the required rank to detect the intruders position on the map
bool ConquerorAndProtectorService::canSee(CPInfo& protector, CPInfo& intruder) {
	int32_t rank = std::max(protector.getLDRank(), protector.getRank());
	const model::templates::cp::CPRank* cpRank =
		dataholders::DataManager::CONQUEROR_AND_PROTECTOR_DATA->getRank(model::templates::cp::CPType::PROTECTOR, rank);
	return cpRank != nullptr && cpRank->getVisibleIntruderMinRank() != 0 && cpRank->getVisibleIntruderMinRank() >= intruder.getRank();
}

// Java ConquerorAndProtectorService.java:244-252
int32_t ConquerorAndProtectorService::getRank(int32_t kills) {
	using configs::main::CustomConfig;
	if (kills >= CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK3.load())
		return 3;
	if (kills >= CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK2.load())
		return 2;
	if (kills >= CustomConfig::CONQUEROR_AND_PROTECTOR_KILLS_RANK1.load())
		return 1;
	return 0;
}

std::optional<model::templates::cp::CPType> ConquerorAndProtectorService::getCPTypeForCurrentMap(model::gameobjects::player::Player& player) {
	std::optional<model::Race> race = handledWorlds.get(player.getWorldId());
	if (!race)
		return std::nullopt;
	return *race == player.getRace() ? model::templates::cp::CPType::PROTECTOR : model::templates::cp::CPType::CONQUEROR;
}

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
