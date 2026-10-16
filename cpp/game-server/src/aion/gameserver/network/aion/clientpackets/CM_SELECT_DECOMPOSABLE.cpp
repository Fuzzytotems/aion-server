#include "aion/gameserver/network/aion/clientpackets/CM_SELECT_DECOMPOSABLE.h"

#include <cstddef>
#include <optional>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ResultedItem.h"
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

namespace Rnd = commons::utils::Rnd;
using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::templates::item::ResultedItem;
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
			std::optional<std::vector<const ResultedItem*>> selectableItems =
				dataholders::DataManager::DECOMPOSABLE_ITEMS_DATA->getSelectableItems(item->getItemId());
			if (!selectableItems) {
				return;
			}
			std::erase_if(*selectableItems, [&player](const ResultedItem* i) { return !i->isObtainableFor(*player); });
			if (index + 1 > static_cast<int32_t>(selectableItems->size())) {
				return;
			}
			PacketSendUtility::broadcastPacketAndReceive(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), objectId, item->getItemId()));
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_UNCOMPRESS_COMPRESSED_ITEM_SUCCEEDED(item->getL10n()));
			player->getInventory().decreaseByObjectId(objectId, 1);
			PacketSendUtility::sendPacket(*player, SM_SECONDARY_SHOW_DECOMPOSABLE(objectId, {})); // TODO
			const ResultedItem* selectedItem = (*selectableItems)[static_cast<size_t>(index)];
			int32_t count = Rnd::get(selectedItem->getMinCount(), selectedItem->getMaxCount());
			runtime::Ref<ItemService::ItemUpdatePredicate> predicate =
				ItemService::ItemUpdatePredicate::create(ItemPacketService::ItemAddType::DECOMPOSABLE, ItemPacketService::ItemUpdateType::INC_ITEM_COLLECT);
			ItemService::addItem(*player, selectedItem->getItemId(), count, true, *predicate);
		}
	}
}

AION_CLIENT_PACKET(CM_SELECT_DECOMPOSABLE);

} // namespace aion::gameserver::network::aion::clientpackets
