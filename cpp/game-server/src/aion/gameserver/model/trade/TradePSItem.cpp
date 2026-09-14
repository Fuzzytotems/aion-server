#include "aion/gameserver/model/trade/TradePSItem.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::trade {

TradePSItem::TradePSItem(int32_t value, int32_t itemIdValue, int64_t countValue, int64_t priceValue)
	: TradeItem(itemIdValue, countValue) {
	setPrice(priceValue);
	setItemObjId(value);
}

runtime::Ref<TradePSItem> TradePSItem::create(int32_t value, int32_t itemIdValue, int64_t countValue, int64_t priceValue) {
	return runtime::makeRef<TradePSItem>(value, itemIdValue, countValue, priceValue);
}

void TradePSItem::decreaseCount(int64_t decreaseCount) {
	AION_UNPORTED();
}

TradePSItem::~TradePSItem() = default;

} // namespace aion::gameserver::model::trade
