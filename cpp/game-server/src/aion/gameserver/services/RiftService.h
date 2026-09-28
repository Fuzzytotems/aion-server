#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/configs/schedule/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/rift/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author Source
 */
class RiftService : public runtime::Immortal {
private:
	runtime::Field<const configs::schedule::RiftSchedule*> schedule{};
	runtime::Field<runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<model::rift::RiftLocation>>>> locations{};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::rift::RiftLocation>> activeRifts{AION_LOCK_CLASS(RiftService::activeRifts#stripe)};
public:
	void initRiftLocations();
	void initRifts();
	bool isValidId(int32_t id);
private:
	bool isRift(int32_t id);
public:
	bool openRifts(int32_t id, bool guards);
	bool closeRifts(int32_t id);
	/** Just a work-around, this stuff needs refactoring */
	void prepareRiftOpening(int32_t id, bool guards);
	void openRifts(model::rift::RiftLocation& location, bool isWithGuards);
	void closeRift(model::rift::RiftLocation& location);
	bool isRiftOpened(int32_t riftId);
	void closeAutoCloseableRifts(int32_t worldId);
	void updateSpawned(int32_t oldObjectId, model::gameobjects::VisibleObject& respawn);
	int32_t getDuration();
	runtime::Ptr<model::rift::RiftLocation> getRiftLocation(int32_t id);
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<model::rift::RiftLocation>>> getRiftLocations() const { return this->locations.get(); }
	static RiftService& getInstance(); // Java singleton (RiftServiceHolder)
};

} // namespace aion::gameserver::services
