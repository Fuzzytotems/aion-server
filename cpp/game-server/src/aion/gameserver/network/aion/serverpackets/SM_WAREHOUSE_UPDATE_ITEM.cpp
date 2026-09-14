#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_UPDATE_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WAREHOUSE_UPDATE_ITEM::SM_WAREHOUSE_UPDATE_ITEM(model::gameobjects::player::Player& playerValue, model::gameobjects::Item& itemValue,
	int32_t warehouseTypeValue, services::item::ItemPacketService_ItemUpdateType updateTypeValue)
	: AionServerPacket(opcodeOf<SM_WAREHOUSE_UPDATE_ITEM>), player(playerValue), item(itemValue), warehouseType(warehouseTypeValue),
	  updateType(updateTypeValue) {
}

SM_WAREHOUSE_UPDATE_ITEM::~SM_WAREHOUSE_UPDATE_ITEM() = default;

void SM_WAREHOUSE_UPDATE_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
