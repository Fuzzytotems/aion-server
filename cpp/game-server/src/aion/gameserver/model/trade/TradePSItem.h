#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/trade/TradeItem.h"
#include "aion/gameserver/model/trade/fwd.h"

namespace aion::gameserver::model::trade {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Simple, Neon
 */
class TradePSItem : public TradeItem {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> itemObjId{};
	runtime::Field<int64_t> price{};

protected:
	TradePSItem(int32_t itemObjId, int32_t itemId, int64_t count, int64_t price);

public:
	static runtime::Ref<TradePSItem> create(int32_t value, int32_t itemIdValue, int64_t countValue, int64_t priceValue);

	void setPrice(int64_t value) { this->price.set(value); }

	int64_t getPrice() const { return this->price.get(); }

	void setItemObjId(int32_t value) { this->itemObjId.set(value); }

	int32_t getItemObjId() const { return this->itemObjId.get(); }

	/** Decreases the count only if it would really decrease and wouldn't become negative */
	void decreaseCount(int64_t decreaseCount);

protected:
	~TradePSItem() override;
};

} // namespace aion::gameserver::model::trade
