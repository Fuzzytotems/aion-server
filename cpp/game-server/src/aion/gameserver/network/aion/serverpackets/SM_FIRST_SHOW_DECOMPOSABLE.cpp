#include "aion/gameserver/network/aion/serverpackets/SM_FIRST_SHOW_DECOMPOSABLE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/templates/item/ResultedItem.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FIRST_SHOW_DECOMPOSABLE::SM_FIRST_SHOW_DECOMPOSABLE(int32_t objectIdValue,
	const std::vector<const model::templates::item::ResultedItem*>& itemsCollectionsValue)
	: AionServerPacket(opcodeOf<SM_FIRST_SHOW_DECOMPOSABLE>), itemsCollections(itemsCollectionsValue), objectId(objectIdValue) {
}

void SM_FIRST_SHOW_DECOMPOSABLE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
