#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"

#include "aion/gameserver/model/instance/InstanceProgressionTypeInfo.h"
#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"

namespace aion::gameserver::model::instance::instancescore {

InstanceScore::InstanceScore() {
}

runtime::Ref<InstanceScore> InstanceScore::create() {
	return runtime::makeRef<InstanceScore>();
}

// Java InstanceScore.java:18-20
std::vector<runtime::Ptr<playerreward::InstancePlayerReward>> InstanceScore::getPlayerRewards() {
	return playerRewards.values().toVector(); // Java: the live values() view; C++ a snapshot
}

// Java InstanceScore.java:22-24
bool InstanceScore::containsPlayer(int32_t objectId) {
	return playerRewards.containsKey(objectId);
}

// Java InstanceScore.java:26-28
void InstanceScore::removePlayerReward(playerreward::InstancePlayerReward& reward) {
	playerRewards.remove(reward.getOwnerId());
}

// Java InstanceScore.java:30-32
runtime::Ptr<playerreward::InstancePlayerReward> InstanceScore::getPlayerReward(int32_t objectId) {
	return playerRewards.get(objectId);
}

// Java InstanceScore.java:34-36
void InstanceScore::addPlayerReward(playerreward::InstancePlayerReward& reward) {
	playerRewards.put(reward.getOwnerId(), runtime::Ref<playerreward::InstancePlayerReward>(reward));
}

// Java InstanceScore.java:46-48
bool InstanceScore::isRewarded() {
	return isEndProgress(instanceProgressionType.get());
}

// Java InstanceScore.java:50-52
bool InstanceScore::isReinforcing() {
	return instance::isReinforcing(instanceProgressionType.get());
}

// Java InstanceScore.java:54-56
bool InstanceScore::isPreparing() {
	return instance::isPreparing(instanceProgressionType.get());
}

// Java InstanceScore.java:58-60
bool InstanceScore::isStartProgress() {
	return instance::isStartProgress(instanceProgressionType.get());
}

// Java InstanceScore.java:62-64
void InstanceScore::clear() {
	playerRewards.clear();
}

InstanceScore::~InstanceScore() = default;

} // namespace aion::gameserver::model::instance::instancescore
