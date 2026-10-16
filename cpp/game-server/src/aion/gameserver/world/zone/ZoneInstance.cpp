#include "aion/gameserver/world/zone/ZoneInstance.h"

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/utils/collections/CollectionUtil.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/zone/ZoneAttributes.h"
#include "aion/gameserver/world/zone/ZoneAttributesInfo.h"
#include "aion/gameserver/world/zone/handler/AdvancedZoneHandler.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone {

using model::templates::zone::ZoneClassName;

ZoneInstance::ZoneInstance(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue)
	: template_(templateValue), mapId(mapIdValue), handlers(runtime::RcArrayList<runtime::Ref<handler::ZoneHandler>>::create()) {
}

ZoneInstance::~ZoneInstance() = default;

runtime::Ref<ZoneInstance> ZoneInstance::create(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) {
	return runtime::makeRef<ZoneInstance>(mapIdValue, templateValue);
}

runtime::Ptr<model::geometry::Area> ZoneInstance::getAreaTemplate() {
	return template_->getArea();
}

const model::templates::zone::ZoneTemplate* ZoneInstance::getZoneTemplate() {
	return template_->getZoneTemplate();
}

bool ZoneInstance::revalidate(model::gameobjects::Creature& creature) {
	return (mapId == creature.getWorldId() && template_->getArea()->isInside3D(creature.getX(), creature.getY(), creature.getZ()));
}

bool ZoneInstance::onEnter(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (creatures.containsKey(creature.getObjectId()))
			return false;
		creatures.put(creature.getObjectId(), runtime::Ref<model::gameobjects::Creature>(creature));
		if (dynamic_cast<model::gameobjects::player::Player*>(&creature) != nullptr)
			creature.getController().onEnterZone(*this);
		for (runtime::Ptr<handler::ZoneHandler> handler : *handlers.get())
			handler->onEnterZone(creature, *this);
		return true;
	}
}

bool ZoneInstance::onLeave(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (!creatures.containsKey(creature.getObjectId()))
			return false;
		creatures.remove(creature.getObjectId());
		creature.getController().onLeaveZone(*this);
		for (runtime::Ptr<handler::ZoneHandler> handler : *handlers.get())
			handler->onLeaveZone(creature, *this);
		return true;
	}
}

bool ZoneInstance::onDie(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target) {
	if (!creatures.containsKey(target.getObjectId()))
		return false;
	for (runtime::Ptr<handler::ZoneHandler> handler : *handlers.get()) {
		if (auto* advancedZoneHandler = dynamic_cast<handler::AdvancedZoneHandler*>(handler.get())) {
			if (advancedZoneHandler->onDie(attacker, target, *this))
				return true;
		}
	}
	return false;
}

bool ZoneInstance::isInsideCreature(model::gameobjects::Creature& creature) {
	return creatures.containsKey(creature.getObjectId());
}

bool ZoneInstance::isInsideCordinate(float x, float y, float z) {
	return template_->getArea()->isInside3D(x, y, z);
}

void ZoneInstance::addHandler(handler::ZoneHandler& handler) {
	handlers.get()->add(runtime::Ref<handler::ZoneHandler>(handler));
}

bool ZoneInstance::canFly() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getFlags() == -1 || zoneTemplate->getFlags() == 0 || World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::FLY))
		return World::getInstance().getWorldMap(mapId)->isFlightAllowed();
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::FLY)) != 0;
}

bool ZoneInstance::canGlide() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getFlags() == -1 || zoneTemplate->getFlags() == 0 ||
		World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::GLIDE))
		return World::getInstance().getWorldMap(mapId)->canGlide();
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::GLIDE)) != 0;
}

bool ZoneInstance::canPutKisk() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getFlags() == -1 || zoneTemplate->getFlags() == 0 ||
		World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::BIND))
		return World::getInstance().getWorldMap(mapId)->canPutKisk();
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::BIND)) != 0;
}

bool ZoneInstance::canRecall() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getFlags() == -1 || zoneTemplate->getFlags() == 0 ||
		World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::RECALL)) {
		return World::getInstance().getWorldMap(mapId)->canRecall();
	}
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::RECALL)) != 0;
}

bool ZoneInstance::canReturnToBattle() {
	return World::getInstance().getWorldMap(mapId)->canReturnToBattle();
}

bool ZoneInstance::canRide() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getFlags() == -1 || zoneTemplate->getFlags() == 0 ||
		World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::RIDE)) {
		return World::getInstance().getWorldMap(mapId)->canRide();
	}
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::RIDE)) != 0;
}

bool ZoneInstance::canFlyRide() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getFlags() == -1 || zoneTemplate->getFlags() == 0 ||
		World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::FLY_RIDE))
		return World::getInstance().getWorldMap(mapId)->canFlyRide();
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::FLY_RIDE)) != 0;
}

bool ZoneInstance::isPvpAllowed() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getZoneType() != ZoneClassName::PVP)
		return World::getInstance().getWorldMap(mapId)->isPvpAllowed();
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::PVP_ENABLED)) != 0;
}

bool ZoneInstance::isSameRaceDuelsAllowed() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getZoneType() != ZoneClassName::DUEL || zoneTemplate->getFlags() == 0 ||
		World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::DUEL_SAME_RACE_ENABLED))
		return World::getInstance().getWorldMap(mapId)->isSameRaceDuelsAllowed();
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::DUEL_SAME_RACE_ENABLED)) != 0;
}

bool ZoneInstance::isOtherRaceDuelsAllowed() {
	const auto* zoneTemplate = template_->getZoneTemplate();
	if (zoneTemplate->getZoneType() != ZoneClassName::DUEL || zoneTemplate->getFlags() == 0 ||
		World::getInstance().getWorldMap(mapId)->hasOverridenOption(ZoneAttributes::DUEL_OTHER_RACE_ENABLED))
		return World::getInstance().getWorldMap(mapId)->isOtherRaceDuelsAllowed();
	return (zoneTemplate->getFlags() & getId(ZoneAttributes::DUEL_OTHER_RACE_ENABLED)) != 0;
}

int32_t ZoneInstance::getTownId() {
	return template_->getZoneTemplate()->getTownId();
}

void ZoneInstance::forEach(const std::function<void(model::gameobjects::Creature&)>& action) {
	utils::collections::CollectionUtil::forEach(creatures.values(), [&action](const runtime::Ptr<model::gameobjects::Creature>& creature) {
		action(*creature);
	});
}

bool ZoneInstance::isDominionZone() {
	return template_->getZoneTemplate()->getZoneType() == ZoneClassName::DOMINION;
}

} // namespace aion::gameserver::world::zone
