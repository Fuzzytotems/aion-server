#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_UPDATE_ITEM.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob_ItemBlobType.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WAREHOUSE_UPDATE_ITEM::SM_WAREHOUSE_UPDATE_ITEM(model::gameobjects::player::Player& playerValue, model::gameobjects::Item& itemValue,
	int32_t warehouseTypeValue, services::item::ItemPacketService_ItemUpdateType updateTypeValue)
	: AionServerPacket(opcodeOf<SM_WAREHOUSE_UPDATE_ITEM>), player(playerValue), item(itemValue), warehouseType(warehouseTypeValue),
	  updateType(updateTypeValue) {
}

SM_WAREHOUSE_UPDATE_ITEM::~SM_WAREHOUSE_UPDATE_ITEM() = default;

void SM_WAREHOUSE_UPDATE_ITEM::writeImpl(AionConnection* con) {
	const model::templates::item::ItemTemplate* itemTemplate = item->getItemTemplate();
	writeD(item->getObjectId());
	writeC(warehouseType);
	writeS(itemTemplate->getL10n());
	runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob = iteminfo::ItemInfoBlob::create(player, *item);
	itemInfoBlob->addBlobEntry(iteminfo::ItemInfoBlob::ItemBlobType::GENERAL_INFO);
	itemInfoBlob->writeMe(getBuf());
	if (detail::itemUpdateTypeData(updateType).sendable)
		writeH(detail::itemUpdateTypeData(updateType).mask);
}

} // namespace aion::gameserver::network::aion::serverpackets
