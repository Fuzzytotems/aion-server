#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"

namespace aion::gameserver::model::instance::playerreward {

// Java CruciblePlayerReward.java:14-16
CruciblePlayerReward::CruciblePlayerReward(int32_t objectId) : InstancePlayerReward(objectId) {
}

CruciblePlayerReward::~CruciblePlayerReward() = default;

runtime::Ref<CruciblePlayerReward> CruciblePlayerReward::create(int32_t objectId) {
	return runtime::makeRef<CruciblePlayerReward>(objectId);
}

} // namespace aion::gameserver::model::instance::playerreward
