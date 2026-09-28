#include "aion/gameserver/network/aion/serverpackets/SM_EXCHANGE_ADD_ITEM.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_EXCHANGE_ADD_ITEM::SM_EXCHANGE_ADD_ITEM(int32_t actionValue, model::gameobjects::Item& itemValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_EXCHANGE_ADD_ITEM>), player(playerValue), action(actionValue), item(itemValue) {
}

SM_EXCHANGE_ADD_ITEM::~SM_EXCHANGE_ADD_ITEM() = default;

void SM_EXCHANGE_ADD_ITEM::writeImpl(AionConnection* con) {
	const model::templates::item::ItemTemplate* itemTemplate = item->getItemTemplate();
	writeC(action); // 0 -self 1-other
	writeD(itemTemplate->getTemplateId());
	writeD(item->getObjectId());
	writeS(itemTemplate->getL10n());
	runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob = iteminfo::ItemInfoBlob::getFullBlob(player, *item);
	itemInfoBlob->writeMe(getBuf());
}

} // namespace aion::gameserver::network::aion::serverpackets
