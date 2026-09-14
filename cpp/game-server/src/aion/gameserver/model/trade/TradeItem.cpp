#include "aion/gameserver/model/trade/TradeItem.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::trade {

TradeItem::TradeItem(int32_t value, int64_t countValue)
	: itemId(value), count(countValue) {
}

runtime::Ref<TradeItem> TradeItem::create(int32_t value, int64_t countValue) {
	return runtime::makeRef<TradeItem>(value, countValue);
}

const templates::item::ItemTemplate* TradeItem::getItemTemplate() {
	AION_UNPORTED();
}

TradeItem::~TradeItem() = default;

} // namespace aion::gameserver::model::trade
