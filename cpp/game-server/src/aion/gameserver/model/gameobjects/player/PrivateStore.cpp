#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

PrivateStore::PrivateStore(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
	items.set(runtime::RcLinkedHashMap<int32_t, runtime::Ref<trade::TradePSItem>>::create(AION_LOCK_CLASS(PrivateStore::items)));
}

PrivateStore::~PrivateStore() = default;

void PrivateStore::addItemToSell(int32_t itemObjId, trade::TradePSItem& tradeItem) {
	AION_UNPORTED();
}

void PrivateStore::removeItem(int32_t itemObjId) {
	AION_UNPORTED();
}

runtime::Ptr<trade::TradePSItem> PrivateStore::getTradeItemByObjId(int32_t itemObjId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
