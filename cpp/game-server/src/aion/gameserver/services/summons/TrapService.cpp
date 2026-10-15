#include "aion/gameserver/services/summons/TrapService.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Trap.h"

namespace aion::gameserver::services::summons {

runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayDeque<runtime::Ref<model::gameobjects::Trap>>>> TrapService::registeredTraps{
	AION_LOCK_CLASS(TrapService::registeredTraps#stripe)};

void TrapService::registerTrap(int32_t ownerObjId, runtime::Ptr<model::gameobjects::Trap> trap, bool removeExcessTraps) {
	if (!trap || trap->isDead())
		return;
	runtime::Ptr<runtime::RcArrayDeque<runtime::Ref<model::gameobjects::Trap>>> traps = registeredTraps.computeIfAbsent(ownerObjId,
		[](int32_t /*objId*/) { return runtime::RcArrayDeque<runtime::Ref<model::gameobjects::Trap>>::create(); });
	traps->offer(runtime::Ref<model::gameobjects::Trap>(trap));
	if (removeExcessTraps) {
		while (!traps->isEmpty() && traps->size() > TRAP_LIMIT_PER_OWNER) {
			runtime::Ptr<model::gameobjects::Trap> firstPlacedTrap = traps->poll();
			if (firstPlacedTrap)
				firstPlacedTrap->getController().delete_();
		}
	}
}

void TrapService::unregisterTrap(int32_t trapObjId) {
	// Java: registeredTraps.values(), then removeIf on every queue and on the values view
	registeredTraps.forEach([trapObjId](const int32_t& /*ownerObjId*/, const auto& traps) {
		traps->removeIf([trapObjId](const runtime::Ref<model::gameobjects::Trap>& trap) { return trap->getObjectId() == trapObjId; });
	});
	registeredTraps.removeIf([](const int32_t& /*ownerObjId*/, const auto& traps) { return traps->isEmpty(); });
}

} // namespace aion::gameserver::services::summons
