#include "aion/gameserver/network/aion/clientpackets/CM_TUNE.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/TuningAction.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/item/ItemActionService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::templates::item::actions::TuningAction;

CM_TUNE::CM_TUNE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_TUNE.java:25-28
void CM_TUNE::readImpl() {
	itemObjectId = readD();
	tuningScrollObjectId = readD();
}

// Java CM_TUNE.java:31-53
//
// Deviation (play-session fixes 2026-09-28, docs/deviations/P5-16.md; owner decision M5c D7's precedent: fix Java's bug and record it): Java
// starts the identification or the tuning without ending an item use that still runs. addTask(ITEM_USE) then cancels the first use's task
// silently and leaves its one-time ItemUseObserver attached, so the first item stays greyed until a later move or hit aborts that stale observer,
// which prints "Canceled tuning of <first item>" long after the fact and cancels the second use's task. Here the running use is aborted first,
// as CM_CASTSPELL and CM_EQUIP_ITEM do (cancelUseItem): its message and cancel animation come at once, and no stale observer is left.
void CM_TUNE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	const runtime::Ptr<Item> item = player->getInventory().getItemByObjId(itemObjectId);
	if (!item)
		return;
	if (!item->isIdentified()) {
		player->getController().cancelUseItem(); // Deviation: see above
		services::item::ItemActionService::identifyItem(*player, *item);
	} else if (tuningScrollObjectId != 0) {
		const runtime::Ptr<Item> tuningScroll = player->getInventory().getItemByObjId(tuningScrollObjectId);
		if (!tuningScroll)
			return;
		// Java dereferences getActions() unchecked: a scroll without actions is a NullPointerException
		const model::templates::item::actions::ItemActions* actions = tuningScroll->getItemTemplate()->getActions();
		if (actions == nullptr)
			throw runtime::NullPointerException(
				"Cannot invoke \"ItemActions.getTuningAction()\" because the return value of \"ItemTemplate.getActions()\" is null");
		const TuningAction* action = actions->getTuningAction();
		if (action != nullptr && action->canAct(*player, tuningScroll, item)) {
			player->getController().cancelUseItem(); // Deviation: see above; only after canAct, so a refused scroll leaves the running use alone
			action->act(*player, tuningScroll, item);
		}
	} else {
		utils::audit::AuditLogger::log(*player, "attempted to tune an already identified item without tuning scroll.");
	}
}

AION_CLIENT_PACKET(CM_TUNE);

} // namespace aion::gameserver::network::aion::clientpackets
