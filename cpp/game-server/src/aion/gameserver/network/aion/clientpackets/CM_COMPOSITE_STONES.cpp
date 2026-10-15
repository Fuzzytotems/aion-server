#include "aion/gameserver/network/aion/clientpackets/CM_COMPOSITE_STONES.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/actions/CompositionAction.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_COMPOSITE_STONES::CM_COMPOSITE_STONES(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_COMPOSITE_STONES.java:36-41
void CM_COMPOSITE_STONES::readImpl() {
	compinationToolItemObjectId = readD();
	firstItemObjectId = readD();
	secondItemObjectId = readD();
}

// Java CM_COMPOSITE_STONES.java:43-75
void CM_COMPOSITE_STONES::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player == nullptr)
		return;
	if (player->isProtectionActive()) {
		player->getController().stopProtectionActiveTask();
	}
	if (player->isCasting()) {
		player->getController().cancelCurrentSkill(nullptr);
	}
	runtime::Ptr<model::gameobjects::Item> tools = player->getInventory().getItemByObjId(compinationToolItemObjectId);
	if (tools == nullptr)
		return;
	runtime::Ptr<model::gameobjects::Item> first = player->getInventory().getItemByObjId(firstItemObjectId);
	if (first == nullptr)
		return;
	runtime::Ptr<model::gameobjects::Item> second = player->getInventory().getItemByObjId(secondItemObjectId);
	if (second == nullptr)
		return;

	if (!restrictions::PlayerRestrictions::canUseItem(player, *tools))
		return;

	const model::templates::item::actions::CompositionAction action;
	if (!action.canAct(*player, *tools, *first, *second))
		return;
	action.act(*player, *tools, *first, *second);
}

AION_CLIENT_PACKET(CM_COMPOSITE_STONES);

} // namespace aion::gameserver::network::aion::clientpackets
