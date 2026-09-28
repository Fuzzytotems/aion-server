#include "aion/gameserver/model/gameobjects/AionObject.h"

#include <string>
#include <typeinfo>

#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::model::gameobjects {

AionObject::AionObject(int32_t objId) : AionObject(objId, false) {
}

AionObject::AionObject(int32_t objId, bool autoReleaseObjectIdValue) : objectId(objId), autoReleaseObjectId(objId != 0 && autoReleaseObjectIdValue) {
}

AionObject::~AionObject() {
	if (autoReleaseObjectId) // Java: CLEANER.register(this, () -> { if (!RespawnService.setAutoReleaseId(objId)) IDFactory.releaseId(objId); })
		runtime::CleanerQueue::push(objectId);
}

std::string AionObject::toString() {
	return utils::simpleClassName(typeid(*this)) + " [name=" + getName() + ", objectId=" + std::to_string(objectId) + "]";
}

} // namespace aion::gameserver::model::gameobjects
