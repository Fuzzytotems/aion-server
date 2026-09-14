#include "aion/gameserver/services/reward/StarterKitService.h"

#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::reward {

StarterKitService& StarterKitService::getInstance() {
	static StarterKitService instance; // Java SingletonHolder
	return instance;
}

StarterKitService::StarterKitService() {
	AION_UNPORTED();
}

void StarterKitService::onLevelUp(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::reward
