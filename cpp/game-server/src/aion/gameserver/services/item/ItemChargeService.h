#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class ItemChargeService {
public:
	static std::vector<runtime::Ptr<model::gameobjects::Item>> filterItemsToCondition(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> selectedItem, int32_t chargeWay);
	static void startChargingEquippedItems(model::gameobjects::player::Player& player, int32_t senderObj, int32_t chargeWay);
private:
	static int64_t calculatePrice(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, model::gameobjects::player::Player& player);
public:
	static void chargeItems(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t maxLevel, bool ignoreRankRequirement, bool requirePayment);
	static bool chargeItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t maxLevel, bool ignoreRankRequirement, bool requirePayment);
	static bool processPayment(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t level);
	static bool processPayment(model::gameobjects::player::Player& player, int32_t chargeWay, int64_t amount);
	static bool processKinahPayment(model::gameobjects::player::Player& player, int64_t requiredKinah);
	static bool processAPPayment(model::gameobjects::player::Player& player, int64_t requiredAP);
	static int64_t getPayAmountForService(model::gameobjects::Item& item, int32_t chargeLevel);
private:
	static int32_t getNextChargeLevel(model::gameobjects::Item& item);
public:
	static int32_t calculateMaxChargeLevelBasedOnRank(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t maxChargeLevel);
};

} // namespace aion::gameserver::services::item
