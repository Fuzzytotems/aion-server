#include "aion/gameserver/model/gameobjects/BrokerItem.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::model::gameobjects {

// anonymous Comparator at BrokerItem.java:222 (BrokerItem$1)
const BrokerItem::Comparator BrokerItem::NAME_SORT_ASC = [](runtime::Ptr<BrokerItem>, runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

// anonymous Comparator at BrokerItem.java:232 (BrokerItem$2)
const BrokerItem::Comparator BrokerItem::NAME_SORT_DESC = [](runtime::Ptr<BrokerItem>, runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

// anonymous Comparator at BrokerItem.java:245 (BrokerItem$3)
const BrokerItem::Comparator BrokerItem::PRICE_SORT_ASC = [](runtime::Ptr<BrokerItem>, runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

// anonymous Comparator at BrokerItem.java:257 (BrokerItem$4)
const BrokerItem::Comparator BrokerItem::PRICE_SORT_DESC = [](runtime::Ptr<BrokerItem>, runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

// anonymous Comparator at BrokerItem.java:272 (BrokerItem$5)
const BrokerItem::Comparator BrokerItem::PIECE_PRICE_SORT_ASC = [](runtime::Ptr<BrokerItem>,
	runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

// anonymous Comparator at BrokerItem.java:284 (BrokerItem$6)
const BrokerItem::Comparator BrokerItem::PIECE_PRICE_SORT_DESC = [](runtime::Ptr<BrokerItem>,
	runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

// anonymous Comparator at BrokerItem.java:299 (BrokerItem$7)
const BrokerItem::Comparator BrokerItem::LEVEL_SORT_ASC = [](runtime::Ptr<BrokerItem>, runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

// anonymous Comparator at BrokerItem.java:311 (BrokerItem$8)
const BrokerItem::Comparator BrokerItem::LEVEL_SORT_DESC = [](runtime::Ptr<BrokerItem>, runtime::Ptr<BrokerItem>) -> int32_t { AION_UNPORTED(); };

BrokerItem::BrokerItem(Item& value, int64_t priceValue, int32_t sellerIdValue, bool splittingAvailableValue, broker::BrokerRace itemBrokerRaceValue)
	: item(runtime::Ref<Item>(value)), itemId(), itemUniqueId(), itemCreator(), price(priceValue), sellerId(sellerIdValue),
	  itemBrokerRace(itemBrokerRaceValue), isSold_(false), isSettled_(false), expireTime(), splittingAvailable(splittingAvailableValue) {
	// Java: this.itemId = item.getItemTemplate().getTemplateId(); this.itemUniqueId = item.getObjectId(); this.itemCount = item.getItemCount();
	// this.itemCreator = item.getItemCreator(); this.expireTime = new Timestamp(System.currentTimeMillis() +
	// TimeUnit.DAYS.toMillis(CustomConfig.BROKER_REGISTRATION_EXPIRATION_DAYS)); this.expireTime.setNanos(0); this.settleTime = new
	// Timestamp(System.currentTimeMillis()); this.state = PersistentState.NEW
	AION_UNPORTED();
}

runtime::Ref<BrokerItem> BrokerItem::create(Item& value, int64_t priceValue, int32_t sellerIdValue, bool splittingAvailableValue,
	broker::BrokerRace itemBrokerRaceValue) {
	return runtime::makeRef<BrokerItem>(value, priceValue, sellerIdValue, splittingAvailableValue, itemBrokerRaceValue);
}

BrokerItem::BrokerItem(Item& value, int32_t itemIdValue, int32_t itemUniqueIdValue, int64_t itemCountValue, std::string_view itemCreatorValue,
	int64_t priceValue, int32_t sellerIdValue, broker::BrokerRace itemBrokerRaceValue, bool isSoldValue, bool isSettledValue,
	std::optional<commons::database::Timestamp> expireTimeValue, std::optional<commons::database::Timestamp> settleTimeValue,
	bool splittingAvailableValue)
	: item(runtime::Ref<Item>(value)), itemId(itemIdValue), itemUniqueId(itemUniqueIdValue), itemCount(itemCountValue),
	  itemCreator(std::string(itemCreatorValue)), price(priceValue), sellerId(sellerIdValue), itemBrokerRace(itemBrokerRaceValue), isSold_(isSoldValue),
	  isSettled_(isSettledValue), expireTime(), splittingAvailable(splittingAvailableValue) {
	// Java: this.expireTime = expireTime; this.expireTime.setNanos(0); this.settleTime = settleTime; this.state = PersistentState.NOACTION
	AION_UNPORTED();
}

runtime::Ref<BrokerItem> BrokerItem::create(Item& value, int32_t itemIdValue, int32_t itemUniqueIdValue, int64_t itemCountValue,
	std::string_view itemCreatorValue, int64_t priceValue, int32_t sellerIdValue, broker::BrokerRace itemBrokerRaceValue, bool isSoldValue,
	bool isSettledValue, std::optional<commons::database::Timestamp> expireTimeValue, std::optional<commons::database::Timestamp> settleTimeValue,
	bool splittingAvailableValue) {
	return runtime::makeRef<BrokerItem>(value, itemIdValue, itemUniqueIdValue, itemCountValue, itemCreatorValue, priceValue, sellerIdValue,
		itemBrokerRaceValue, isSoldValue, isSettledValue, expireTimeValue, settleTimeValue, splittingAvailableValue);
}

std::string BrokerItem::getItemCreator() {
	AION_UNPORTED();
}

void BrokerItem::removeItem() {
	AION_UNPORTED();
}

void BrokerItem::setPersistentState(Persistable::PersistentState persistentState) {
	AION_UNPORTED();
}

void BrokerItem::setSettled() {
	AION_UNPORTED();
}

void BrokerItem::decreaseItemCount(int64_t value) {
	AION_UNPORTED();
}

int32_t BrokerItem::getItemLevel() {
	AION_UNPORTED();
}

int64_t BrokerItem::getPiecePrice() {
	AION_UNPORTED();
}

std::string BrokerItem::getItemName() {
	AION_UNPORTED();
}

int32_t BrokerItem::compareTo(const BrokerItem& o) const {
	AION_UNPORTED();
}

BrokerItem::Comparator BrokerItem::getComparatoryByType(int8_t sortType) {
	AION_UNPORTED();
}

BrokerItem::~BrokerItem() = default;

} // namespace aion::gameserver::model::gameobjects
