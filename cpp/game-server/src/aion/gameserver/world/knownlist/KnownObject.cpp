#include "aion/gameserver/world/knownlist/KnownObject.h"

#include <typeinfo>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"

namespace aion::gameserver::world::knownlist {

KnownObject::KnownObject(model::gameobjects::VisibleObject& objectValue) : object(objectValue) {
}

KnownObject::~KnownObject() = default;

runtime::Ref<KnownObject> KnownObject::create(model::gameobjects::VisibleObject& objectValue) {
	return runtime::makeRef<KnownObject>(objectValue);
}

bool KnownObject::updateVisible(bool value) {
	AION_UNPORTED();
}

bool KnownObject::equals(const KnownObject& o) const {
	// Java: getClass() == o.getClass() && Objects.equals(object, that.object); object is never null
	return typeid(*this) == typeid(o) && object->equals(*o.object);
}

int32_t KnownObject::hashCode() const {
	return object->hashCode(); // Java: Objects.hashCode(object)
}

std::string KnownObject::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::world::knownlist
