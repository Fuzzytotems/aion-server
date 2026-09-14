#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INVENTORY_ADD_ITEM::SM_INVENTORY_ADD_ITEM(const std::vector<runtime::Ptr<model::gameobjects::Item>>& itemsValue,
	model::gameobjects::player::Player& playerValue, services::item::ItemPacketService_ItemAddType addTypeValue)
	: AionServerPacket(opcodeOf<SM_INVENTORY_ADD_ITEM>), items(itemsValue.begin(), itemsValue.end()), player(playerValue), addType(addTypeValue) {
}

SM_INVENTORY_ADD_ITEM::~SM_INVENTORY_ADD_ITEM() = default;

void SM_INVENTORY_ADD_ITEM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

void SM_INVENTORY_ADD_ITEM::writeItemInfo(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
