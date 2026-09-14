#include "aion/gameserver/services/BonusPackService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

BonusPackService::BonusPackService() {
	AION_UNPORTED();
}

BonusPackService::~BonusPackService() = default;

BonusPackService& BonusPackService::getInstance() {
	static BonusPackService instance; // Java SingletonHolder
	return instance;
}

void BonusPackService::addPlayerCustomReward(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
