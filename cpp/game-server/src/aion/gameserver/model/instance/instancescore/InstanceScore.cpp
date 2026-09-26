#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"

namespace aion::gameserver::model::instance::instancescore {

InstanceScore::InstanceScore() {
}

runtime::Ref<InstanceScore> InstanceScore::create() {
	return runtime::makeRef<InstanceScore>();
}

std::vector<runtime::Ptr<playerreward::InstancePlayerReward>> InstanceScore::getPlayerRewards() {
	AION_UNPORTED();
}

bool InstanceScore::containsPlayer(int32_t objectId) {
	AION_UNPORTED();
}

void InstanceScore::removePlayerReward(playerreward::InstancePlayerReward& reward) {
	AION_UNPORTED();
}

runtime::Ptr<playerreward::InstancePlayerReward> InstanceScore::getPlayerReward(int32_t objectId) {
	AION_UNPORTED();
}

void InstanceScore::addPlayerReward(playerreward::InstancePlayerReward& reward) {
	AION_UNPORTED();
}

bool InstanceScore::isRewarded() {
	AION_UNPORTED();
}

bool InstanceScore::isReinforcing() {
	AION_UNPORTED();
}

bool InstanceScore::isPreparing() {
	AION_UNPORTED();
}

bool InstanceScore::isStartProgress() {
	AION_UNPORTED();
}

void InstanceScore::clear() {
	AION_UNPORTED();
}

InstanceScore::~InstanceScore() = default;

} // namespace aion::gameserver::model::instance::instancescore
