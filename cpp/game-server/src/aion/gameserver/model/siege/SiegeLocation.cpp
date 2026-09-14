#include "aion/gameserver/model/siege/SiegeLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::model::siege {

SiegeLocation::SiegeLocation(const templates::siegelocation::SiegeLocationTemplate* value)
	: template_(value) {
}

runtime::Ref<SiegeLocation> SiegeLocation::create(const templates::siegelocation::SiegeLocationTemplate* value) {
	return runtime::makeRef<SiegeLocation>(value);
}

int32_t SiegeLocation::getLocationId() {
	AION_UNPORTED();
}

int32_t SiegeLocation::getWorldId() {
	AION_UNPORTED();
}

SiegeType SiegeLocation::getType() {
	AION_UNPORTED();
}

int32_t SiegeLocation::getSiegeDuration() {
	AION_UNPORTED();
}

std::vector<const templates::siegelocation::SiegeReward*> SiegeLocation::getRewards() {
	AION_UNPORTED();
}

void SiegeLocation::increaseOccupiedCount() {
	AION_UNPORTED();
}

void SiegeLocation::adjustFactionBalance(int32_t adjustment) {
	AION_UNPORTED();
}

bool SiegeLocation::isCanTeleport(runtime::Ptr<gameobjects::player::Player> player) {
	AION_UNPORTED();
}

int32_t SiegeLocation::getLegionGp() {
	AION_UNPORTED();
}

int32_t SiegeLocation::getInfluenceValue() {
	AION_UNPORTED();
}

void SiegeLocation::addZone(world::zone::SiegeZoneInstance& zone) {
	AION_UNPORTED();
}

bool SiegeLocation::isInsideLocation(gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool SiegeLocation::isInsideLocation(float x, float y, float z) {
	AION_UNPORTED();
}

void SiegeLocation::clearLocation() {
	AION_UNPORTED();
}

void SiegeLocation::onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void SiegeLocation::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

void SiegeLocation::forEachCreature(const std::function<void(gameobjects::Creature&)>& consumer) {
	AION_UNPORTED();
}

void SiegeLocation::forEachPlayer(const std::function<void(gameobjects::player::Player&)>& consumer) {
	AION_UNPORTED();
}

SiegeLocation::~SiegeLocation() = default;

} // namespace aion::gameserver::model::siege
