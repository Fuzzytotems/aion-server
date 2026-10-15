#include "aion/gameserver/network/aion/clientpackets/CM_MANASTONE.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/EnchantService.h"
#include "aion/gameserver/services/StigmaService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/utils/PositionUtil.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Item;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::templates::item::actions::EnchantItemAction;
using serverpackets::SM_SYSTEM_MESSAGE;

namespace {

/**
 * Java `new EnchantItemAction()` (CM_MANASTONE.java:79): a default-constructed action template, without the attributes the item data gives
 * the bound ones. It holds nothing mutable, so one immortal instance stands for Java's fresh object: a task or observer its act may create
 * (M5c) can keep the address the way it keeps a bound template's, which a stack object would not allow.
 */
const EnchantItemAction& defaultEnchantItemAction() {
	static const EnchantItemAction action{};
	return action;
}

} // namespace

CM_MANASTONE::CM_MANASTONE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_MANASTONE.java:38-58
void CM_MANASTONE::readImpl() {
	actionType = readUC();
	targetFusedSlot = readUC();
	targetItemUniqueId = readD();
	switch (actionType) {
		case 1:
		case 2:
		case 4:
		case 8:
			stoneUniqueId = readD();
			supplementUniqueId = readD();
			break;
		case 3:
			slotNum = readUC();
			readC();
			readH();
			npcObjId = readD();
			break;
	}
}

// Java CM_MANASTONE.java:60-108
void CM_MANASTONE::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	switch (actionType) {
		case 1:   // enchant stone
		case 2: { // add manastone
			runtime::Ptr<Item> stone = player->getInventory().getItemByObjId(stoneUniqueId);
			if (!stone)
				return;
			runtime::Ptr<Item> targetItem = player->getEquipment().getEquippedItemByObjId(targetItemUniqueId);
			if (!targetItem && !(targetItem = player->getInventory().getItemByObjId(targetItemUniqueId))) {
				sendPacket(actionType == 1 ? SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_NO_TARGET_ITEM() : SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_NO_TARGET_ITEM());
				return;
			}

			if (stone->getItemTemplate()->isStigma() && targetItem->getItemTemplate()->isStigma()) {
				services::StigmaService::chargeStigma(*player, *targetItem, *stone);
			} else {
				const EnchantItemAction& action = defaultEnchantItemAction();
				if (action.canAct(*player, stone, targetItem)) {
					runtime::Ptr<Item> supplement = player->getInventory().getItemByObjId(supplementUniqueId);
					if (supplement) {
						if (supplement->getItemId() / 100000 != 1661) // suppliment id check
							return;
					}
					action.act(*player, *stone, *targetItem, supplement, targetFusedSlot);
				}
			}
			break;
		}
		case 3: { // remove manastone
			runtime::Ptr<VisibleObject> visibleObject = player->getTarget();
			// Java `visibleObject instanceof Npc npc`, false for null; getObjectId() of a null target is Java's NullPointerException
			if (visibleObject->getObjectId() == npcObjId) {
				if (runtime::Ptr<Npc> npc = runtime::as<Npc>(visibleObject); npc && utils::PositionUtil::isInTalkRange(*player, *npc))
					services::item::ItemSocketService::removeManastone(*player, targetItemUniqueId, slotNum, targetFusedSlot != 1);
			}
			break;
		}
		case 4: { // add godstone
			runtime::Ptr<Item> weaponItem = player->getInventory().getItemByObjId(targetItemUniqueId);
			if (!weaponItem) {
				bool isEquipped = player->getEquipment().getEquippedItemByObjId(targetItemUniqueId) != nullptr;
				sendPacket(isEquipped ? SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_CANNOT_GIVE_PROC_TO_EQUIPPED_ITEM()
									  : SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_TARGET_ITEM());
				return;
			}
			services::item::ItemSocketService::socketGodstone(*player, weaponItem, stoneUniqueId);
			break;
		}
		case 8: // amplification
			services::EnchantService::amplifyItem(player, targetItemUniqueId, supplementUniqueId, stoneUniqueId);
			break;
	}
}

AION_CLIENT_PACKET(CM_MANASTONE);

} // namespace aion::gameserver::network::aion::clientpackets
