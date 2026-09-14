#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::instance::playerreward {

InstancePlayerReward::InstancePlayerReward(int32_t value)
	: objectId(value) {
}

runtime::Ref<InstancePlayerReward> InstancePlayerReward::create(int32_t value) {
	return runtime::makeRef<InstancePlayerReward>(value);
}

void InstancePlayerReward::addPoints(int32_t value) {
	AION_UNPORTED();
}

void InstancePlayerReward::addPvPKill() {
	AION_UNPORTED();
}

void InstancePlayerReward::addMonsterKillToPlayer() {
	AION_UNPORTED();
}

InstancePlayerReward::~InstancePlayerReward() = default;

} // namespace aion::gameserver::model::instance::playerreward
