#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

ConquerorAndProtectorService::ConquerorAndProtectorService() = default;

ConquerorAndProtectorService::~ConquerorAndProtectorService() = default;

ConquerorAndProtectorService& ConquerorAndProtectorService::getInstance() {
	static ConquerorAndProtectorService instance; // Java SingletonHolder
	return instance;
}

// callback at ConquerorAndProtectorService.java:56 (fieldmap key ConquerorAndProtectorService@L56:55)
void ConquerorAndProtectorService::init() {
	AION_UNPORTED();
}

runtime::Ptr<CPInfo> ConquerorAndProtectorService::getCPInfoForCurrentMap(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<CPInfo> ConquerorAndProtectorService::getCPInfoForCurrentMap(model::gameobjects::player::Player& player, bool createIfNotExists) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::onEnterMap(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::onLeaveMap(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void ConquerorAndProtectorService::onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
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
	AION_UNPORTED();
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
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
