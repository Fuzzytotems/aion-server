#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * The walker formations of every world map instance.
 * <p>
 * C++: a static-only class (Java package-private; the members are public, hub-headers.md §9.1). The map is defined in the .cpp (its element
 * type is incomplete here).
 *
 * @author Rolandas
 */
class WalkerFormationsCache {
private:
	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<WorldWalkerFormations>> formations;

public:
	WalkerFormationsCache() = delete;

	/** Java protected */
	static runtime::Ptr<InstanceWalkerFormations> getInstanceFormations(int32_t worldId, int32_t instanceId);

	/** Java protected */
	static void onInstanceDestroy(int32_t worldId, int32_t instanceId);
};

} // namespace aion::gameserver::spawnengine
