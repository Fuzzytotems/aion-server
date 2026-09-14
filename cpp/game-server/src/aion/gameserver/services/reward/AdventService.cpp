#include "aion/gameserver/services/reward/AdventService.h"

#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::reward {

AdventService::AdventService() {
	AION_UNPORTED();
}

AdventService::~AdventService() = default;

AdventService& AdventService::getInstance() {
	static AdventService instance; // Java SingletonHolder
	return instance;
}

void AdventService::addReward(int32_t day, int32_t itemId, int64_t itemCount) {
	AION_UNPORTED();
}

void AdventService::onLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AdventService::isAdventSeason() {
	AION_UNPORTED();
}

bool AdventService::isAdventSeason(commons::database::Date date) {
	AION_UNPORTED();
}

void AdventService::redeemReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void AdventService::showTodaysReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::reward
