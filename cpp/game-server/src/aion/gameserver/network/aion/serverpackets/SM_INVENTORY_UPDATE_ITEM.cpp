#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INVENTORY_UPDATE_ITEM::SM_INVENTORY_UPDATE_ITEM(model::gameobjects::player::Player& playerValue, model::gameobjects::Item& itemValue)
	: SM_INVENTORY_UPDATE_ITEM(playerValue, itemValue, services::item::ItemPacketService_ItemUpdateType::DEC_ITEM_USE) {
}

SM_INVENTORY_UPDATE_ITEM::SM_INVENTORY_UPDATE_ITEM(model::gameobjects::player::Player& playerValue, model::gameobjects::Item& itemValue,
	services::item::ItemPacketService_ItemUpdateType updateTypeValue)
	: AionServerPacket(opcodeOf<SM_INVENTORY_UPDATE_ITEM>), player(playerValue), item(itemValue), updateType(updateTypeValue) {
}

SM_INVENTORY_UPDATE_ITEM::~SM_INVENTORY_UPDATE_ITEM() = default;

void SM_INVENTORY_UPDATE_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
