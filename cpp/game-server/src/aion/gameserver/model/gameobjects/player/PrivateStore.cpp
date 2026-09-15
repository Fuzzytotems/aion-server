#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/trade/TradePSItem.h"

namespace aion::gameserver::model::gameobjects::player {

PrivateStore::PrivateStore(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
	items.set(runtime::RcLinkedHashMap<int32_t, runtime::Ref<trade::TradePSItem>>::create(AION_LOCK_CLASS(PrivateStore::items)));
}

PrivateStore::~PrivateStore() = default;

void PrivateStore::addItemToSell(int32_t itemObjId, trade::TradePSItem& tradeItem) {
	items->put(itemObjId, runtime::Ref<trade::TradePSItem>(tradeItem));
}

void PrivateStore::removeItem(int32_t itemObjId) {
	runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<trade::TradePSItem>>> current = items.get();
	if (current->containsKey(itemObjId)) {
		runtime::Ref<runtime::RcLinkedHashMap<int32_t, runtime::Ref<trade::TradePSItem>>> newItems =
			runtime::RcLinkedHashMap<int32_t, runtime::Ref<trade::TradePSItem>>::create(AION_LOCK_CLASS(PrivateStore::items));
		for (const auto& entry : current->snapshot()) {
			if (itemObjId != entry.key)
				newItems->put(entry.key, runtime::Ref<trade::TradePSItem>(entry.value));
		}
		items.set(newItems);
	}
}

runtime::Ptr<trade::TradePSItem> PrivateStore::getTradeItemByObjId(int32_t itemObjId) {
	return items->get(itemObjId);
}

} // namespace aion::gameserver::model::gameobjects::player
