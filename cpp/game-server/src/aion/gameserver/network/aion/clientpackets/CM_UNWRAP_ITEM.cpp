#include "aion/gameserver/network/aion/clientpackets/CM_UNWRAP_ITEM.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UNWRAP_ITEM.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using utils::PacketSendUtility;

CM_UNWRAP_ITEM::CM_UNWRAP_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_UNWRAP_ITEM.java:26-28
void CM_UNWRAP_ITEM::readImpl() {
	objectId = readD();
}

// Java CM_UNWRAP_ITEM.java:31-45
void CM_UNWRAP_ITEM::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (player == nullptr)
		return;
	const runtime::Ptr<Item> item = player->getInventory().getItemByObjId(objectId);
	if (item != nullptr) {
		if (item->getPackCount() > 0) {
			sendPacket(serverpackets::SM_UNWRAP_ITEM(objectId, item->getPackCount()));
			item->setPackCount(item->getPackCount() * -1);
			item->setPersistentState(Item::PersistentState::UPDATE_REQUIRED);
			PacketSendUtility::sendPacket(*player, serverpackets::SM_INVENTORY_UPDATE_ITEM(*player, *item));
		}
	}
}

AION_CLIENT_PACKET(CM_UNWRAP_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
