#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/worldraid/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/services/worldraid/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Whoop, Sykra
 */
class WorldRaidService : public runtime::Immortal {
private:
	runtime::Field<runtime::Ref<runtime::RcHashMap<int32_t, const model::templates::worldraid::WorldRaidLocation*>>> raidLocationsById{};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<worldraid::WorldRaid>> activeRaids{AION_LOCK_CLASS(WorldRaidService::activeRaids#stripe)};
	runtime::ConcurrentHashMap<int32_t, int64_t> lastMsgDateByMapId{AION_LOCK_CLASS(WorldRaidService::lastMsgDateByMapId#stripe)};
	WorldRaidService();
public:
	void initWorldRaidLocations();
	void initWorldRaids();
	std::vector<const model::templates::worldraid::WorldRaidLocation*> getActiveWorldRaidLocations();
	bool isValidWorldRaidLocation(int32_t locationId);
	bool isWorldRaidInProgress(int32_t locationId);
	void startRaid(int32_t locationId, bool useSpecialSpawnMsg);
	void stopRaid(int32_t locationId);
	static WorldRaidService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
