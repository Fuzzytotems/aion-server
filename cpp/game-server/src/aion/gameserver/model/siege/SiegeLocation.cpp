#include "aion/gameserver/model/siege/SiegeLocation.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/utils/collections/CollectionUtil.h"
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
	// Java: empty (only FortressLocation overrides it)
}

void SiegeLocation::onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	// java-race: Java's containsKey/put guard is not atomic (SiegeLocation.java:209). A lost race, or an entry that onLeaveZone kept because
	// the creature was still inside another zone of the location, leaves the previous Ref in the maps; both are keyed by object id, so it is
	// replaced on the next enter after the removal, exactly as in Java.
	if (!creatures.containsKey(creature.getObjectId())) {
		creatures.put(creature.getObjectId(), runtime::Ref<gameobjects::Creature>(creature));
		if (auto* player = dynamic_cast<gameobjects::player::Player*>(&creature)) {
			players.put(creature.getObjectId(), runtime::Ref<gameobjects::player::Player>(*player));
		}
	}
}

void SiegeLocation::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	if (!isInsideLocation(creature)) {
		creatures.remove(creature.getObjectId());
		players.remove(creature.getObjectId());
	}
}

void SiegeLocation::forEachCreature(const std::function<void(gameobjects::Creature&)>& consumer) {
	utils::collections::CollectionUtil::forEach(creatures.values(), [&consumer](const runtime::Ptr<gameobjects::Creature>& creature) {
		consumer(*creature);
	});
}

void SiegeLocation::forEachPlayer(const std::function<void(gameobjects::player::Player&)>& consumer) {
	utils::collections::CollectionUtil::forEach(players.values(), [&consumer](const runtime::Ptr<gameobjects::player::Player>& player) {
		consumer(*player);
	});
}

SiegeLocation::~SiegeLocation() = default;

} // namespace aion::gameserver::model::siege
