#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_INFO.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_INVENTORY_INFO::SM_INVENTORY_INFO(bool isFirstPacketValue, const std::vector<runtime::Ptr<model::gameobjects::Item>>& itemsValue,
	model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_INVENTORY_INFO>), isFirstPacket(isFirstPacketValue), player(playerValue) {
	// this should prevent client crashes but need to discover when item is null
	// C++: Java removes the nulls from the caller's list (items.removeAll(singletonList(null))); the packet keeps the non-null elements
	for (runtime::Ptr<model::gameobjects::Item> item : itemsValue) {
		if (item)
			items.emplace_back(item);
	}
}

SM_INVENTORY_INFO::~SM_INVENTORY_INFO() = default;

void SM_INVENTORY_INFO::writeImpl(AionConnection* con) {
	// something wrong with cube part.
	writeC(isFirstPacket ? 1 : 0);
	writeC(player->getNpcExpands()); // cube size from npc (so max 5 for now)
	writeC(player->getQuestExpands()); // cube size from quest (so max 2 for now)
	writeC(player->getItemExpands()); // count of ticket expands
	writeH(static_cast<int32_t>(items.size())); // number of entries
	for (const runtime::Ref<model::gameobjects::Item>& item : items)
		writeItemInfo(*item);
}

void SM_INVENTORY_INFO::writeItemInfo(model::gameobjects::Item& item) {
	const model::templates::item::ItemTemplate* itemTemplate = item.getItemTemplate();
	writeD(item.getObjectId());
	writeD(itemTemplate->getTemplateId());
	writeS(itemTemplate->getL10n());
	runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob = iteminfo::ItemInfoBlob::getFullBlob(player, item);
	itemInfoBlob->writeMe(getBuf());
	// invisible -1, visible is a slot
	writeH(static_cast<int32_t>(item.getEquipmentSlot() & 0xFFFF));
	// probably a right to equip the item, related to passive skill learn
	writeC(itemTemplate->isCloth() ? 1 : 0);
}

} // namespace aion::gameserver::network::aion::serverpackets
