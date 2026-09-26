#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_INFO.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_WAREHOUSE_INFO::SM_WAREHOUSE_INFO(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t warehouseTypeValue,
	int32_t expandLvlValue, bool firstPacketValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_WAREHOUSE_INFO>), warehouseType(warehouseTypeValue), itemList(items.begin(), items.end()),
	  firstPacket(firstPacketValue), expandLvl(expandLvlValue), player(playerValue) {
}

SM_WAREHOUSE_INFO::~SM_WAREHOUSE_INFO() = default;

void SM_WAREHOUSE_INFO::writeImpl(AionConnection* con) {
	writeC(warehouseType);
	writeC(firstPacket ? 1 : 0);
	writeC(expandLvl); // warehouse expand (0 - 9)
	if (warehouseType == model::items::storage::getId(model::items::storage::StorageType::REGULAR_WAREHOUSE) && !itemList.empty()) {
		writeC(1);
		writeC(0); // unk, seen value 0x02
	} else {
		writeH(0);
	}
	writeH(static_cast<int32_t>(itemList.size()));
	for (const runtime::Ref<model::gameobjects::Item>& item : itemList)
		writeItemInfo(*item);
}

void SM_WAREHOUSE_INFO::writeItemInfo(model::gameobjects::Item& item) {
	const model::templates::item::ItemTemplate* itemTemplate = item.getItemTemplate();
	writeD(item.getObjectId());
	writeD(itemTemplate->getTemplateId());
	writeC(0); // some item info (4 - weapon, 7 - armor, 8 - rings, 17 - bottles)
	writeS(itemTemplate->getL10n());
	runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob = iteminfo::ItemInfoBlob::getFullBlob(player, item);
	itemInfoBlob->writeMe(getBuf());
	writeH(static_cast<int32_t>(item.getEquipmentSlot() & 0xFFFF));
}

} // namespace aion::gameserver::network::aion::serverpackets
