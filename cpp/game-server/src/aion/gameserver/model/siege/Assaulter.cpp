#include "aion/gameserver/model/siege/Assaulter.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::siege {

Assaulter::Assaulter(int32_t value, float spawnCostValue, int32_t headingOffsetValue, int32_t distanceOffsetValue)
	: npcId(value), spawnCost(spawnCostValue), headingOffset(headingOffsetValue), distanceOffset(distanceOffsetValue) {
}

runtime::Ref<Assaulter> Assaulter::create(int32_t value, float spawnCostValue, int32_t headingOffsetValue, int32_t distanceOffsetValue) {
	return runtime::makeRef<Assaulter>(value, spawnCostValue, headingOffsetValue, distanceOffsetValue);
}

Assaulter::~Assaulter() = default;

} // namespace aion::gameserver::model::siege
