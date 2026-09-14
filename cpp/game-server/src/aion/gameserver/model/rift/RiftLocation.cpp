#include "aion/gameserver/model/rift/RiftLocation.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/rift/RiftTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::model::rift {

RiftLocation::RiftLocation(const templates::rift::RiftTemplate* value)
	: template_(value) {
}

runtime::Ref<RiftLocation> RiftLocation::create(const templates::rift::RiftTemplate* value) {
	return runtime::makeRef<RiftLocation>(value);
}

int32_t RiftLocation::getId() {
	AION_UNPORTED();
}

int32_t RiftLocation::getWorldId() {
	AION_UNPORTED();
}

bool RiftLocation::hasSpawns() {
	AION_UNPORTED();
}

bool RiftLocation::isAutoCloseable() {
	AION_UNPORTED();
}

void RiftLocation::addSpawned(gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

bool RiftLocation::replaceSpawned(int32_t oldObjectId, gameobjects::VisibleObject& newObject) {
	AION_UNPORTED();
}

RiftLocation::~RiftLocation() = default;

} // namespace aion::gameserver::model::rift
