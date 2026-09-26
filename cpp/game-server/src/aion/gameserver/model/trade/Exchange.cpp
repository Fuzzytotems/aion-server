#include "aion/gameserver/model/trade/Exchange.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/trade/ExchangeItem.h"

namespace aion::gameserver::model::trade {

Exchange::Exchange(gameobjects::player::Player& value, gameobjects::player::Player& targetPlayerValue)
	: activeplayer(runtime::Ref<gameobjects::player::Player>(value)), targetPlayer(runtime::Ref<gameobjects::player::Player>(targetPlayerValue)) {
}

runtime::Ref<Exchange> Exchange::create(gameobjects::player::Player& value, gameobjects::player::Player& targetPlayerValue) {
	return runtime::makeRef<Exchange>(value, targetPlayerValue);
}

void Exchange::confirm() {
	confirmed.set(true);
}

void Exchange::lock() {
	locked.set(true);
}

void Exchange::addItem(int32_t parentItemObjId, ExchangeItem& exchangeItem) {
	this->items.put(parentItemObjId, runtime::Ref<ExchangeItem>(exchangeItem));
}

void Exchange::addKinah(int64_t countToAdd) {
	this->kinahCount += countToAdd;
}

bool Exchange::isExchangeListFull() {
	return items.size() >= 18;
}

Exchange::~Exchange() = default;

} // namespace aion::gameserver::model::trade
