#include "aion/gameserver/model/assemblednpc/AssembledNpc.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"

namespace aion::gameserver::model::assemblednpc {

AssembledNpc::AssembledNpc(int32_t value, int32_t mapIdValue, int32_t liveTime,
	const std::vector<runtime::Ptr<AssembledNpcPart>>& assembledPatrsValue)
	: spawnTime(commons::utils::currentTimeMillis()), routeId(value), mapId(mapIdValue) {
	// Java: this.assembledPatrs = assembledPatrs
	AION_UNPORTED();
}

runtime::Ref<AssembledNpc> AssembledNpc::create(int32_t value, int32_t mapIdValue, int32_t liveTime,
	const std::vector<runtime::Ptr<AssembledNpcPart>>& assembledPatrsValue) {
	return runtime::makeRef<AssembledNpc>(value, mapIdValue, liveTime, assembledPatrsValue);
}

int64_t AssembledNpc::getTimeOnMap() {
	AION_UNPORTED();
}

AssembledNpc::~AssembledNpc() = default;

} // namespace aion::gameserver::model::assemblednpc
