#include "aion/gameserver/network/aion/serverpackets/SM_SECONDARY_SHOW_DECOMPOSABLE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/item/ResultedItem.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SECONDARY_SHOW_DECOMPOSABLE::SM_SECONDARY_SHOW_DECOMPOSABLE(int32_t objectIdValue,
	const std::vector<const model::templates::item::ResultedItem*>& itemsCollectionsValue)
	: AionServerPacket(opcodeOf<SM_SECONDARY_SHOW_DECOMPOSABLE>), itemsCollections(itemsCollectionsValue), objectId(objectIdValue) {
}

void SM_SECONDARY_SHOW_DECOMPOSABLE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
