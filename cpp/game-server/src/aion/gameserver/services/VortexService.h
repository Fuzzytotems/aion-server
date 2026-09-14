#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/vortex/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/services/vortex/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Source
 */
class VortexService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<vortex::DimensionalVortex>> activeInvasions{
		AION_LOCK_CLASS(VortexService::activeInvasions#stripe)};
public:
	void initVortexLocations();
	void startInvasion(int32_t id);
	void stopInvasion(int32_t id);
	void spawn(model::vortex::VortexLocation& loc, model::vortex::VortexStateType state);
	void despawn(model::vortex::VortexLocation& loc);
	bool isInvasionInProgress(int32_t id);
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<vortex::DimensionalVortex>>& getActiveInvasions() { return this->activeInvasions; }
	int32_t getDuration();
	void removeDefenderPlayer(model::gameobjects::player::Player& player);
	void removeInvaderPlayer(model::gameobjects::player::Player& player);
	bool isInvaderPlayer(model::gameobjects::player::Player& player);
	bool isInsideVortexZone(model::gameobjects::player::Player& player);
	runtime::Ptr<model::vortex::VortexLocation> getLocationByRift(int32_t npcId);
	runtime::Ptr<model::vortex::VortexLocation> getLocationByWorld(int32_t worldId);
	static VortexService& getInstance(); // Java singleton (VortexServiceHolder)
};

} // namespace aion::gameserver::services
