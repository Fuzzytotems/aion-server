#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_ADD_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WAREHOUSE_ADD_ITEM::SM_WAREHOUSE_ADD_ITEM(model::gameobjects::Item& item, int32_t warehouseTypeValue,
	model::gameobjects::player::Player& playerValue, services::item::ItemPacketService_ItemAddType addTypeValue)
	: AionServerPacket(opcodeOf<SM_WAREHOUSE_ADD_ITEM>), warehouseType(warehouseTypeValue), player(playerValue), addType(addTypeValue) {
	items.emplace_back(item); // Java: Collections.singletonList(item)
}

SM_WAREHOUSE_ADD_ITEM::~SM_WAREHOUSE_ADD_ITEM() = default;

void SM_WAREHOUSE_ADD_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_WAREHOUSE_ADD_ITEM::writeItemInfo(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
