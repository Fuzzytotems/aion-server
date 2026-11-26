#include "aion/gameserver/network/aion/clientpackets/CM_SELECT_DECOMPOSABLE.h"

#include <cstddef>
#include <optional>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/DecomposableItem.h"
#include "aion/gameserver/model/templates/item/DecomposableSet.h"
#include "aion/gameserver/model/templates/item/DecomposedItem.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SECONDARY_SHOW_DECOMPOSABLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::templates::item::DecomposableItem;
using model::templates::item::DecomposableSet;
using model::templates::item::DecomposedItem;
using serverpackets::SM_ITEM_USAGE_ANIMATION;
using serverpackets::SM_SECONDARY_SHOW_DECOMPOSABLE;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::item::ItemPacketService;
using services::item::ItemService;
using utils::PacketSendUtility;

CM_SELECT_DECOMPOSABLE::CM_SELECT_DECOMPOSABLE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SELECT_DECOMPOSABLE.java:38-42
void CM_SELECT_DECOMPOSABLE::readImpl() {
	objectId = readD();
	unk = readD();
	index = readUC();
}

// Java CM_SELECT_DECOMPOSABLE.java:45-68
void CM_SELECT_DECOMPOSABLE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player) {
		const runtime::Ptr<Item> item = player->getInventory().getItemByObjId(objectId);
		if (item) {
			// C++ interim (docs/deviations/P5-07.md): the selectable items are the first applicable set of the decomposable item of upstream
			// d254ea172 (Java 68eccc7b1 DecomposableItem.getSelectableSet); the rest of this packet stays at 51d7a19cb until wave 5
			const DecomposableItem* decomposableItem = dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->get(item->getItemId());
			if (decomposableItem == nullptr || !decomposableItem->isSelectable()) {
				return;
			}
			const DecomposableSet* set = decomposableItem->getSelectableSet(*player);
			if (set == nullptr || index + 1 > static_cast<int32_t>(set->getItems().size())) {
				return;
			}
			PacketSendUtility::broadcastPacketAndReceive(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), objectId, item->getItemId()));
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED(item->getL10n()));
			player->getInventory().decreaseByObjectId(objectId, 1);
			PacketSendUtility::sendPacket(*player, SM_SECONDARY_SHOW_DECOMPOSABLE(objectId, {})); // TODO
			const DecomposedItem* selectedItem = &set->getItems()[static_cast<size_t>(index)];
			int32_t count = selectedItem->getCount();
			runtime::Ref<ItemService::ItemUpdatePredicate> predicate =
				ItemService::ItemUpdatePredicate::create(ItemPacketService::ItemAddType::DECOMPOSABLE, ItemPacketService::ItemUpdateType::INC_ITEM_COLLECT);
			ItemService::addItem(*player, selectedItem->getItemId(), count, true, *predicate);
		}
	}
}

AION_CLIENT_PACKET(CM_SELECT_DECOMPOSABLE);

} // namespace aion::gameserver::network::aion::clientpackets
