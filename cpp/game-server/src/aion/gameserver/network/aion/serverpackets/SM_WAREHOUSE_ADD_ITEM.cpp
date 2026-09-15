#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_ADD_ITEM.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WAREHOUSE_ADD_ITEM::SM_WAREHOUSE_ADD_ITEM(model::gameobjects::Item& item, int32_t warehouseTypeValue,
	model::gameobjects::player::Player& playerValue, services::item::ItemPacketService_ItemAddType addTypeValue)
	: AionServerPacket(opcodeOf<SM_WAREHOUSE_ADD_ITEM>), warehouseType(warehouseTypeValue), player(playerValue), addType(addTypeValue) {
	items.emplace_back(item); // Java: Collections.singletonList(item)
}

SM_WAREHOUSE_ADD_ITEM::~SM_WAREHOUSE_ADD_ITEM() = default;

void SM_WAREHOUSE_ADD_ITEM::writeImpl(AionConnection* con) {
	writeC(warehouseType);
	writeH(detail::itemAddTypeMask(addType));
	writeH(static_cast<int32_t>(items.size()));
	for (const runtime::Ref<model::gameobjects::Item>& item : items)
		writeItemInfo(*item);
}

void SM_WAREHOUSE_ADD_ITEM::writeItemInfo(model::gameobjects::Item& item) {
	const model::templates::item::ItemTemplate* itemTemplate = item.getItemTemplate();
	writeD(item.getObjectId());
	writeD(itemTemplate->getTemplateId());
	writeC(0); // some item info (4 - weapon, 7 - armor, 8 - rings, 17 - bottles)
	writeS(itemTemplate->getL10n());
	iteminfo::ItemInfoBlob::getFullBlob(player, item)->writeMe(getBuf());
	writeH(static_cast<int32_t>(item.getEquipmentSlot() & 0xFFFF));
}

} // namespace aion::gameserver::network::aion::serverpackets
