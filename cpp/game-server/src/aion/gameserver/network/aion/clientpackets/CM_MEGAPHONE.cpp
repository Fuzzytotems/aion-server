#include "aion/gameserver/network/aion/clientpackets/CM_MEGAPHONE.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/MegaphoneAction.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_MEGAPHONE::CM_MEGAPHONE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_MEGAPHONE.java:30-34
void CM_MEGAPHONE::readImpl() {
	message = readS();
	itemObjId = readD();
}

// Java CM_MEGAPHONE.java:36-60
void CM_MEGAPHONE::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	runtime::Ptr<model::gameobjects::Item> item = player->getInventory().getItemByObjId(itemObjId);
	if (item == nullptr)
		return;

	if (!restrictions::PlayerRestrictions::canUseItem(player, *item))
		return;

	// Java: getActions().getItemActions().stream().filter(a -> a instanceof MegaphoneAction).findAny().orElse(null); getActions() of an item
	// without actions is Java's NullPointerException
	const model::templates::item::actions::ItemActions* actions = item->getItemTemplate()->getActions();
	if (actions == nullptr)
		throw runtime::NullPointerException("ItemTemplate.getActions()");
	const model::templates::item::actions::MegaphoneAction* megaphoneAction = nullptr;
	for (const std::unique_ptr<model::templates::item::actions::AbstractItemAction>& a : actions->getItemActions()) {
		if (const auto* m = dynamic_cast<const model::templates::item::actions::MegaphoneAction*>(a.get())) {
			megaphoneAction = m;
			break;
		}
	}

	if (megaphoneAction == nullptr) {
		utils::PacketSendUtility::sendPacket(*player, serverpackets::SM_SYSTEM_MESSAGE::STR_ITEM_IS_NOT_USABLE());
		return;
	}

	if (megaphoneAction->canAct(*player, item, nullptr, {std::any(message)})) {
		const model::templates::item::ItemUseLimits* useLimits = item->getItemTemplate()->getUseLimits();
		if (useLimits == nullptr) // Java: getUseLimits().getDelayTime() on null
			throw runtime::NullPointerException("ItemTemplate.getUseLimits()");
		int32_t useDelay = useLimits->getDelayTime();
		if (useDelay > 0)
			player->addItemCoolDown(useLimits->getDelayId(), commons::utils::currentTimeMillis() + useDelay, useDelay / 1000);
		player->getObserveController()->notifyItemuseObservers(*item);
		megaphoneAction->act(*player, item, nullptr, {std::any(message)});
	}
}

AION_CLIENT_PACKET(CM_MEGAPHONE);

} // namespace aion::gameserver::network::aion::clientpackets
