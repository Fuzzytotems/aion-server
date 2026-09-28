#include "aion/gameserver/network/aion/iteminfo/CompositeItemBlobEntry.h"

#include <unordered_map>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/ManaStone.h"

namespace aion::gameserver::network::aion::iteminfo {

CompositeItemBlobEntry::CompositeItemBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::COMPOSITE_ITEM) {
}

CompositeItemBlobEntry::~CompositeItemBlobEntry() = default;

runtime::Ref<CompositeItemBlobEntry> CompositeItemBlobEntry::create() {
	return runtime::makeRef<CompositeItemBlobEntry>();
}

void CompositeItemBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeD(buf, ownerItem->getFusionedItemId());
	writeFusionStones(buf);
	writeC(buf, ownerItem->getFusionedItemOptionalSockets()); // additional manastone sockets
	writeC(buf, ownerItem->getFusionedItemBonusStatsId());
}

int32_t CompositeItemBlobEntry::getSize() {
	return model::gameobjects::Item::MAX_BASIC_STONES * 4 + 6;
}

void CompositeItemBlobEntry::writeFusionStones(commons::utils::ByteBuffer& buf) {
	runtime::Ptr<model::gameobjects::Item> item = ownerItem.get();
	if (item->hasFusionStones()) {
		std::unordered_map<int32_t, runtime::Ptr<model::items::ManaStone>> stonesBySlot;
		for (runtime::Ptr<model::items::ManaStone> itemStone : *item->getFusionStones()) {
			stonesBySlot.insert_or_assign(itemStone->getSlot(), itemStone);
		}
		for (int32_t i = 0; i < model::gameobjects::Item::MAX_BASIC_STONES; i++) {
			auto stone = stonesBySlot.find(i);
			writeD(buf, stone == stonesBySlot.end() ? 0 : stone->second->getItemId());
		}
	} else {
		skip(buf, model::gameobjects::Item::MAX_BASIC_STONES * 4);
	}
}

} // namespace aion::gameserver::network::aion::iteminfo
