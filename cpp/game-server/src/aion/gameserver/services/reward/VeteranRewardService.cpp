#include "aion/gameserver/services/reward/VeteranRewardService.h"

#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::reward {

runtime::ArrayList<runtime::Ref<runtime::RcArrayList<runtime::Ref<model::templates::rewards::RewardItem>>>> VeteranRewardService::rewards{
	AION_LOCK_CLASS(VeteranRewardService::rewards)};
runtime::ArrayList<runtime::Ref<model::templates::rewards::RewardItem>> VeteranRewardService::randomRewards{
	AION_LOCK_CLASS(VeteranRewardService::randomRewards)};

VeteranRewardService::VeteranRewardService() = default;

VeteranRewardService& VeteranRewardService::getInstance() {
	static VeteranRewardService instance; // Java SingletonHolder
	return instance;
}

void VeteranRewardService::tryReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::reward
