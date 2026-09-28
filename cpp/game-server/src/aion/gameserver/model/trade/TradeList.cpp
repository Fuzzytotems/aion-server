#include "aion/gameserver/model/trade/TradeList.h"

#include <limits>
#include <optional>

#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/Acquisition.h"
#include "aion/gameserver/model/templates/item/AcquisitionType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/trade/TradeItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::trade {

namespace {

/** Java long multiplication and addition (two's complement wrap-around) */
constexpr int64_t javaMul(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

constexpr int64_t javaAdd(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

/** Java (int) cast of a double: NaN 0, saturating */
constexpr int32_t doubleToInt(double a) noexcept {
	if (a != a)
		return 0;
	if (a >= 2147483647.0)
		return std::numeric_limits<int32_t>::max();
	if (a <= -2147483648.0)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(a);
}

/** Java `tradeItem.getItemTemplate()` dereferenced by the price calculations (NullPointerException for an unknown item id) */
const templates::item::ItemTemplate& requireTemplate(TradeItem& tradeItem) {
	const templates::item::ItemTemplate* itemTemplate = tradeItem.getItemTemplate();
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("itemTemplate");
	return *itemTemplate;
}

} // namespace

TradeList::TradeList() : sellerObjId(0) {
}

TradeList::TradeList(int32_t sellerObjIdValue) : sellerObjId(sellerObjIdValue) {
}

TradeList::~TradeList() = default;

runtime::Ref<TradeList> TradeList::create() {
	return runtime::makeRef<TradeList>();
}

runtime::Ref<TradeList> TradeList::create(int32_t sellerObjIdValue) {
	return runtime::makeRef<TradeList>(sellerObjIdValue);
}

void TradeList::addItem(int32_t itemId, int64_t countValue) {
	addTradeItem(*TradeItem::create(itemId, countValue));
}

void TradeList::addTradeItem(TradeItem& tradeItem) {
	tradeItems.add(runtime::Ref<TradeItem>(tradeItem));
}

bool TradeList::calculateBuyListPrice(gameobjects::player::Player& player, int32_t modifier) {
	int64_t availableKinah = player.getInventory().getKinah();
	requiredKinah.set(0);

	for (runtime::Ptr<TradeItem> tradeItem : tradeItems) {
		int64_t price = services::trade::PricesService::getBuyPrice(requireTemplate(*tradeItem).getPrice(), player.getRace());
		requiredKinah.set(javaAdd(requiredKinah.get(), javaMul(javaMul(price, tradeItem->getCount()), modifier) / 100));
	}

	return availableKinah >= requiredKinah.get();
}

bool TradeList::calculateAbyssRewardBuyList(gameobjects::player::Player& player, int32_t modifier) {
	int32_t ap = player.getAbyssRank()->getAp();

	this->requiredAp.set(0);
	this->requiredItems.clear();

	for (runtime::Ptr<TradeItem> tradeItem : tradeItems) {
		const templates::item::Acquisition* aquisition = requireTemplate(*tradeItem).getAcquisition();
		if (aquisition == nullptr)
			continue;

		if (aquisition->getType() == templates::item::AcquisitionType::AP || aquisition->getType() == templates::item::AcquisitionType::ABYSS) {
			double apPrice = static_cast<double>(javaMul(javaMul(aquisition->getRequiredAp(), tradeItem->getCount()), modifier)) / 100.0;
			int32_t itemAp = doubleToInt(apPrice * services::trade::PricesService::getVendorBuyModifier()) / 100;
			requiredAp.set(static_cast<int32_t>(static_cast<uint32_t>(requiredAp.get()) + static_cast<uint32_t>(itemAp)));
		}

		int32_t rewardItemId = aquisition->getItemId();
		if (rewardItemId == 0) // no required item (medals, etc))
			continue;

		int64_t alreadyAddedCount = 0;
		if (requiredItems.containsKey(rewardItemId))
			alreadyAddedCount = requiredItems.get(rewardItemId).value();
		if (alreadyAddedCount == 0)
			requiredItems.put(rewardItemId, javaMul(aquisition->getItemCount(), tradeItem->getCount()));
		else
			requiredItems.put(rewardItemId, javaAdd(alreadyAddedCount, javaMul(aquisition->getItemCount(), tradeItem->getCount())));
	}

	if (ap < requiredAp.get()) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_ABYSSPOINT());
		return false;
	}

	for (int32_t itemId : requiredItems.keySet()) {
		int64_t inventoryCount = player.getInventory().getItemCountByItemId(itemId);
		std::optional<int64_t> requiredCount = requiredItems.get(itemId);
		if (!requiredCount) // Java: unboxing a null Long
			throw runtime::NullPointerException("requiredItems");
		if (*requiredCount < 1 || inventoryCount < *requiredCount)
			return false;
	}

	return true;
}

int32_t TradeList::size() {
	return tradeItems.size();
}

} // namespace aion::gameserver::model::trade
