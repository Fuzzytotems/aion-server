#include "aion/gameserver/services/summons/TrapService.h"

#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::summons {

runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayDeque<runtime::Ref<model::gameobjects::Trap>>>> TrapService::registeredTraps{
	AION_LOCK_CLASS(TrapService::registeredTraps#stripe)};

void TrapService::registerTrap(int32_t ownerObjId, runtime::Ptr<model::gameobjects::Trap> trap, bool removeExcessTraps) {
	AION_UNPORTED();
}

void TrapService::unregisterTrap(int32_t trapObjId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::summons
