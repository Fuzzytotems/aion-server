#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"

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
#include "aion/gameserver/runtime/base/Unported.h"
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
		// M5a (plan E1-07): the Legion Dominion protector rank (CPInfo.setLDRank, CPBuff.applyEffect, SM_CONQUEROR_PROTECTOR) is ported with the
		// conqueror and protector system
		AION_UNPORTED();
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

bool ConquerorAndProtectorService::isOccupiedLegionDominionZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::onKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::sendDetectCooldown(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::addVictims(runtime::Ptr<model::gameobjects::player::Player> player, CPInfo& info, int32_t value) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::updateBuffAndNotifyNearbyPlayers(model::gameobjects::player::Player& player, CPInfo& cpInfo) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::intruderScan(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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

std::vector<runtime::Ptr<model::gameobjects::player::Player>> ConquerorAndProtectorService::findIntruders(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool ConquerorAndProtectorService::canSee(CPInfo& protector, CPInfo& intruder) {
	AION_UNPORTED();
}

int32_t ConquerorAndProtectorService::getRank(int32_t kills) {
	AION_UNPORTED();
}

std::optional<model::templates::cp::CPType> ConquerorAndProtectorService::getCPTypeForCurrentMap(model::gameobjects::player::Player& player) {
	std::optional<model::Race> race = handledWorlds.get(player.getWorldId());
	if (!race)
		return std::nullopt;
	return *race == player.getRace() ? model::templates::cp::CPType::PROTECTOR : model::templates::cp::CPType::CONQUEROR;
}

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
