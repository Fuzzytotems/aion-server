#include "aion/gameserver/model/trade/ExchangeItem.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::model::trade {

ExchangeItem::ExchangeItem(int32_t value, int64_t itemCountValue, gameobjects::Item& itemValue)
	: itemObjId(value), itemCount(itemCountValue), item(runtime::Ref<gameobjects::Item>(itemValue)) {
}

runtime::Ref<ExchangeItem> ExchangeItem::create(int32_t value, int64_t itemCountValue, gameobjects::Item& itemValue) {
	return runtime::makeRef<ExchangeItem>(value, itemCountValue, itemValue);
}

void ExchangeItem::setItem(runtime::Ptr<gameobjects::Item> value) {
	this->item.set(value);
}

void ExchangeItem::addCount(int64_t countToAdd) {
	AION_UNPORTED();
}

ExchangeItem::~ExchangeItem() = default;

} // namespace aion::gameserver::model::trade
