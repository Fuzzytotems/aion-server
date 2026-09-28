#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/assemblednpc/fwd.h"

namespace aion::gameserver::model::assemblednpc {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class AssembledNpc : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<AssembledNpcPart>> assembledPatrs{AION_LOCK_CLASS(AssembledNpc::assembledPatrs)}; // Java: = new ArrayList<>()
	const int64_t spawnTime; // Java: = System.currentTimeMillis()
	const int32_t routeId;
	const int32_t mapId;

protected:
	AssembledNpc(int32_t routeId, int32_t mapId, int32_t liveTime, const std::vector<runtime::Ptr<AssembledNpcPart>>& assembledPatrs);

public:
	static runtime::Ref<AssembledNpc> create(int32_t value, int32_t mapIdValue, int32_t liveTime,
		const std::vector<runtime::Ptr<AssembledNpcPart>>& assembledPatrsValue);

	runtime::ArrayList<runtime::Ref<AssembledNpcPart>>& getAssembledParts() { return this->assembledPatrs; }

	int32_t getRouteId() const { return this->routeId; }

	int32_t getMapId() const { return this->mapId; }

	int64_t getTimeOnMap();

protected:
	~AssembledNpc() override;
};

} // namespace aion::gameserver::model::assemblednpc
