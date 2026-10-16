#include "aion/gameserver/network/aion/clientpackets/CM_USE_ITEM.h"

#include <algorithm>
#include <any>
#include <memory>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.h"
#include "aion/gameserver/model/templates/item/actions/DyeAction.h"
#include "aion/gameserver/model/templates/item/actions/InstanceTimeClear.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/MultiReturnAction.h"
#include "aion/gameserver/model/templates/item/actions/QuestStartAction.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::HouseObject;
using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::templates::item::actions::AbstractItemAction;
using model::templates::item::actions::DyeAction;
using model::templates::item::actions::InstanceTimeClear;
using model::templates::item::actions::MultiReturnAction;
using model::templates::item::actions::QuestStartAction;
using questEngine::handlers::HandlerResult;
using serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

CM_USE_ITEM::CM_USE_ITEM(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_USE_ITEM.java:37-52
void CM_USE_ITEM::readImpl() {
	uniqueItemId = readD();
	int8_t type = readC();
	switch (type) {
		case 2:
			targetItemId = readD();
			break;
		case 5: // instance cooltime reset scroll
			syncId = readD();
			break;
		case 6:
			indexReturn = readD();
			break;
	}
}

// Java CM_USE_ITEM.java:54-125
void CM_USE_ITEM::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	if (player->isProtectionActive())
		player->getController().stopProtectionActiveTask();

	runtime::Ptr<Item> item = player->getInventory().getItemByObjId(uniqueItemId);
	if (!item)
		return;

	runtime::Ptr<Item> targetItem;
	runtime::Ptr<HouseObject> targetHouseObject;
	if (targetItemId != 0) {
		targetItem = player->getInventory().getItemByObjId(targetItemId);
		if (!targetItem)
			targetItem = player->getEquipment().getEquippedItemByObjId(targetItemId);
		if (!targetItem && player->getActiveHouse())
			targetHouseObject = player->getActiveHouse()->getRegistry()->getObjectByObjId(targetItemId);
	}

	// check use item multicast delay exploit cast (spam)
	if (player->isCasting())
		player->getController().cancelCurrentSkill(nullptr);

	// notify item use observer
	player->getObserveController()->notifyItemuseObservers(*item);

	if (!restrictions::PlayerRestrictions::canUseItem(player, *item))
		return;

	// Java: getActions() == null ? Collections.emptyList() : getActions().getItemActions(); the actions are static data, borrowed by pointer
	std::vector<const AbstractItemAction*> itemActions;
	if (const model::templates::item::actions::ItemActions* actions = item->getItemTemplate()->getActions()) {
		for (const std::unique_ptr<AbstractItemAction>& action : actions->getItemActions())
			itemActions.push_back(action.get());
	}

	// QuestStartAction opens the quest dialog itself (after the casting delay), quest handlers must not open it a second time
	HandlerResult result =
		std::ranges::any_of(itemActions, [](const AbstractItemAction* a) { return dynamic_cast<const QuestStartAction*>(a) != nullptr; })
		? HandlerResult::UNKNOWN
		: questEngine::QuestEngine::getInstance().onItemUseEvent(*questEngine::model::QuestEnv::create(nullptr, *player, 0), *item);

	if (itemActions.empty() && result != HandlerResult::SUCCESS) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_IS_NOT_USABLE());
		return;
	}

	// Java passes one vararg: a DyeAction reads params[0] as the HouseObject (a Ref, null for Java null), MultiReturnAction and
	// InstanceTimeClear as an Integer (AbstractItemAction.h)
	const runtime::Ref<HouseObject> houseObjectParam(targetHouseObject);
	std::vector<const AbstractItemAction*> actions;
	for (const AbstractItemAction* itemAction : itemActions) {
		// check if the item can be used before placing it on the cooldown list.
		if (dynamic_cast<const DyeAction*>(itemAction)) {
			if (itemAction->canAct(*player, item, targetItem, {std::any(houseObjectParam)}))
				actions.push_back(itemAction);
		} else if (dynamic_cast<const MultiReturnAction*>(itemAction)) {
			if (itemAction->canAct(*player, item, targetItem, {std::any(indexReturn)}))
				actions.push_back(itemAction);
		} else if (dynamic_cast<const InstanceTimeClear*>(itemAction)) {
			if (itemAction->canAct(*player, item, targetItem, {std::any(syncId)}))
				actions.push_back(itemAction);
		} else if (itemAction->canAct(*player, item, targetItem)) {
			actions.push_back(itemAction);
		}
	}

	if (actions.empty())
		return; // notification should be handled in canAct

	for (const AbstractItemAction* itemAction : actions) {
		if (dynamic_cast<const DyeAction*>(itemAction))
			itemAction->act(*player, item, targetItem, {std::any(houseObjectParam)});
		else if (dynamic_cast<const MultiReturnAction*>(itemAction))
			itemAction->act(*player, item, targetItem, {std::any(indexReturn)});
		else if (dynamic_cast<const InstanceTimeClear*>(itemAction))
			itemAction->act(*player, item, targetItem, {std::any(syncId)});
		else
			itemAction->act(*player, item, targetItem);
	}
}

AION_CLIENT_PACKET(CM_USE_ITEM);

} // namespace aion::gameserver::network::aion::clientpackets
