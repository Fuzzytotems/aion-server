#include "aion/gameserver/model/base/SiegeBase.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/base/SiegeBaseLocation.h"

namespace aion::gameserver::model::base {

// Java SiegeBase.java:12-14
SiegeBase::SiegeBase(SiegeBaseLocation& loc) : Base(loc) {
}

runtime::Ref<SiegeBase> SiegeBase::create(SiegeBaseLocation& loc) {
	return runtime::makeRef<SiegeBase>(loc);
}

runtime::Ptr<SiegeBaseLocation> SiegeBase::getLocation() const {
	return runtime::cast<SiegeBaseLocation>(Base::getLocation());
}

// Java SiegeBase.java:16-19
int32_t SiegeBase::getAssaultDelay() {
	return 180 * 60000;
}

// Java SiegeBase.java:21-24
int32_t SiegeBase::getAssaultDespawnDelay() {
	return 295 * 1000; // TODO: Dont Reassault with Despawntask
}

// Java SiegeBase.java:26-29
int32_t SiegeBase::getBossSpawnDelay() {
	return 0;
}

// Java SiegeBase.java:31-34
int32_t SiegeBase::getNpcSpawnDelay() {
	return 0;
}

SiegeBase::~SiegeBase() = default;

} // namespace aion::gameserver::model::base
