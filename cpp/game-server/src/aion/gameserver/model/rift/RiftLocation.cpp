#include "aion/gameserver/model/rift/RiftLocation.h"

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/rift/RiftTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::model::rift {

RiftLocation::RiftLocation(const templates::rift::RiftTemplate* value)
	: template_(value) {
}

runtime::Ref<RiftLocation> RiftLocation::create(const templates::rift::RiftTemplate* value) {
	return runtime::makeRef<RiftLocation>(value);
}

// Java RiftLocation.java:23-25
int32_t RiftLocation::getId() {
	return template_->getId();
}

// Java RiftLocation.java:27-29
int32_t RiftLocation::getWorldId() {
	return template_->getWorldId();
}

// Java RiftLocation.java:31-33
bool RiftLocation::hasSpawns() {
	return template_->hasSpawns();
}

// Java RiftLocation.java:35-37
bool RiftLocation::isAutoCloseable() {
	return template_->isAutoCloseable();
}

// Java RiftLocation.java:51-53
void RiftLocation::addSpawned(gameobjects::VisibleObject& object) {
	spawned.put(object.getObjectId(), runtime::Ref<templates::spawns::SpawnTemplate>(object.getSpawn()));
}

// Java RiftLocation.java:55-61
bool RiftLocation::replaceSpawned(int32_t oldObjectId, gameobjects::VisibleObject& newObject) {
	if (spawned.remove(oldObjectId)) {
		addSpawned(newObject);
		return true;
	}
	return false;
}

RiftLocation::~RiftLocation() = default;

} // namespace aion::gameserver::model::rift
