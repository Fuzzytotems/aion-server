#include "aion/gameserver/network/aion/serverpackets/SM_VIEW_PLAYER_DETAILS.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_VIEW_PLAYER_DETAILS::SM_VIEW_PLAYER_DETAILS(const std::vector<runtime::Ptr<model::gameobjects::Item>>& itemsValue,
	model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_VIEW_PLAYER_DETAILS>), items(itemsValue.begin(), itemsValue.end()), player(playerValue) {
	targetObjId = playerValue.getObjectId();
	itemSize = static_cast<int32_t>(itemsValue.size());
}

SM_VIEW_PLAYER_DETAILS::~SM_VIEW_PLAYER_DETAILS() = default;

void SM_VIEW_PLAYER_DETAILS::writeImpl(AionConnection* con) {
	writeD(targetObjId);
	writeC(11);
	writeH(itemSize);
	for (const runtime::Ref<model::gameobjects::Item>& item : items)
		writeItemInfo(*item);
}

void SM_VIEW_PLAYER_DETAILS::writeItemInfo(model::gameobjects::Item& item) {
	const model::templates::item::ItemTemplate* itemTemplate = item.getItemTemplate();
	writeD(0);
	writeD(itemTemplate->getTemplateId());
	writeS(itemTemplate->getL10n());
	runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob = iteminfo::ItemInfoBlob::getFullBlob(player, item);
	itemInfoBlob->writeMe(getBuf());
}

} // namespace aion::gameserver::network::aion::serverpackets
