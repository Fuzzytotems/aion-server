#include "aion/gameserver/model/gameobjects/AionObject.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"

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
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
