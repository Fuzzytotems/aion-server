#include "aion/gameserver/services/RepurchaseService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::services {

RepurchaseService::RepurchaseService() = default;

void RepurchaseService::addRepurchaseItems(model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	AION_UNPORTED();
}

void RepurchaseService::removeRepurchaseItems(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

std::unordered_set<runtime::Ptr<model::gameobjects::Item>> RepurchaseService::getRepurchaseItems(int32_t playerObjectId) {
	AION_UNPORTED();
}

bool RepurchaseService::canRepurchase(model::gameobjects::player::Player& player, int32_t itemObjectId) {
	AION_UNPORTED();
}

void RepurchaseService::repurchaseFromShop(model::gameobjects::player::Player& player, model::trade::RepurchaseList& repurchaseList) {
	AION_UNPORTED();
}

RepurchaseService& RepurchaseService::getInstance() {
	static RepurchaseService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
