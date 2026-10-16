#include "aion/gameserver/model/limiteditems/LimitedItem.h"


namespace aion::gameserver::model::limiteditems {

LimitedItem::LimitedItem(int32_t value, int32_t sellLimitValue, int32_t buyLimitValue, std::string_view salesTimeValue)
	: itemId(value), sellLimit(sellLimitValue), buyLimit(buyLimitValue), defaultSellLimit(sellLimitValue), salesTime(std::string(salesTimeValue)) {
}

runtime::Ref<LimitedItem> LimitedItem::create(int32_t value, int32_t sellLimitValue, int32_t buyLimitValue, std::string_view salesTimeValue) {
	return runtime::makeRef<LimitedItem>(value, sellLimitValue, buyLimitValue, salesTimeValue);
}

void LimitedItem::setBuyCount(int32_t playerObjectId, int32_t value) {
	buyCounts.put(playerObjectId, value);
}

int32_t LimitedItem::getBuyCount(int32_t playerObjectId) {
	return buyCounts.getOrDefault(playerObjectId, 0);
}

void LimitedItem::setToDefault() {
	sellLimit.set(defaultSellLimit);
	buyCounts.clear();
}

LimitedItem::~LimitedItem() = default;

} // namespace aion::gameserver::model::limiteditems
