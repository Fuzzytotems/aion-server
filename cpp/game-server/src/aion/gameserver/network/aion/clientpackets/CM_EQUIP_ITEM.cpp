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

namespace {

/** C++ only (the deviation of runImpl): unEquipItem's own test (Equipment.java:235-237), i.e. the object id names an item that is equipped */
bool isStillEquipped(Equipment& equipment, int32_t itemObjId) {
	const runtime::Ptr<Item> item = equipment.getEquippedItemByObjId(itemObjId);
	return item && item->isEquipped();
}

} // namespace

// Java CM_EQUIP_ITEM.java:35-63
//
// Deviation (report 3 of the 2026-09-28 play session, fixed after the client packet trace of 2026-09-29; docs/deviations/P5-15.md; owner
// decision M5c D7's precedent: fix Java's bug and record it): Java answers every null of unEquipItem with STR_UI_INVENTORY_FULL
// (CM_EQUIP_ITEM.java:50-53). unEquipItem also answers null for an object id that is not equipped (Equipment.java:235-237), and the client's
// weapon swap sends one: it unequips both weapons, then equips them back crosswise, and when the main-hand weapon goes first, Java's "retail
// like" rule has already moved the off-hand weapon to the cube too (Equipment.java:239-247), so the second unequip names an item that is no
// longer equipped. The swap completes, with a spurious "inventory is full". Here the message comes only when the item is still equipped, i.e.
// when the cube was the reason (a full cube, :231-232, or fewer than 2 free slots for a main-hand weapon with an off-hand weapon, :240-245); a
// stale unequip fails silently.
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
			if (!resultItem && isStillEquipped(equipment, itemObjId)) // Deviation: see above
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

// C++ only (play-session fixes 2026-09-28, docs/deviations/P5-15.md): the form of Java's CM_MOVE.toString (CM_MOVE.java:204-208)
std::string CM_EQUIP_ITEM::toString() const {
	return "CM_EQUIP_ITEM [action=" + std::to_string(action) + ", slot=" + std::to_string(slotRead) + ", itemObjId=" + std::to_string(itemObjId) +
		"]";
}

AION_CLIENT_PACKET(CM_EQUIP_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
