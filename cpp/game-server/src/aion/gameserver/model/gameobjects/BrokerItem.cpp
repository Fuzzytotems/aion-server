#include "aion/gameserver/model/gameobjects/BrokerItem.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects {

namespace {

/** Java String.compareTo: UTF-16 code unit order, then length */
int32_t javaStringCompareTo(const std::string& a, const std::string& b) {
	std::u16string left = commons::utils::StringUtils::toUtf16(a);
	std::u16string right = commons::utils::StringUtils::toUtf16(b);
	size_t limit = std::min(left.size(), right.size());
	for (size_t k = 0; k < limit; ++k) {
		if (left[k] != right[k])
			return static_cast<int32_t>(left[k]) - static_cast<int32_t>(right[k]);
	}
	return static_cast<int32_t>(left.size()) - static_cast<int32_t>(right.size());
}

commons::database::Timestamp nowTimestamp() {
	return commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()));
}

/** Java Timestamp.setNanos(0): drops the fractional seconds (db queries by this timestamp but doesn't store fractional seconds) */
commons::database::Timestamp withoutFractionalSeconds(commons::database::Timestamp timestamp) {
	return std::chrono::floor<std::chrono::seconds>(timestamp);
}

/** Java dereferences the Timestamp (`expireTime.setNanos(0)`), a NullPointerException for null */
commons::database::Timestamp requireTimestamp(const std::optional<commons::database::Timestamp>& timestamp) {
	if (!timestamp)
		throw runtime::NullPointerException("expireTime");
	return *timestamp;
}

} // namespace

// anonymous Comparator at BrokerItem.java:222 (BrokerItem$1)
const BrokerItem::Comparator BrokerItem::NAME_SORT_ASC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	return javaStringCompareTo(o1->getItemName(), o2->getItemName());
};

// anonymous Comparator at BrokerItem.java:232 (BrokerItem$2); Java sorts ascending here too
const BrokerItem::Comparator BrokerItem::NAME_SORT_DESC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	return javaStringCompareTo(o1->getItemName(), o2->getItemName());
};

// anonymous Comparator at BrokerItem.java:245 (BrokerItem$3)
const BrokerItem::Comparator BrokerItem::PRICE_SORT_ASC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	if (o1->getPrice() == o2->getPrice())
		return 0;
	return o1->getPrice() > o2->getPrice() ? 1 : -1;
};

// anonymous Comparator at BrokerItem.java:257 (BrokerItem$4)
const BrokerItem::Comparator BrokerItem::PRICE_SORT_DESC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	if (o1->getPrice() == o2->getPrice())
		return 0;
	return o1->getPrice() > o2->getPrice() ? -1 : 1;
};

// anonymous Comparator at BrokerItem.java:272 (BrokerItem$5)
const BrokerItem::Comparator BrokerItem::PIECE_PRICE_SORT_ASC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	if (o1->getPiecePrice() == o2->getPiecePrice())
		return 0;
	return o1->getPiecePrice() > o2->getPiecePrice() ? 1 : -1;
};

// anonymous Comparator at BrokerItem.java:284 (BrokerItem$6)
const BrokerItem::Comparator BrokerItem::PIECE_PRICE_SORT_DESC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	if (o1->getPiecePrice() == o2->getPiecePrice())
		return 0;
	return o1->getPiecePrice() > o2->getPiecePrice() ? -1 : 1;
};

// anonymous Comparator at BrokerItem.java:299 (BrokerItem$7)
const BrokerItem::Comparator BrokerItem::LEVEL_SORT_ASC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	if (o1->getItemLevel() == o2->getItemLevel())
		return 0;
	return o1->getItemLevel() > o2->getItemLevel() ? 1 : -1;
};

// anonymous Comparator at BrokerItem.java:311 (BrokerItem$8)
const BrokerItem::Comparator BrokerItem::LEVEL_SORT_DESC = [](runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2) -> int32_t {
	if (!o1 || !o2)
		return comparePossiblyNull(o1, o2);
	if (o1->getItemLevel() == o2->getItemLevel())
		return 0;
	return o1->getItemLevel() > o2->getItemLevel() ? -1 : 1;
};

BrokerItem::BrokerItem(Item& value, int64_t priceValue, int32_t sellerIdValue, bool splittingAvailableValue, broker::BrokerRace itemBrokerRaceValue)
	: item(runtime::Ref<Item>(value)), itemId(value.getItemTemplate()->getTemplateId()), itemUniqueId(value.getObjectId()),
	  itemCount(value.getItemCount()), itemCreator(value.getItemCreator()), price(priceValue), sellerId(sellerIdValue),
	  itemBrokerRace(itemBrokerRaceValue), isSold_(false), isSettled_(false),
	  expireTime(
		  withoutFractionalSeconds(nowTimestamp() + std::chrono::days(configs::main::CustomConfig::BROKER_REGISTRATION_EXPIRATION_DAYS.load()))),
	  settleTime(nowTimestamp()), splittingAvailable(splittingAvailableValue), state(PersistentState::NEW) {
}

