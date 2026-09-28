#include "aion/gameserver/network/aion/clientpackets/CM_TUNE.h"

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
void CM_TUNE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	const runtime::Ptr<Item> item = player->getInventory().getItemByObjId(itemObjectId);
	if (!item)
		return;
	if (!item->isIdentified()) {
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
		if (action != nullptr && action->canAct(*player, tuningScroll, item))
			action->act(*player, tuningScroll, item);
	} else {
		utils::audit::AuditLogger::log(*player, "attempted to tune an already identified item without tuning scroll.");
	}
}

AION_CLIENT_PACKET(CM_TUNE);

} // namespace aion::gameserver::network::aion::clientpackets
