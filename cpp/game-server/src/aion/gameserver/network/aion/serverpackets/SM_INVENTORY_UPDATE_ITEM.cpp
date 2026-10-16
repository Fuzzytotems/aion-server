#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

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
	using services::item::ItemPacketService_ItemUpdateType;
	using ItemBlobType = iteminfo::ItemInfoBlob::ItemBlobType;
	const model::templates::item::ItemTemplate* itemTemplate = item->getItemTemplate();
	writeD(item->getObjectId());
	writeS(itemTemplate->getL10n());
	runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob;
	switch (updateType) {
		case ItemPacketService_ItemUpdateType::EQUIP_UNEQUIP:
			itemInfoBlob = iteminfo::ItemInfoBlob::create(player, *item);
			itemInfoBlob->addBlobEntry(ItemBlobType::EQUIPPED_SLOT);
			break;
		case ItemPacketService_ItemUpdateType::CHARGE:
			itemInfoBlob = iteminfo::ItemInfoBlob::create(player, *item);
			itemInfoBlob->addBlobEntry(ItemBlobType::CONDITIONING_INFO);
			break;
		case ItemPacketService_ItemUpdateType::POLISH_CHARGE:
			itemInfoBlob = iteminfo::ItemInfoBlob::create(player, *item);
			itemInfoBlob->addBlobEntry(ItemBlobType::POLISH_INFO);
			break;
		default:
			itemInfoBlob = iteminfo::ItemInfoBlob::getFullBlob(player, *item);
			break;
	}
	itemInfoBlob->writeMe(getBuf());
	if (detail::itemUpdateTypeData(updateType).sendable)
		writeH(detail::itemUpdateTypeData(updateType).mask);
}

} // namespace aion::gameserver::network::aion::serverpackets
