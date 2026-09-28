#include "aion/gameserver/spawnengine/WalkerFormationsCache.h"

#include "aion/gameserver/spawnengine/InstanceWalkerFormations.h"
#include "aion/gameserver/spawnengine/WorldWalkerFormations.h"

namespace aion::gameserver::spawnengine {

runtime::ConcurrentHashMap<int32_t, runtime::Ref<WorldWalkerFormations>> WalkerFormationsCache::formations{
	AION_LOCK_CLASS(WalkerFormationsCache::formations#stripe)};

runtime::Ptr<InstanceWalkerFormations> WalkerFormationsCache::getInstanceFormations(int32_t worldId, int32_t instanceId) {
	// Deviation (D6): Java's get-then-put lets two threads create two formations of one world, and the walker groups registered in the replaced
	// one are lost; computeIfAbsent creates exactly one
	runtime::Ptr<WorldWalkerFormations> wwf = formations.computeIfAbsent(worldId, [] { return WorldWalkerFormations::create(); });
	return wwf->getInstanceFormations(instanceId);
}

void WalkerFormationsCache::onInstanceDestroy(int32_t worldId, int32_t instanceId) {
	getInstanceFormations(worldId, instanceId)->onInstanceDestroy();
}

} // namespace aion::gameserver::spawnengine
