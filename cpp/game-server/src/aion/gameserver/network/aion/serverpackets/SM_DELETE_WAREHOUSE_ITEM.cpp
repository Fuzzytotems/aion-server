#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_WAREHOUSE_ITEM.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE_WAREHOUSE_ITEM::SM_DELETE_WAREHOUSE_ITEM(int32_t warehouseTypeValue, int32_t itemObjIdValue,
	services::item::ItemPacketService_ItemDeleteType deleteTypeValue)
	: AionServerPacket(opcodeOf<SM_DELETE_WAREHOUSE_ITEM>), warehouseType(warehouseTypeValue), itemObjId(itemObjIdValue),
	  deleteType(deleteTypeValue) {
}

void SM_DELETE_WAREHOUSE_ITEM::writeImpl(AionConnection* con) {
	writeC(warehouseType);
	writeD(itemObjId);
	writeC(detail::itemDeleteTypeMask(deleteType));
}

} // namespace aion::gameserver::network::aion::serverpackets
