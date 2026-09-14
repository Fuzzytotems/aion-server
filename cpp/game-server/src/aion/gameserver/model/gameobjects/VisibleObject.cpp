#include "aion/gameserver/model/gameobjects/VisibleObject.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"

// S0b transition (docs/design/hub-headers.md "Complete types in the .cpp"): the constructor, the destructor and the part accessors need the
// complete part and member types, whose hub headers are written by other S0b groups. Remove the guard once they exist (spine freeze).
#if __has_include("aion/gameserver/controllers/VisibleObjectController.h") && __has_include("aion/gameserver/world/knownlist/KnownList.h") && \
	__has_include("aion/gameserver/world/WorldPosition.h") && __has_include("aion/gameserver/model/templates/spawns/SpawnTemplate.h")
#define AION_S0B_VISIBLE_OBJECT_PARTS 1
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#else
#define AION_S0B_VISIBLE_OBJECT_PARTS 0
#endif

namespace aion::gameserver::model::gameobjects {

#if AION_S0B_VISIBLE_OBJECT_PARTS
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
#endif

std::string VisibleObject::getName() {
	AION_UNPORTED();
}

int32_t VisibleObject::getInstanceId() {
	AION_UNPORTED();
}

int32_t VisibleObject::getWorldId() {
	AION_UNPORTED();
}

world::WorldType VisibleObject::getWorldType() {
	AION_UNPORTED();
}

world::WorldDropType VisibleObject::getWorldDropType() {
	AION_UNPORTED();
}

float VisibleObject::getX() {
	AION_UNPORTED();
}

float VisibleObject::getY() {
	AION_UNPORTED();
}

float VisibleObject::getZ() {
	AION_UNPORTED();
}

int8_t VisibleObject::getHeading() {
	AION_UNPORTED();
}

runtime::Ptr<world::WorldMapInstance> VisibleObject::getWorldMapInstance() {
	AION_UNPORTED();
}

bool VisibleObject::isSpawned() {
	AION_UNPORTED();
}

bool VisibleObject::isInWorld() {
	AION_UNPORTED();
}

bool VisibleObject::isInInstance() {
	AION_UNPORTED();
}

void VisibleObject::clearKnownlist() {
	AION_UNPORTED();
}

void VisibleObject::clearKnownlist(animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

void VisibleObject::updateKnownlist() {
	AION_UNPORTED();
}

bool VisibleObject::canSee(runtime::Ptr<VisibleObject> object) {
	AION_UNPORTED();
}

void VisibleObject::setTarget(runtime::Ptr<VisibleObject> creature) {
	AION_UNPORTED();
}

void VisibleObject::breakTarget() noexcept {
	(void)target.exchange(runtime::Ptr<VisibleObject>());
}

bool VisibleObject::isTargeting(int32_t value) {
	AION_UNPORTED();
}

std::string VisibleObject::toString() {
	AION_UNPORTED();
}

std::vector<const char*> VisibleObject::breakKnownEdges() {
	return player::LogoutBreakers::breakZombieEdges(*this);
}

} // namespace aion::gameserver::model::gameobjects