runtime::Ref<BrokerItem> BrokerItem::create(Item& value, int64_t priceValue, int32_t sellerIdValue, bool splittingAvailableValue,
	broker::BrokerRace itemBrokerRaceValue) {
	return runtime::makeRef<BrokerItem>(value, priceValue, sellerIdValue, splittingAvailableValue, itemBrokerRaceValue);
}

BrokerItem::BrokerItem(runtime::Ptr<Item> value, int32_t itemIdValue, int32_t itemUniqueIdValue, int64_t itemCountValue, std::string_view itemCreatorValue,
	int64_t priceValue, int32_t sellerIdValue, broker::BrokerRace itemBrokerRaceValue, bool isSoldValue, bool isSettledValue,
	std::optional<commons::database::Timestamp> expireTimeValue, std::optional<commons::database::Timestamp> settleTimeValue,
	bool splittingAvailableValue)
	: item(value), itemId(itemIdValue), itemUniqueId(itemUniqueIdValue), itemCount(itemCountValue),
	  itemCreator(std::string(itemCreatorValue)), price(priceValue), sellerId(sellerIdValue), itemBrokerRace(itemBrokerRaceValue), isSold_(isSoldValue),
	  isSettled_(isSettledValue), expireTime(withoutFractionalSeconds(requireTimestamp(expireTimeValue))),
	  // the NOT NULL settle_time column always has a value (BrokerDAO); Java would keep a null
	  settleTime(settleTimeValue.value_or(commons::database::Timestamp{})), splittingAvailable(splittingAvailableValue),
	  state(PersistentState::NOACTION) {
}

runtime::Ref<BrokerItem> BrokerItem::create(runtime::Ptr<Item> value, int32_t itemIdValue, int32_t itemUniqueIdValue, int64_t itemCountValue,
	std::string_view itemCreatorValue, int64_t priceValue, int32_t sellerIdValue, broker::BrokerRace itemBrokerRaceValue, bool isSoldValue,
	bool isSettledValue, std::optional<commons::database::Timestamp> expireTimeValue, std::optional<commons::database::Timestamp> settleTimeValue,
	bool splittingAvailableValue) {
	return runtime::makeRef<BrokerItem>(value, itemIdValue, itemUniqueIdValue, itemCountValue, itemCreatorValue, priceValue, sellerIdValue,
		itemBrokerRaceValue, isSoldValue, isSettledValue, expireTimeValue, settleTimeValue, splittingAvailableValue);
}

std::string BrokerItem::getItemCreator() {
	return itemCreator; // Java: itemCreator == null ? "" : itemCreator (C++ strings are never null)
}

void BrokerItem::removeItem() {
	// this.item = null;
	isSold_.set(true);
	isSettled_.set(true);
	settleTime.set(nowTimestamp());
}

void BrokerItem::setPersistentState(Persistable::PersistentState persistentState) {
	// java-race: check-then-act with the DAO save thread (BrokerDAO stores the item and then sets UPDATED), so a change made in between is
	// marked saved until the next change
	switch (persistentState) {
		case PersistentState::DELETED:
			if (state.get() == PersistentState::NEW)
				state.set(PersistentState::NOACTION);
			else
				state.set(PersistentState::DELETED);
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (state.get() == PersistentState::NEW)
				break;
			[[fallthrough]];
		default:
			state.set(persistentState);
	}
}

void BrokerItem::setSettled() {
	isSettled_.set(true);
	settleTime.set(nowTimestamp());
}

void BrokerItem::decreaseItemCount(int64_t value) {
	itemCount -= value;
	item->decreaseItemCount(value);
}

int32_t BrokerItem::getItemLevel() {
	return item->getItemTemplate()->getLevel();
}

int64_t BrokerItem::getPiecePrice() {
	int64_t pieces = getItemCount();
	if (pieces == 0)
		throw commons::utils::ArithmeticException("/ by zero"); // Java long division
	return getPrice() / pieces;
}

std::string BrokerItem::getItemName() {
	return item->getItemName();
}

int32_t BrokerItem::compareTo(const BrokerItem& o) const {
	return itemUniqueId > o.getItemUniqueId() ? 1 : -1;
}

BrokerItem::Comparator BrokerItem::getComparatoryByType(int8_t sortType) {
	switch (sortType) {
		case 0:
			return NAME_SORT_ASC;
		case 1:
			return NAME_SORT_DESC;
		case 2:
			return LEVEL_SORT_ASC;
		case 3:
			return LEVEL_SORT_DESC;
		case 4:
			return PRICE_SORT_ASC;
		case 5:
			return PRICE_SORT_DESC;
		case 6:
			return PIECE_PRICE_SORT_ASC;
		case 7:
			return PIECE_PRICE_SORT_DESC;
		default:
			throw runtime::IllegalArgumentException("Illegal sort type for broker items");
	}
}

BrokerItem::~BrokerItem() = default;

} // namespace aion::gameserver::model::gameobjects
