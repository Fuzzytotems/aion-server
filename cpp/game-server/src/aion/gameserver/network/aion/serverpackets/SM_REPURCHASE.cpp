#include "aion/gameserver/network/aion/serverpackets/SM_REPURCHASE.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_REPURCHASE::SM_REPURCHASE(model::gameobjects::player::Player& playerValue, int32_t npcId)
	: AionServerPacket(opcodeOf<SM_REPURCHASE>), player(playerValue), targetObjectId(npcId) {
	for (const runtime::Ptr<model::gameobjects::Item>& item : detail::getRepurchaseItems(playerValue.getObjectId()))
		items.emplace_back(*item);
}

SM_REPURCHASE::~SM_REPURCHASE() = default;

void SM_REPURCHASE::writeImpl(AionConnection* con) {
	writeD(targetObjectId);
	writeD(1);
	writeH(static_cast<int32_t>(items.size()));
	for (const runtime::Ref<model::gameobjects::Item>& item : items) {
		const model::templates::item::ItemTemplate* itemTemplate = item->getItemTemplate();
		writeD(item->getObjectId());
		writeD(itemTemplate->getTemplateId());
		writeS(itemTemplate->getL10n());
		runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob = iteminfo::ItemInfoBlob::getFullBlob(player, *item);
		itemInfoBlob->writeMe(getBuf());
		writeQ(item->getRepurchasePrice());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
