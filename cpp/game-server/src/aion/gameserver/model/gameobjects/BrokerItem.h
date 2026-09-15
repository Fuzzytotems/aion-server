#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/broker/fwd.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Java implements Comparable<BrokerItem>.
 *
 * @author kosyachok
 */
class BrokerItem : public runtime::RefCounted, public Persistable {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<Item> item;
	const int32_t itemId;
	const int32_t itemUniqueId;
	runtime::Field<int64_t> itemCount{};
	const std::string itemCreator;
	const int64_t price;
	const int32_t sellerId;
	const broker::BrokerRace itemBrokerRace;
	runtime::Field<bool> isSold_{};
	runtime::Field<bool> isCanceled_{};
	runtime::Field<bool> isSettled_{};
	const commons::database::Timestamp expireTime;
	runtime::Field<commons::database::Timestamp> settleTime{};
	const bool splittingAvailable;
	runtime::Field<int64_t> averagePrice{};

public:
	runtime::Field<Persistable::PersistentState> state{};

	/**
	 * Java `Comparator<BrokerItem>` (compare(o1, o2); both may be null). C++: a std::function over nullable borrows, used with
	 * std::ranges::sort by BrokerService.
	 */
	using Comparator = std::function<int32_t(runtime::Ptr<BrokerItem> o1, runtime::Ptr<BrokerItem> o2)>;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator NAME_SORT_ASC;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator NAME_SORT_DESC;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator PRICE_SORT_ASC;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator PRICE_SORT_DESC;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator PIECE_PRICE_SORT_ASC;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator PIECE_PRICE_SORT_DESC;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator LEVEL_SORT_ASC;

	// fieldmap.toml: static comparator constant, never reassigned (anonymous Comparator, defined in BrokerItem.cpp)
	static const Comparator LEVEL_SORT_DESC;

protected:
	BrokerItem(Item& item, int64_t price, int32_t sellerId, bool splittingAvailable, broker::BrokerRace itemBrokerRace);

public:
	static runtime::Ref<BrokerItem> create(Item& value, int64_t priceValue, int32_t sellerIdValue, bool splittingAvailableValue,
		broker::BrokerRace itemBrokerRaceValue);

protected:
	BrokerItem(Item& item, int32_t itemId, int32_t itemUniqueId, int64_t itemCount, std::string_view itemCreator, int64_t price, int32_t sellerId,
		broker::BrokerRace itemBrokerRace, bool isSold, bool isSettled, std::optional<commons::database::Timestamp> expireTime,
		std::optional<commons::database::Timestamp> settleTime, bool splittingAvailable);

public:
	static runtime::Ref<BrokerItem> create(Item& value, int32_t itemIdValue, int32_t itemUniqueIdValue, int64_t itemCountValue,
		std::string_view itemCreatorValue, int64_t priceValue, int32_t sellerIdValue, broker::BrokerRace itemBrokerRaceValue, bool isSoldValue,
		bool isSettledValue, std::optional<commons::database::Timestamp> expireTimeValue, std::optional<commons::database::Timestamp> settleTimeValue,
		bool splittingAvailableValue);

	std::string getItemCreator();

	runtime::Ptr<Item> getItem() const { return this->item; }

	bool isCanceled() const { return this->isCanceled_.get(); }

	void setIsCanceled(bool value) { this->isCanceled_.set(value); }

	void removeItem();

	int32_t getItemId() const { return this->itemId; }

	int32_t getItemUniqueId() const { return this->itemUniqueId; }

	int64_t getPrice() const { return this->price; }

	int32_t getSellerId() const { return this->sellerId; }

	broker::BrokerRace getItemBrokerRace() const { return this->itemBrokerRace; }

	bool isSold() const { return this->isSold_.get(); }

	void setPersistentState(Persistable::PersistentState persistentState) override;

	Persistable::PersistentState getPersistentState() override { return this->state.get(); }

	bool isSettled() const { return this->isSettled_.get(); }

	void setSettled();

	std::optional<commons::database::Timestamp> getExpireTime() const { return this->expireTime; }

	std::optional<commons::database::Timestamp> getSettleTime() const { return this->settleTime.get(); }

	int64_t getItemCount() const { return this->itemCount.get(); }

	void decreaseItemCount(int64_t value);

private:
	int32_t getItemLevel();

	int64_t getPiecePrice();

	std::string getItemName();

public:
	bool isSplittingAvailable() const { return this->splittingAvailable; }

	int64_t getAveragePrice() const { return this->averagePrice.get(); }

	void setAveragePrice(int64_t value) { this->averagePrice.set(value); }

	/** Default sorting: using itemUniqueId */
	int32_t compareTo(const BrokerItem& o) const;

private:
	template <class T>
	static int32_t comparePossiblyNull(T aThis, T aThat) {
		int32_t result = 0;
		if (aThis == nullptr && aThat != nullptr) {
			result = -1;
		} else if (aThis != nullptr && aThat == nullptr) {
			result = 1;
		}
		return result;
	}

public:
	/**
	 * 1 - by name;<br>
	 * 2 - by level;<br>
	 * 4 - by totalPrice;<br>
	 * 6 - by price for piece (Math.round(item.getPrice() / item.getItemCount))<br>
	 */
	static Comparator getComparatoryByType(int8_t sortType);
protected:
	~BrokerItem() override;
};

} // namespace aion::gameserver::model::gameobjects
