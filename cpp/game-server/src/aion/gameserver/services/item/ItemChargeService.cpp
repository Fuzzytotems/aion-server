#include "aion/gameserver/services/item/ItemChargeService.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/item/Improvement.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::item {

namespace {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::items::ChargeInfo;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

/** Java: the anonymous RequestResponseHandler<Player> of startChargingEquippedItems (ItemChargeService.java:46-54, fieldmap key ItemChargeService$1) */
class ItemChargeService_RequestResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const Ref<Player> player;                                  // captured param Player player
	const Ref<runtime::RcArrayList<Ref<Item>>> filteredItems; // captured local Collection<Item> filteredItems
	const int32_t chargeWay;                                   // captured param int chargeWay
	const int64_t payAmount;                                   // captured local long payAmount

	static Ref<ItemChargeService_RequestResponseHandler> create(Player& player, Ref<runtime::RcArrayList<Ref<Item>>> filteredItems, int32_t chargeWay,
		int64_t payAmount) {
		return runtime::makeRef<ItemChargeService_RequestResponseHandler>(player, std::move(filteredItems), chargeWay, payAmount);
	}

	void acceptRequest(Ptr<model::gameobjects::Creature> /*requester*/, Player& /*responder*/) override {
		if (ItemChargeService::processPayment(*player, chargeWay, payAmount)) {
			std::vector<Ptr<Item>> items;
			for (Ptr<Item> item : *filteredItems)
				items.push_back(item);
			ItemChargeService::chargeItems(*player, items, 2, false, false);
		}
	}

protected:
	ItemChargeService_RequestResponseHandler(Player& playerValue, Ref<runtime::RcArrayList<Ref<Item>>> filteredItemsValue, int32_t chargeWayValue, int64_t payAmountValue)
		: RequestResponseHandler(Ptr<model::gameobjects::Creature>(playerValue)), player(Ref<Player>(playerValue)), filteredItems(std::move(filteredItemsValue)),
		  chargeWay(chargeWayValue), payAmount(payAmountValue) {}
	~ItemChargeService_RequestResponseHandler() override = default;
};

} // namespace

// Java ItemChargeService.java:26-32
std::vector<runtime::Ptr<model::gameobjects::Item>> ItemChargeService::filterItemsToCondition(model::gameobjects::player::Player& player,
	runtime::Ptr<model::gameobjects::Item> selectedItem, int32_t chargeWay) {
	if (selectedItem != nullptr)
		return {selectedItem};
	std::vector<Ptr<Item>> result;
	for (const Ptr<Item>& item : player.getEquipment().getEquippedItems()) {
		if (item->calculateAvailableChargeLevel(player) != 0 && item->getImprovement() != nullptr && item->getImprovement()->getChargeWay() == chargeWay
			&& item->getChargePoints() < ChargeInfo::LEVEL2)
			result.push_back(item);
	}
	return result;
}

// Java ItemChargeService.java:34-58
void ItemChargeService::startChargingEquippedItems(model::gameobjects::player::Player& player, int32_t senderObj, int32_t chargeWay) {
	// TODO: Check this : SM_QUESTION_WINDOW.STR_ITEM_CHARGE_CONFIRM_SOME_ALREADY_CHARGED !!!
	const std::vector<Ptr<Item>> filteredItems = filterItemsToCondition(player, nullptr, chargeWay);
	if (filteredItems.empty()) {
		if (chargeWay == 1)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_ALL_FAIL_NO_CHARGEABLE_EQUIPMENT());
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE2_ALL_FAIL_NO_CHARGEABLE_EQUIPMENT());
		return;
	}

	const int64_t payAmount = calculatePrice(filteredItems, player);
	auto captured = runtime::RcArrayList<Ref<Item>>::create();
	for (const Ptr<Item>& item : filteredItems)
		captured->add(Ref<Item>(*item));
	Ref<ItemChargeService_RequestResponseHandler> request = ItemChargeService_RequestResponseHandler::create(player, std::move(captured), chargeWay, payAmount);
	int32_t msg = chargeWay == 1 ? SM_QUESTION_WINDOW::STR_ITEM_CHARGE_ALL_CONFIRM : SM_QUESTION_WINDOW::STR_ITEM_CHARGE2_ALL_CONFIRM;
	if (player.getResponseRequester().putRequest(msg, request))
		PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(msg, senderObj, 0, std::to_string(payAmount)));
}

// Java ItemChargeService.java:60-65
int64_t ItemChargeService::calculatePrice(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, model::gameobjects::player::Player& player) {
	int64_t result = 0;
	for (const Ptr<Item>& item : items) // Java long +=, wrapping
		result = static_cast<int64_t>(static_cast<uint64_t>(result) + static_cast<uint64_t>(getPayAmountForService(*item, item->calculateAvailableChargeLevel(player))));
	return result;
}

// Java ItemChargeService.java:67-85
void ItemChargeService::chargeItems(model::gameobjects::player::Player& player, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t maxLevel,
	bool ignoreRankRequirement, bool requirePayment) {
	if (items.empty())
		return;
	// Java: a HashSet<Integer>(2); with the charge ways 1 and 2 it iterates them in ascending order, as the std::set does
	std::set<int32_t> chargeWays;
	bool itemsUpdated = false;
	for (const Ptr<Item>& item : items) {
		if (chargeItem(player, *item, maxLevel, ignoreRankRequirement, requirePayment)) {
			itemsUpdated = true;
			chargeWays.insert(item->getImprovement()->getChargeWay());
		}
	}
	if (!itemsUpdated)
		return;
	for (int32_t chargeWay : chargeWays) {
		if (chargeWay == 1)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_ALL_COMPLETE());
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE2_ALL_COMPLETE());
	}
}

