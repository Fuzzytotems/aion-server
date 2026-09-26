#include "aion/gameserver/services/reward/WebRewardService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::reward {

static const auto log = commons::logging::LoggerFactory::getLogger("WEB_REWARDS_LOG");

bool WebRewardService::MaxLevelReward::isPendingAscension(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool WebRewardService::MaxLevelReward::reward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void WebRewardService::MaxLevelReward::addBasicGear(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

WebRewardService& WebRewardService::getInstance() {
	static WebRewardService instance; // Java SingletonHolder
	return instance;
}

WebRewardService::WebRewardService() = default;

void WebRewardService::sendAvailableRewards(runtime::Ptr<model::gameobjects::player::Player> player) {
	AION_UNPORTED();
}

bool WebRewardService::sendRewardItem(model::gameobjects::player::Player& player, model::templates::rewards::RewardEntryItem& item) {
	AION_UNPORTED();
}

bool WebRewardService::executeRewardAction(model::gameobjects::player::Player& player, model::templates::rewards::RewardEntryItem& rewardItem) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::reward
