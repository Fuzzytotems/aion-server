#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * The walker formations of the instances of one world map.
 * <p>
 * C++: RefCounted (fieldmap K4, WalkerFormationsCache.formations), created with create().
 *
 * @author Rolandas
 */
class WorldWalkerFormations : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<InstanceWalkerFormations>> formations{AION_LOCK_CLASS(WorldWalkerFormations::formations#stripe)};

protected:
	WorldWalkerFormations();
	~WorldWalkerFormations() override;

public:
	/** Java: new WorldWalkerFormations() */
	static runtime::Ref<WorldWalkerFormations> create();

	/** Java protected */
	runtime::Ptr<InstanceWalkerFormations> getInstanceFormations(int32_t instanceId);
};

} // namespace aion::gameserver::spawnengine
