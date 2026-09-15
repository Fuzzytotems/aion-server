#include "aion/gameserver/network/aion/serverpackets/SM_SECONDARY_SHOW_DECOMPOSABLE.h"

#include "aion/gameserver/model/templates/item/ResultedItem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SECONDARY_SHOW_DECOMPOSABLE::SM_SECONDARY_SHOW_DECOMPOSABLE(int32_t objectIdValue,
	const std::vector<const model::templates::item::ResultedItem*>& itemsCollectionsValue)
	: AionServerPacket(opcodeOf<SM_SECONDARY_SHOW_DECOMPOSABLE>), itemsCollections(itemsCollectionsValue), objectId(objectIdValue) {
}

void SM_SECONDARY_SHOW_DECOMPOSABLE::writeImpl(AionConnection* con) {
	writeD(objectId);
	writeD(0);
	writeC(static_cast<int32_t>(itemsCollections.size()));
	int32_t index = 0;
	for (const model::templates::item::ResultedItem* item : itemsCollections) {
		writeC(index);
		writeD(item->getItemId());
		writeD(item->getMinCount());
		writeC(0);
		writeC(0);
		writeC(0);
		writeC(1);
		index++;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
