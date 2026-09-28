#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::items::storage::Storage;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::item::ItemPacketService_ItemDeleteType;
using utils::PacketSendUtility;

CM_DELETE_ITEM::CM_DELETE_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_DELETE_ITEM.java:25-28
void CM_DELETE_ITEM::readImpl() {
	itemObjectId = readD();
}

// Java CM_DELETE_ITEM.java:30-44
void CM_DELETE_ITEM::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	Storage& inventory = player->getInventory();
	runtime::Ptr<Item> item = inventory.getItemByObjId(itemObjectId);

	if (item) {
		if (!item->getItemTemplate()->isBreakable()) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_UNBREAKABLE_ITEM(item->getL10n()));
		} else {
			inventory.delete_(*item, ItemPacketService_ItemDeleteType::DISCARD);
		}
	}
}

AION_CLIENT_PACKET(CM_DELETE_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