// Java ItemChargeService.java:87-111
bool ItemChargeService::chargeItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t maxLevel, bool ignoreRankRequirement,
	bool requirePayment) {
	const model::templates::item::Improvement* improvement = item.getImprovement();
	if (improvement == nullptr)
		return false;

	int32_t level = ignoreRankRequirement ? maxLevel : calculateMaxChargeLevelBasedOnRank(player, item, maxLevel);
	if (level <= 0)
		return false;
	int32_t maxChargePoints = level == 1 ? ChargeInfo::LEVEL1 : ChargeInfo::LEVEL2;
	int32_t chargePointsToAdd = std::max(0, static_cast<int32_t>(static_cast<uint32_t>(maxChargePoints) - static_cast<uint32_t>(item.getChargePoints())));
	// process payment if needed
	if (chargePointsToAdd <= 0 || (requirePayment && !processPayment(player, item, level)))
		return false;

	if (item.getConditioningInfo()->updateChargePoints(chargePointsToAdd))
		PacketSendUtility::sendPacket(player, SM_INVENTORY_UPDATE_ITEM(player, item, ItemPacketService::ItemUpdateType::CHARGE));

	if (improvement->getChargeWay() == 1) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE_SUCCESS(item.getL10n(), level));
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_CHARGE2_SUCCESS(item.getL10n(), level));
	}
	player.getGameStats()->updateStatsVisually();
	return true;
}

// Java ItemChargeService.java:113-115
bool ItemChargeService::processPayment(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t level) {
	const model::templates::item::Improvement* improvement = item.getImprovement();
	if (improvement == nullptr) // Java: item.getImprovement().getChargeWay() on null
		throw runtime::NullPointerException("Item.getImprovement()");
	return processPayment(player, improvement->getChargeWay(), getPayAmountForService(item, level));
}

// Java ItemChargeService.java:117-123
bool ItemChargeService::processPayment(model::gameobjects::player::Player& player, int32_t chargeWay, int64_t amount) {
	switch (chargeWay) {
		case 1:
			return processKinahPayment(player, amount);
		case 2:
			return processAPPayment(player, amount);
		default:
			return false;
	}
}

// Java ItemChargeService.java:125-127
bool ItemChargeService::processKinahPayment(model::gameobjects::player::Player& player, int64_t requiredKinah) {
	return player.getInventory().tryDecreaseKinah(requiredKinah);
}

// Java ItemChargeService.java:129-134
bool ItemChargeService::processAPPayment(model::gameobjects::player::Player& player, int64_t requiredAP) {
	if (player.getAbyssRank()->getAp() < requiredAP)
		return false;
	// Java: (int) -requiredAP - the long negated (wrapping), then narrowed
	abyss::AbyssPointsService::addAp(player, static_cast<int32_t>(static_cast<uint32_t>(0ULL - static_cast<uint64_t>(requiredAP))));
	return true;
}

// Java ItemChargeService.java:136-163
int64_t ItemChargeService::getPayAmountForService(model::gameobjects::Item& item, int32_t chargeLevel) {
	const model::templates::item::Improvement* improvement = item.getImprovement();
	if (improvement == nullptr)
		return 0;
	int32_t price1 = improvement->getPrice1();
	int32_t price2 = improvement->getPrice2();
	double firstLevel = price1 / 2.0;
	// Java: Math.round(firstLevel + (price2 - price1) / 2d) - an int subtraction (wrapping), the long result widened to double
	double updateLevel =
		static_cast<double>(utils::JavaMath::round(firstLevel + static_cast<int32_t>(static_cast<uint32_t>(price2) - static_cast<uint32_t>(price1)) / 2.0));
	double money = 0;
	float currentChargeRatio = 1.0F;
	switch (chargeLevel) {
		case 1:
			currentChargeRatio -= (static_cast<float>(item.getChargePoints()) / static_cast<float>(ChargeInfo::LEVEL1));
			money = std::ceil(firstLevel * currentChargeRatio);
			break;
		case 2:
			switch (getNextChargeLevel(item)) {
				case 1: {
					// full
					currentChargeRatio -= ((static_cast<float>(item.getChargePoints()) / static_cast<float>(ChargeInfo::LEVEL1)));
					money = std::ceil(firstLevel * currentChargeRatio) + updateLevel;
					break;
				}
				case 2: {
					// update
					currentChargeRatio -= ((static_cast<float>(item.getChargePoints() - ChargeInfo::LEVEL1) /
						static_cast<float>(ChargeInfo::LEVEL2 - ChargeInfo::LEVEL1)));
					money = std::ceil(updateLevel * currentChargeRatio);
					break;
				}
				default:
					break;
			}
			break;
		default:
			break;
	}
	return std::max<int64_t>(0, utils::JavaMath::doubleToLong(money));
}

// Java ItemChargeService.java:165-172
int32_t ItemChargeService::getNextChargeLevel(model::gameobjects::Item& item) {
	int32_t charge = item.getChargePoints();
	if (charge < ChargeInfo::LEVEL1)
		return 1;
	if (charge < ChargeInfo::LEVEL2)
		return 2;
	throw runtime::IllegalArgumentException("Invalid charge level " + std::to_string(charge));
}

// Java ItemChargeService.java:174-176
int32_t ItemChargeService::calculateMaxChargeLevelBasedOnRank(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t maxChargeLevel) {
	return std::min(item.calculateAvailableChargeLevel(player), maxChargeLevel);
}

} // namespace aion::gameserver::services::item
