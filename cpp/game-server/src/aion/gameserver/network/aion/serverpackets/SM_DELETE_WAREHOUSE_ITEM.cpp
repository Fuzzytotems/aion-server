#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_WAREHOUSE_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE_WAREHOUSE_ITEM::SM_DELETE_WAREHOUSE_ITEM(int32_t warehouseTypeValue, int32_t itemObjIdValue,
	services::item::ItemPacketService_ItemDeleteType deleteTypeValue)
	: AionServerPacket(opcodeOf<SM_DELETE_WAREHOUSE_ITEM>), warehouseType(warehouseTypeValue), itemObjId(itemObjIdValue),
	  deleteType(deleteTypeValue) {
}

void SM_DELETE_WAREHOUSE_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
