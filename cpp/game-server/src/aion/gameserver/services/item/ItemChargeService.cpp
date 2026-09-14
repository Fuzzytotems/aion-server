#include "aion/gameserver/services/item/ItemChargeService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::item {

std::vector<runtime::Ptr<model::gameobjects::Item>> ItemChargeService::filterItemsToCondition(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> selectedItem, int32_t chargeWay) {
	AION_UNPORTED();
}

// anonymous RequestResponseHandler at ItemChargeService.java:46 (fieldmap key ItemChargeService$1); local request; storage: stored in ResponseRequester
void ItemChargeService::startChargingEquippedItems(model::gameobjects::player::Player& player, int32_t senderObj, int32_t chargeWay) {
	AION_UNPORTED();
}

int64_t ItemChargeService::calculatePrice(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ItemChargeService::chargeItems(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t maxLevel, bool ignoreRankRequirement, bool requirePayment) {
	AION_UNPORTED();
}

bool ItemChargeService::chargeItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t maxLevel, bool ignoreRankRequirement, bool requirePayment) {
	AION_UNPORTED();
}

bool ItemChargeService::processPayment(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t level) {
	AION_UNPORTED();
}

bool ItemChargeService::processPayment(model::gameobjects::player::Player& player, int32_t chargeWay, int64_t amount) {
	AION_UNPORTED();
}

bool ItemChargeService::processKinahPayment(model::gameobjects::player::Player& player, int64_t requiredKinah) {
	AION_UNPORTED();
}

bool ItemChargeService::processAPPayment(model::gameobjects::player::Player& player, int64_t requiredAP) {
	AION_UNPORTED();
}

int64_t ItemChargeService::getPayAmountForService(model::gameobjects::Item& item, int32_t chargeLevel) {
	AION_UNPORTED();
}

int32_t ItemChargeService::getNextChargeLevel(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

int32_t ItemChargeService::calculateMaxChargeLevelBasedOnRank(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t maxChargeLevel) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
