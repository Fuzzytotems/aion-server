#include "aion/gameserver/spawnengine/WorldWalkerFormations.h"

#include "aion/gameserver/spawnengine/InstanceWalkerFormations.h"

namespace aion::gameserver::spawnengine {

WorldWalkerFormations::WorldWalkerFormations() = default;

WorldWalkerFormations::~WorldWalkerFormations() = default;

runtime::Ref<WorldWalkerFormations> WorldWalkerFormations::create() {
	return runtime::makeRef<WorldWalkerFormations>();
}

runtime::Ptr<InstanceWalkerFormations> WorldWalkerFormations::getInstanceFormations(int32_t instanceId) {
	// Deviation (D6): Java's get-then-put can replace a formation another thread just created; computeIfAbsent creates exactly one
	return formations.computeIfAbsent(instanceId, [] { return InstanceWalkerFormations::create(); });
}

} // namespace aion::gameserver::spawnengine
