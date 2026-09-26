#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INVENTORY_ADD_ITEM::SM_INVENTORY_ADD_ITEM(const std::vector<runtime::Ptr<model::gameobjects::Item>>& itemsValue,
	model::gameobjects::player::Player& playerValue, services::item::ItemPacketService_ItemAddType addTypeValue)
	: AionServerPacket(opcodeOf<SM_INVENTORY_ADD_ITEM>), items(itemsValue.begin(), itemsValue.end()), player(playerValue), addType(addTypeValue) {
}

SM_INVENTORY_ADD_ITEM::~SM_INVENTORY_ADD_ITEM() = default;

void SM_INVENTORY_ADD_ITEM::writeImpl(AionConnection* con) {
	// TODO: Rework it, who knows where it could be bugged else.
	int32_t mask = detail::itemAddTypeMask(addType);
	if (addType == services::item::ItemPacketService_ItemAddType::ITEM_COLLECT) {
		// TODO: if size != 1, then it's buy item, should not specify any slot in other places then !!!
		if (items.size() == 1 && items[0]->getEquipmentSlot() != model::items::storage::ItemStorage::FIRST_AVAILABLE_SLOT)
			mask = detail::itemAddTypeMask(services::item::ItemPacketService_ItemAddType::PARTIAL_WITH_SLOT);
	}
	writeH(mask); //
	writeH(static_cast<int32_t>(items.size())); // number of entries
	for (const runtime::Ref<model::gameobjects::Item>& item : items)
		writeItemInfo(*item);
}

void SM_INVENTORY_ADD_ITEM::writeItemInfo(model::gameobjects::Item& item) {
	const model::templates::item::ItemTemplate* itemTemplate = item.getItemTemplate();
	writeD(item.getObjectId());
	writeD(itemTemplate->getTemplateId());
	writeS(itemTemplate->getL10n());
	iteminfo::ItemInfoBlob::getFullBlob(player, item)->writeMe(getBuf());
	writeH(static_cast<int32_t>(item.getEquipmentSlot() & 0xFFFF));
	writeC(item.getItemTemplate()->isCloth() ? 1 : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
