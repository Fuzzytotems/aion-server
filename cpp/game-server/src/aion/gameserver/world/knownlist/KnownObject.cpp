#include "aion/gameserver/world/knownlist/KnownObject.h"

#include <string>
#include <typeinfo>

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::world::knownlist {

KnownObject::KnownObject(model::gameobjects::VisibleObject& objectValue) : object(objectValue) {
}

KnownObject::~KnownObject() = default;

runtime::Ref<KnownObject> KnownObject::create(model::gameobjects::VisibleObject& objectValue) {
	return runtime::makeRef<KnownObject>(objectValue);
}

bool KnownObject::updateVisible(bool value) {
	SYNCHRONIZED(*this) {
		if (visible.get() != value) {
			visible.set(value);
			return true;
		}
	}
	return false;
}

bool KnownObject::equals(const KnownObject& o) const {
	// Java: getClass() == o.getClass() && Objects.equals(object, that.object); object is never null
	return typeid(*this) == typeid(o) && object->equals(*o.object);
}

int32_t KnownObject::hashCode() const {
	return object->hashCode(); // Java: Objects.hashCode(object)
}

std::string KnownObject::toString() {
	return object->getName() + " (objectId: " + std::to_string(object->getObjectId()) + ", visible: " + (visible.get() ? "true" : "false") + ")";
}

} // namespace aion::gameserver::world::knownlist
