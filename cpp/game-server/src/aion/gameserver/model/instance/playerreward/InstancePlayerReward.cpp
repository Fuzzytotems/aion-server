#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"

namespace aion::gameserver::model::instance::playerreward {

InstancePlayerReward::InstancePlayerReward(int32_t value)
	: objectId(value) {
}

runtime::Ref<InstancePlayerReward> InstancePlayerReward::create(int32_t value) {
	return runtime::makeRef<InstancePlayerReward>(value);
}

// Java InstancePlayerReward.java:33-35 (not synchronized: a read and a write, as Java's +=)
void InstancePlayerReward::addPoints(int32_t value) {
	this->points.set(this->points.get() + value);
}

// Java InstancePlayerReward.java:41-43
void InstancePlayerReward::addPvPKill() {
	playerPvPKills.set(playerPvPKills.get() + 1);
}

// Java InstancePlayerReward.java:45-47
void InstancePlayerReward::addMonsterKillToPlayer() {
	playerMonsterKills.set(playerMonsterKills.get() + 1);
}

InstancePlayerReward::~InstancePlayerReward() = default;

} // namespace aion::gameserver::model::instance::playerreward
