#include "aion/gameserver/model/gameobjects/VisibleObject.h"

#include <string>
#include <typeinfo>

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects {

VisibleObject::VisibleObject(CreateKey, int32_t objId, std::unique_ptr<controllers::VisibleObjectController> controllerValue,
	runtime::Ptr<templates::spawns::SpawnTemplate> spawnTemplateValue, const templates::VisibleObjectTemplate* objectTemplateValue,
	runtime::Ptr<world::WorldPosition> positionValue, bool autoReleaseObjectId)
	: AionObject(objId, autoReleaseObjectId), objectTemplate(objectTemplateValue), position(runtime::Ref<world::WorldPosition>(positionValue)),
	  controller(std::move(controllerValue)), spawnTemplate(spawnTemplateValue) {
}

VisibleObject::~VisibleObject() = default;

void VisibleObject::setKnownlist(std::unique_ptr<world::knownlist::KnownList> value) {
	knownlist.set(std::move(value));
}

world::knownlist::KnownList& VisibleObject::getKnownList() const {
	return *knownlist;
}

controllers::VisibleObjectController& VisibleObject::getController() const {
	return *controller;
}

void VisibleObject::setPosition(runtime::Ptr<world::WorldPosition> value) {
	position.set(value);
}

std::string VisibleObject::getName() {
	return objectTemplate->getName(); // Java NullPointerException for an object without template; subclasses without one override getName
}

int32_t VisibleObject::getInstanceId() {
	return getPosition()->getInstanceId();
}

int32_t VisibleObject::getWorldId() {
	return getPosition()->getMapId();
}

world::WorldType VisibleObject::getWorldType() {
	return world::World::getInstance().getWorldMap(getWorldId())->getWorldType();
}

world::WorldDropType VisibleObject::getWorldDropType() {
	return world::World::getInstance().getWorldMap(getWorldId())->getWorldDropType();
}

float VisibleObject::getX() {
	return getPosition()->getX();
}

float VisibleObject::getY() {
	return getPosition()->getY();
}

float VisibleObject::getZ() {
	return getPosition()->getZ();
}

int8_t VisibleObject::getHeading() {
	return getPosition()->getHeading();
}

runtime::Ptr<world::WorldMapInstance> VisibleObject::getWorldMapInstance() {
	return getPosition()->getWorldMapInstance();
}

bool VisibleObject::isSpawned() {
	return getPosition()->isSpawned();
}

bool VisibleObject::isInWorld() {
	return world::World::getInstance().isInWorld(getObjectId());
}

bool VisibleObject::isInInstance() {
	return getPosition()->isInstanceMap();
}

void VisibleObject::clearKnownlist() {
	clearKnownlist(animations::ObjectDeleteAnimation::FADE_OUT);
}

void VisibleObject::clearKnownlist(animations::ObjectDeleteAnimation animation) {
	getKnownList().clear(animation);
}

void VisibleObject::updateKnownlist() {
	getKnownList().update();
}

bool VisibleObject::canSee(runtime::Ptr<VisibleObject> object) {
	return static_cast<bool>(object);
}

void VisibleObject::setTarget(runtime::Ptr<VisibleObject> creature) {
	if (target.get() != creature) {
		target.set(creature);
		// Java bug kept (S0B-052, D6): the field is assigned before the call, so Java passes the new target as the old one too
		// (VisibleObject.java:206-209)
		getController().onTargetChanged(creature, creature);
	}
}

void VisibleObject::breakTarget() noexcept {
	(void)target.exchange(runtime::Ptr<VisibleObject>());
}

bool VisibleObject::isTargeting(int32_t value) {
	runtime::Ptr<VisibleObject> current = target.get();
	return current && current->getObjectId() == value;
}

std::string VisibleObject::toString() {
	std::string result = utils::simpleClassName(typeid(*this)) + " [";
	if (objectTemplate != nullptr)
		result += "templateId=" + std::to_string(objectTemplate->getTemplateId()) + ", ";
	runtime::Ptr<world::WorldPosition> currentPosition = position.get();
	result += "objectId=" + std::to_string(getObjectId()) + ", name=" + getName() +
		", position=" + (currentPosition ? currentPosition->toString() : std::string("null")) + "]";
	return result;
}

std::vector<const char*> VisibleObject::breakKnownEdges() {
	return player::LogoutBreakers::breakZombieEdges(*this);
}

} // namespace aion::gameserver::model::gameobjects
