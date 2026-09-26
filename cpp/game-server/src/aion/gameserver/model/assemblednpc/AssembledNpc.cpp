#include "aion/gameserver/model/assemblednpc/AssembledNpc.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"

namespace aion::gameserver::model::assemblednpc {

AssembledNpc::AssembledNpc(int32_t value, int32_t mapIdValue, int32_t liveTime,
	const std::vector<runtime::Ptr<AssembledNpcPart>>& assembledPatrsValue)
	: spawnTime(commons::utils::currentTimeMillis()), routeId(value), mapId(mapIdValue) {
	// Java stores the caller's list (BalaurAssaultService builds it for this object only); the C++ member is the shim, filled in list order
	for (runtime::Ptr<AssembledNpcPart> part : assembledPatrsValue)
		assembledPatrs.add(runtime::Ref<AssembledNpcPart>(part));
}

runtime::Ref<AssembledNpc> AssembledNpc::create(int32_t value, int32_t mapIdValue, int32_t liveTime,
	const std::vector<runtime::Ptr<AssembledNpcPart>>& assembledPatrsValue) {
	return runtime::makeRef<AssembledNpc>(value, mapIdValue, liveTime, assembledPatrsValue);
}

int64_t AssembledNpc::getTimeOnMap() {
	return commons::utils::currentTimeMillis() - spawnTime;
}

AssembledNpc::~AssembledNpc() = default;

} // namespace aion::gameserver::model::assemblednpc
