#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PRIVATE_STORE::SM_PRIVATE_STORE(runtime::Ptr<model::gameobjects::player::PrivateStore> storeValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_PRIVATE_STORE>), player(playerValue), store(storeValue) {
}

SM_PRIVATE_STORE::~SM_PRIVATE_STORE() = default;

void SM_PRIVATE_STORE::writeImpl(AionConnection* con) {
	if (store != nullptr) {
		model::gameobjects::player::Player& seller = store->getOwner();
		const auto soldItems = store->getSoldItems()->snapshot();
		writeD(seller.getObjectId());
		writeH(static_cast<int32_t>(soldItems.size()));
		for (const auto& entry : soldItems) { // Java soldItems.values() of the LinkedHashMap
			runtime::Ptr<model::trade::TradePSItem> tradeItem = entry.value;
			writeD(tradeItem->getItemObjId());
			writeD(tradeItem->getItemId());
			writeH(static_cast<int32_t>(tradeItem->getCount()));
			writeQ(tradeItem->getPrice());
			runtime::Ptr<model::gameobjects::Item> item = seller.getInventory().getItemByObjId(tradeItem->getItemObjId());
			iteminfo::ItemInfoBlob::getFullBlob(player, *item)->writeMe(getBuf());
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
