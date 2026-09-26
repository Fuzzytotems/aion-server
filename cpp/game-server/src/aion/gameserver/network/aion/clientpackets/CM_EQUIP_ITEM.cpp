#include "aion/gameserver/network/aion/clientpackets/CM_EQUIP_ITEM.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_PLAYER_APPEARANCE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Item;
using model::gameobjects::player::Equipment;
using model::gameobjects::player::Player;
using restrictions::PlayerRestrictions;
using serverpackets::SM_SYSTEM_MESSAGE;
using serverpackets::SM_UPDATE_PLAYER_APPEARANCE;
using utils::PacketSendUtility;

CM_EQUIP_ITEM::CM_EQUIP_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_EQUIP_ITEM.java:28-33
void CM_EQUIP_ITEM::readImpl() {
	action = readC(); // 0/1/2 = equip/unequip/switch weapons
	slotRead = readQ();
	itemObjId = readD();
}

// Java CM_EQUIP_ITEM.java:35-63
void CM_EQUIP_ITEM::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();

	activePlayer->getController().cancelUseItem();

	if (!PlayerRestrictions::canChangeEquip(*activePlayer))
		return;

	Equipment& equipment = activePlayer->getEquipment();
	runtime::Ptr<Item> resultItem;
	switch (action) {
		case 0:
			resultItem = equipment.equipItem(itemObjId, slotRead);
			break;
		case 1:
			resultItem = equipment.unEquipItem(itemObjId);
			if (!resultItem)
				PacketSendUtility::sendPacket(*activePlayer, SM_SYSTEM_MESSAGE::STR_UI_INVENTORY_FULL());
			break;
		case 2:
			equipment.switchHands();
			break;
	}

	if (resultItem || action == 2)
		PacketSendUtility::broadcastPacket(*activePlayer,
			SM_UPDATE_PLAYER_APPEARANCE(activePlayer->getObjectId(), equipment.getEquippedForAppearance()), true);
}

AION_CLIENT_PACKET(CM_EQUIP_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
