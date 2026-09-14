#include "aion/gameserver/world/zone/ZoneInstance.h"

#include "aion/gameserver/runtime/base/Unported.h"

// S0b transition (docs/design/hub-headers.md §3.3): the constructor, the destructor and create() need the complete member types. Creature and
// ZoneHandler are hubs of other S0b groups; ZoneInfo is not a hub (its header comes with its chunk). Remove the guard once they exist.
#if __has_include("aion/gameserver/model/gameobjects/Creature.h") && __has_include("aion/gameserver/model/templates/zone/ZoneInfo.h") && \
	__has_include("aion/gameserver/world/zone/handler/ZoneHandler.h")
#define AION_S0B_ZONE_INSTANCE_MEMBERS 1
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"
#else
#define AION_S0B_ZONE_INSTANCE_MEMBERS 0
#endif

namespace aion::gameserver::world::zone {

#if AION_S0B_ZONE_INSTANCE_MEMBERS
ZoneInstance::ZoneInstance(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue)
	: template_(templateValue), mapId(mapIdValue), handlers(runtime::RcArrayList<runtime::Ref<handler::ZoneHandler>>::create()) {
}

ZoneInstance::~ZoneInstance() = default;

runtime::Ref<ZoneInstance> ZoneInstance::create(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) {
	return runtime::makeRef<ZoneInstance>(mapIdValue, templateValue);
}
#endif

runtime::Ptr<model::geometry::Area> ZoneInstance::getAreaTemplate() {
	AION_UNPORTED();
}

const model::templates::zone::ZoneTemplate* ZoneInstance::getZoneTemplate() {
	AION_UNPORTED();
}

bool ZoneInstance::revalidate(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
bool ZoneInstance::onEnter(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

// lint: L7 Java synchronized; the ported body is SYNCHRONIZED(*this) { ... } (hub-headers.md §11.4)
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
