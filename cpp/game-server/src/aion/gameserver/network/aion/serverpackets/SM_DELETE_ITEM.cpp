#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE_ITEM::SM_DELETE_ITEM(int32_t itemObjectIdValue)
	: SM_DELETE_ITEM(itemObjectIdValue, services::item::ItemPacketService_ItemDeleteType::DEFAULT) {
}

SM_DELETE_ITEM::SM_DELETE_ITEM(int32_t itemObjectIdValue, services::item::ItemPacketService_ItemDeleteType deleteTypeValue)
	: AionServerPacket(opcodeOf<SM_DELETE_ITEM>), itemObjectId(itemObjectIdValue), deleteType(deleteTypeValue) {
}

void SM_DELETE_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
