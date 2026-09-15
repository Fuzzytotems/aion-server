#include "aion/gameserver/network/aion/serverpackets/SM_LOOT_ITEMLIST.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LOOT_ITEMLIST::SM_LOOT_ITEMLIST(model::gameobjects::DropNpc& dropNpc, const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& setItems,
	model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_LOOT_ITEMLIST>) {
	targetObjectId = dropNpc.getObjectId();
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<model::gameobjects::player::Player>>> playersInRange = dropNpc.getInRangePlayers();
	teamMembersNearby = playersInRange->size() > 1 && playersInRange->contains(runtime::Ptr<model::gameobjects::player::Player>(player));
	// Java iterates a Set (the drop registration's HashSet); the C++ set has its own order, so the items keep that order
	for (const runtime::Ptr<model::drop::DropItem>& item : setItems)
		if (item->canViewDropItem(player.getObjectId()))
			dropItems.emplace_back(*item);
}

SM_LOOT_ITEMLIST::~SM_LOOT_ITEMLIST() = default;

void SM_LOOT_ITEMLIST::writeImpl(AionConnection* con) {
	runtime::Ptr<model::gameobjects::player::Player> activePlayer = detail::requireConnection(con, "SM_LOOT_ITEMLIST").getActivePlayer();
	if (activePlayer == nullptr)
		return;
	writeD(targetObjectId);
	writeC(static_cast<int32_t>(dropItems.size()));
	for (const runtime::Ref<model::drop::DropItem>& dropItem : dropItems) {
		const model::drop::Drop* drop = dropItem->getDropTemplate();
		writeC(dropItem->getIndex()); // index in droplist
		writeD(drop->getItemId());
		writeD(static_cast<int32_t>(dropItem->getCount()));
		writeC(dropItem->getOptionalSocket());
		writeC(0);
		writeC(0); // 3.5
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(drop->getItemId());
		if (itemTemplate == nullptr)
			throw runtime::NullPointerException("SM_LOOT_ITEMLIST: no item template for item " + std::to_string(drop->getItemId()));
		bool showLootConfirmation = !itemTemplate->isTradeable();
		if (dropItem->isOnlyPossibleLooter(*activePlayer) || !teamMembersNearby)
			showLootConfirmation = false;
		writeC(showLootConfirmation ? 1 : 0);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
