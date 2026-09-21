#include "aion/gameserver/model/siege/SiegeLocation.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
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
	return template_->getId();
}

int32_t SiegeLocation::getWorldId() {
	return template_->getWorldId();
}

SiegeType SiegeLocation::getType() {
	// Java returns the template's nullable type; every siege_location of the data has one (a null would be a C++ NullPointerException here)
	if (!template_->getType())
		throw runtime::NullPointerException("SiegeLocationTemplate.getType() is null for location " + std::to_string(template_->getId()));
	return *template_->getType();
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
	if (template_ == nullptr)
		throw runtime::NullPointerException("template"); // Java: NullPointerException
	return template_->getInfluenceValue();
}

void SiegeLocation::addZone(world::zone::SiegeZoneInstance& zone) {
	zones.add(runtime::Ref<world::zone::SiegeZoneInstance>(zone));
	zone.addHandler(*this);
}

bool SiegeLocation::isInsideLocation(gameobjects::Creature& creature) {
	if (zones.isEmpty())
		return false;
	for (runtime::Ptr<world::zone::SiegeZoneInstance> zone : zones)
		if (zone->isInsideCreature(creature))
			return true;
	return false;
}

bool SiegeLocation::isInsideLocation(float x, float y, float z) {
	if (zones.isEmpty())
		return false;
	for (runtime::Ptr<world::zone::SiegeZoneInstance> zone : zones)
		if (zone->isInsideCordinate(x, y, z))
			return true;
	return false;
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
