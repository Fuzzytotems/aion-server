#include "aion/gameserver/model/base/CasualBase.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/base/BaseLocation.h"

namespace aion::gameserver::model::base {

// Java CasualBase.java:11-13
CasualBase::CasualBase(BaseLocation& loc) : Base(loc) {
}

runtime::Ref<CasualBase> CasualBase::create(BaseLocation& loc) {
	return runtime::makeRef<CasualBase>(loc);
}

// Java CasualBase.java:15-20
int32_t CasualBase::getAssaultDelay() {
	if (getWorldId() == 600100000)
		return commons::utils::Rnd::get(300, 1200) * 6000;
	return commons::utils::Rnd::get(75, 200) * 6000;
}

// Java CasualBase.java:22-25
int32_t CasualBase::getAssaultDespawnDelay() {
	return 15 * 60000;
}

// Java CasualBase.java:27-32
int32_t CasualBase::getBossSpawnDelay() {
	if (getWorldId() == 600090000)
		return 0;
	return commons::utils::Rnd::get(100, 200) * 6000;
}

// Java CasualBase.java:34-37
int32_t CasualBase::getNpcSpawnDelay() {
	return commons::utils::Rnd::get(60, 295) * 1000;
}

CasualBase::~CasualBase() = default;

} // namespace aion::gameserver::model::base
