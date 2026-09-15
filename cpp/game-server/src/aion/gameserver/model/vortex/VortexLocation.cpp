#include "aion/gameserver/model/vortex/VortexLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/vortex/HomePoint.h"
#include "aion/gameserver/model/templates/vortex/StartPoint.h"
#include "aion/gameserver/model/templates/vortex/VortexTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"
#include "aion/gameserver/world/zone/InvasionZoneInstance.h"

namespace aion::gameserver::model::vortex {

VortexLocation::VortexLocation(const templates::vortex::VortexTemplate* value)
	: template_(value) {
}

runtime::Ref<VortexLocation> VortexLocation::create(const templates::vortex::VortexTemplate* value) {
	return runtime::makeRef<VortexLocation>(value);
}

void VortexLocation::setActiveVortex(runtime::Ptr<services::vortex::DimensionalVortex> vortex) {
	AION_UNPORTED();
}

void VortexLocation::setVortexController(runtime::Ptr<controllers::RVController> controller) {
	this->vortexController.set(controller);
}

runtime::Ptr<world::WorldPosition> VortexLocation::getHomePoint() {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldPosition> VortexLocation::getResurrectionPoint() {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldPosition> VortexLocation::getStartPoint() {
	AION_UNPORTED();
}

int32_t VortexLocation::getId() {
	return template_->getId();
}

Race VortexLocation::getDefendersRace() {
	AION_UNPORTED();
}

Race VortexLocation::getInvadersRace() {
	AION_UNPORTED();
}

int32_t VortexLocation::getHomeWorldId() {
	const auto* home = template_->getHomePoint();
	if (home == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("VortexTemplate.getHomePoint() is null");
	return home->getWorldId();
}

int32_t VortexLocation::getInvasionWorldId() {
	const auto* start = template_->getStartPoint();
	if (start == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("VortexTemplate.getStartPoint() is null");
	return start->getWorldId();
}

bool VortexLocation::isInvaderInside(int32_t objId) {
	AION_UNPORTED();
}

bool VortexLocation::isInsideActiveVotrex(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void VortexLocation::addZone(world::zone::InvasionZoneInstance& zone) {
	zones.add(runtime::Ref<world::zone::InvasionZoneInstance>(zone));
	zone.addHandler(*this);
}

bool VortexLocation::isInsideLocation(gameobjects::Creature& creature) {
	AION_UNPORTED();
}

void VortexLocation::onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

// lambda at VortexLocation.java:181 (fieldmap key vortex.VortexLocation@L181:49)
// lambda at VortexLocation.java:189 (fieldmap key vortex.VortexLocation@L189:48)
void VortexLocation::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	AION_UNPORTED();
}

VortexLocation::~VortexLocation() = default;

} // namespace aion::gameserver::model::vortex
