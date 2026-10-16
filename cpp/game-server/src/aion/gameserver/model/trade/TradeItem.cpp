#include "aion/gameserver/model/trade/TradeItem.h"

#include "aion/gameserver/model/items/detail/StaticDataLookups.h"

namespace aion::gameserver::model::trade {

TradeItem::TradeItem(int32_t value, int64_t countValue)
	: itemId(value), count(countValue) {
}

runtime::Ref<TradeItem> TradeItem::create(int32_t value, int64_t countValue) {
	return runtime::makeRef<TradeItem>(value, countValue);
}

const templates::item::ItemTemplate* TradeItem::getItemTemplate() {
	return items::detail::getItemTemplate(itemId);
}

TradeItem::~TradeItem() = default;

} // namespace aion::gameserver::model::trade
