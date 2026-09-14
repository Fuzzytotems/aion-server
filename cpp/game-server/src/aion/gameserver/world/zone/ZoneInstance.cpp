#include "aion/gameserver/world/zone/ZoneInstance.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone {

ZoneInstance::ZoneInstance(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue)
	: template_(templateValue), mapId(mapIdValue), handlers(runtime::RcArrayList<runtime::Ref<handler::ZoneHandler>>::create()) {
}

ZoneInstance::~ZoneInstance() = default;

runtime::Ref<ZoneInstance> ZoneInstance::create(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) {
	return runtime::makeRef<ZoneInstance>(mapIdValue, templateValue);
}

runtime::Ptr<model::geometry::Area> ZoneInstance::getAreaTemplate() {
	AION_UNPORTED();
}

const model::templates::zone::ZoneTemplate* ZoneInstance::getZoneTemplate() {
	AION_UNPORTED();
}

bool ZoneInstance::revalidate(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool ZoneInstance::onEnter(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool ZoneInstance::onLeave(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool ZoneInstance::onDie(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target) {
	AION_UNPORTED();
}

bool ZoneInstance::isInsideCreature(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool ZoneInstance::isInsideCordinate(float x, float y, float z) {
	AION_UNPORTED();
}

void ZoneInstance::addHandler(handler::ZoneHandler& handler) {
	AION_UNPORTED();
}

bool ZoneInstance::canFly() {
	AION_UNPORTED();
}

bool ZoneInstance::canGlide() {
	AION_UNPORTED();
}

bool ZoneInstance::canPutKisk() {
	AION_UNPORTED();
}

bool ZoneInstance::canRecall() {
	AION_UNPORTED();
}

bool ZoneInstance::canReturnToBattle() {
	AION_UNPORTED();
}

bool ZoneInstance::canRide() {
	AION_UNPORTED();
}

bool ZoneInstance::canFlyRide() {
	AION_UNPORTED();
}

bool ZoneInstance::isPvpAllowed() {
	AION_UNPORTED();
}

bool ZoneInstance::isSameRaceDuelsAllowed() {
	AION_UNPORTED();
}

bool ZoneInstance::isOtherRaceDuelsAllowed() {
	AION_UNPORTED();
}

int32_t ZoneInstance::getTownId() {
	AION_UNPORTED();
}

void ZoneInstance::forEach(const std::function<void(model::gameobjects::Creature&)>& action) {
	AION_UNPORTED();
}

bool ZoneInstance::isDominionZone() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::zone
